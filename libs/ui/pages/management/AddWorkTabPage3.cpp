#include "ui/pages/management/AddWorkTabPage3.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/CompleterLineEdit.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "ui/components/PersonTransferSelector.h"
#include "ui/components/WorkTagSelector.h"
#include "ui/dialogs/AddActorDialog.h"
#include "ui/dialogs/AddActressDialog.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenControls.h"
#include "ui/components/CrawlerFieldSelector.h"
#include "ui/components/FanartStripWidget.h"
#include "ui/components/ImageDropWidget.h"
#include "ui/components/AsyncImageLabel.h"
#include "ui/components/WikiTextEdit.h"
#include "ui/layouts/myads/ThemeAdapter.h"
#include "settings/Paths.h"
#include "settings/Settings.h"
#include "utils/MediaUtils.h"
#include "domain/SerialNumber.h"
#include "database/SqliteConnection.h"
#include "graph/GraphManager.h"
#include "ui/pages/ForceDirectPage.h"
#include "crawler/CrawlerScheduler.h"
#include "services/ImageFetchService.h"
#include "services/LlmTranslationService.h"

#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QStandardPaths>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QUrl>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace darkeye
{
namespace
{
constexpr int kFormControlMinimumWidth = 160;
constexpr int kFormControlMinimumHeight = 32;
constexpr int kBasicPaneMinimumWidth = 360;
constexpr int kBasicPaneMinimumHeight = 360;
constexpr int kWorkspaceAuxiliaryMinimumHeight = 200;

// Python stores an unknown runtime as 0, while the C++ repository also has to
// handle older rows where the column is NULL.  Both values render as "未知" in
// the editor and must therefore be the same value from the view-model's
// change-tracking perspective.
bool sameRuntime(const std::optional<int> &first, const std::optional<int> &second)
{
    return first == second
        || (!first.has_value() && second.has_value() && *second == 0)
        || (!second.has_value() && first.has_value() && *first == 0);
}

template <typename Callback>
auto withShortDatabase(const QString &databasePath, bool readOnly, QString *errorMessage,
                       Callback &&callback) -> decltype(callback(QSqlDatabase{}))
{
    using Result = decltype(callback(QSqlDatabase{}));
    SqliteConnection connection;
    if (!connection.open(databasePath, readOnly, errorMessage))
        return Result{};
    return callback(connection.database());
}
} // namespace

AddWorkTabPage3::AddWorkTabPage3(QSqlDatabase database, ThemeService &themes,
                                   CrawlerScheduler &crawlerScheduler,
                                   QString coverDirectory, QString fanartDirectory,
                                   QUrl imageFetchEndpoint, QWidget *parent)
    : QWidget(parent), m_database(database), m_repository(database), m_references(database),
      m_people(std::move(database)), m_themes(themes), m_coverDirectory(std::move(coverDirectory)),
      m_fanartDirectory(std::move(fanartDirectory)),
      m_imageFetchEndpoint(std::move(imageFetchEndpoint)),
      m_crawlerScheduler(crawlerScheduler)
{
    if (m_fanartDirectory.isEmpty() && !m_coverDirectory.isEmpty())
        m_fanartDirectory =
            QDir(QFileInfo(m_coverDirectory).absolutePath()).filePath(QStringLiteral("fanart"));
    if (m_workspaceLayoutPath.isEmpty())
        m_workspaceLayoutPath = settings::Paths().addWorkWorkspaceLayoutFile();
    m_imageDrop = new ImageDropWidget(m_coverDirectory, this);
    m_imageDrop->setObjectName(QStringLiteral("WorkCoverDropWidget"));
    m_imageDrop->setPurpose(QStringLiteral("作品封面"), QStringLiteral("把JAV封面拖进来"));
    // Python 的 CoverDropWidget 固定 0.7 宽高比并展示原图右侧（JAV 封面）。
    m_imageDrop->setPreviewAspectRatio(0.7);
    m_imageDrop->setFitMode(ImageFitMode::RightCover);
    m_imageDrop->setQualityBadgeEnabled(true);
    m_crawlerImageFetch = new ImageFetchService(m_imageFetchEndpoint, this);
    connect(m_crawlerImageFetch, &ImageFetchService::requestFinished, this,
            [this](quint64, bool succeeded, const QString &destinationPath,
                   const QString &errorMessage)
            {
                if (m_crawlSerial.isEmpty())
                    return;
                if (succeeded)
                {
                    m_imageDrop->setImagePath(destinationPath);
                    m_imageDrop->setDirty(true);
                    m_imageUrl->setText(destinationPath);
                    const QString serial = m_crawlSerial;
                    m_crawlSerial.clear();
                    m_crawlCoverUrls.clear();
                    m_crawlerScheduler.complete(serial, true);
                    Toast::showSuccess(window(), QStringLiteral("采集与封面下载完成"), &m_themes);
                    return;
                }
                ++m_crawlCoverIndex;
                if (m_crawlCoverIndex < m_crawlCoverUrls.size())
                {
                    fetchNextCrawledCover();
                    return;
                }
                const QString serial = m_crawlSerial;
                m_crawlSerial.clear();
                m_crawlCoverUrls.clear();
                m_crawlerScheduler.complete(serial, false);
                Toast::showWarning(window(), QStringLiteral("信息已采集，但封面下载失败：%1")
                                       .arg(errorMessage), &m_themes);
            });

    auto *form = new QFormLayout;
    m_serialNumber = new CompleterLineEdit(
        [this]
        {
            QString errorMessage;
            return withShortDatabase(m_database.databaseName(), true, &errorMessage,
                                     [](QSqlDatabase connection)
                                     { return WorkRepository(connection).serialSuggestions(); });
        }, this);
    m_serialNumber->setProperty("workControlId", QStringLiteral("WorkSerialInput"));
    m_chineseTitle = new DesignPlainTextEdit(this);
    m_chineseTitle->setProperty("workControlId", QStringLiteral("WorkChineseTitleInput"));
    m_japaneseTitle = new DesignPlainTextEdit(this);
    m_japaneseTitle->setProperty("workControlId", QStringLiteral("WorkJapaneseTitleInput"));
    m_director = new CompleterLineEdit(
        [this]
        {
            QString errorMessage;
            return withShortDatabase(m_database.databaseName(), true, &errorMessage,
                                     [](QSqlDatabase connection)
                                     { return WorkRepository(connection).directorSuggestions(); });
        }, this);
    m_releaseDate = new DesignLineEdit(this);
    m_releaseDate->setPlaceholderText(QStringLiteral("YYYY-MM-DD"));
    m_runtime = new TokenSpinBox(this);
    // Keep the same upper bound as Python's AddWorkTabPage3.  Imported works
    // occasionally contain long-running compilation or bonus-disc durations.
    m_runtime->setRange(0, 9999);
    m_runtime->setSpecialValueText(QStringLiteral("未知"));
    m_runtime->setSuffix(QStringLiteral(" 分钟"));
    m_imageUrl = new DesignLineEdit(this);
    m_imageUrl->setProperty("workControlId", QStringLiteral("WorkImageUrlInput"));
    m_videoUrl = new DesignLineEdit(this);
    m_videoUrl->setProperty("workControlId", QStringLiteral("WorkVideoUrlInput"));
    m_maker = new DesignComboBox(this);
    m_label = new DesignComboBox(this);
    m_series = new DesignComboBox(this);
    // The Python page keeps a usable minimum field width while its workspace
    // panes are resized.  Let long reference names elide instead of using
    // their contents to enlarge the pane, but do not allow the fields to
    // collapse to a few pixels.
    for (QComboBox *combo : {m_maker, m_label, m_series})
    {
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(0);
        combo->setMinimumSize(kFormControlMinimumWidth, kFormControlMinimumHeight);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    m_notes = new WikiTextEdit(this);
    m_notes->setProperty("workControlId", QStringLiteral("WorkNotesInput"));
    QString completionError;
    m_notes->setCompleterList(
        withShortDatabase(m_database.databaseName(), true, &completionError,
                          [](QSqlDatabase connection)
                          { return WorkRepository(connection).serialSuggestions(); }));
    m_notes->setWorkIdResolver([this](const QString &serial) {
        QString errorMessage;
        return withShortDatabase(m_database.databaseName(), true, &errorMessage,
                                 [&serial](QSqlDatabase connection)
                                 { return WorkRepository(connection).findIdBySerial(serial); });
    });
    m_notes->setImageResolver([this](const QString &serial) {
        QString errorMessage;
        const auto id = withShortDatabase(
            m_database.databaseName(), true, &errorMessage,
            [&serial](QSqlDatabase connection)
            { return WorkRepository(connection).findIdBySerial(serial); });
        if (!id) return QString();
        const auto work = withShortDatabase(
            m_database.databaseName(), true, &errorMessage,
            [id](QSqlDatabase connection) { return WorkRepository(connection).findById(*id); });
        if (!work || work->imageUrl.trimmed().isEmpty()) return QString();
        return QFileInfo(work->imageUrl).isAbsolute()
            ? work->imageUrl : QDir(m_coverDirectory).filePath(work->imageUrl);
    });
    connect(m_notes, &WikiTextEdit::workLinkRequested,
            this, &AddWorkTabPage3::workLinkRequested);
    connect(m_notes, &WikiTextEdit::actressLinkRequested,
            this, &AddWorkTabPage3::actressLinkRequested);
    m_chineseStory = new DesignPlainTextEdit(this);
    m_japaneseStory = new DesignPlainTextEdit(this);
    const auto configureFormField = [](QWidget *widget) {
        widget->setMinimumSize(kFormControlMinimumWidth, kFormControlMinimumHeight);
        widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    };
    for (QWidget *field : {static_cast<QWidget *>(m_serialNumber),
                           static_cast<QWidget *>(m_director),
                           static_cast<QWidget *>(m_releaseDate),
                           static_cast<QWidget *>(m_runtime),
                           static_cast<QWidget *>(m_imageUrl),
                           static_cast<QWidget *>(m_videoUrl)})
        configureFormField(field);

    auto *serialRow = new QWidget(this);
    serialRow->setMinimumHeight(kFormControlMinimumHeight);
    serialRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *serialLayout = new QHBoxLayout(serialRow);
    serialLayout->setContentsMargins(0, 0, 0, 0);
    serialLayout->addWidget(m_serialNumber, 1);
    m_loadButton = new DesignButton(QStringLiteral("加载"), serialRow);
    m_loadButton->setObjectName(QStringLiteral("WorkLoadButton"));
    auto *loadButton = m_loadButton;
    loadButton->setMinimumSize(56, kFormControlMinimumHeight);
    m_detailButton = new IconButton(QStringLiteral("eye"), &m_themes, serialRow);
    auto *detailButton = m_detailButton;
    detailButton->setToolTip(QStringLiteral("查看作品详情"));
    detailButton->setIconPixelSize(16);
    detailButton->setButtonPixelSize(28);
    serialLayout->addWidget(loadButton);
    serialLayout->addWidget(detailButton);
    const auto addFormRow = [form, this](const QString &label, QWidget *field) {
        auto *formLabel = new DesignLabel(label, this);
        formLabel->setFormLabelColumn();
        form->addRow(formLabel, field);
    };
    // Keep the content and order identical to Python's basic_info_container.
    // Cover/video paths remain backing fields for the cover and local-video
    // features, but are deliberately not rows in this pane.
    addFormRow(QStringLiteral("番号："), serialRow);
    addFormRow(QStringLiteral("发布日期："), m_releaseDate);
    addFormRow(QStringLiteral("导演："), m_director);
    addFormRow(QStringLiteral("影片长度："), m_runtime);
    addFormRow(QStringLiteral("片商："), m_maker);
    addFormRow(QStringLiteral("厂牌："), m_label);
    addFormRow(QStringLiteral("系列："), m_series);
    auto *localVideoRow = new QWidget(this);
    auto *localVideoLayout = new QHBoxLayout(localVideoRow);
    localVideoLayout->setContentsMargins(0, 0, 0, 0);
    m_playButton = new IconButton(QStringLiteral("tv"), &m_themes, localVideoRow);
    auto *playButton = m_playButton;
    playButton->setToolTip(QStringLiteral("播放已保存的本地视频"));
    localVideoLayout->addWidget(playButton);
    localVideoLayout->addStretch();
    playButton->setIconPixelSize(16);
    playButton->setButtonPixelSize(28);
    localVideoRow->setMinimumHeight(kFormControlMinimumHeight);
    localVideoRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    addFormRow(QStringLiteral("本地视频："), localVideoRow);
    m_actresses = new PersonTransferSelector(QStringLiteral("女优"), this);
    m_actresses->setObjectName(QStringLiteral("WorkActressSelector"));
    m_actors = new PersonTransferSelector(QStringLiteral("男优"), this);
    m_actors->setObjectName(QStringLiteral("WorkActorSelector"));
    QString tagError;
    const QList<TagOption> tagOptions = withShortDatabase(
        m_database.databaseName(), true, &tagError,
        [](QSqlDatabase connection) { return WorkRepository(connection).tagOptions(); });
    m_tags = new WorkTagSelector(tagOptions, &m_themes, this);
    m_tags->setAvailablePanelExpanded(true);
    m_tags->setLoader(
        [this]
        {
            QString errorMessage;
            return withShortDatabase(m_database.databaseName(), true, &errorMessage,
                                     [](QSqlDatabase connection)
                                     { return WorkRepository(connection).tagOptions(); });
        });
    connect(m_actresses, &PersonTransferSelector::addRequested, this, [this] {
        auto *dialog = new AddActressDialog(m_database, this);
        connect(dialog, &AddActressDialog::personAdded, this, [this](qint64 id) {
            m_associationsLoaded = false;
            refreshAssociations();
            m_actresses->selectId(id);
        });
        dialog->open();
    });
    connect(m_actors, &PersonTransferSelector::addRequested, this, [this] {
        auto *dialog = new AddActorDialog(m_database, this);
        connect(dialog, &AddActorDialog::personAdded, this, [this](qint64 id) {
            m_associationsLoaded = false;
            refreshAssociations();
            m_actors->selectId(id);
        });
        dialog->open();
    });
    m_fanart =
        new FanartStripWidget(m_fanartDirectory, m_coverDirectory, m_imageFetchEndpoint, this);
    m_fanart->setProperty("workControlId", QStringLiteral("WorkFanartStrip"));
    m_fanart->setCanAdd(true);
    m_crawlerFields = new CrawlerFieldSelector(this);
    auto *crawlButton = new DesignButton(QStringLiteral("采集所选字段"), m_crawlerFields);
    crawlButton->setToolTip(QStringLiteral("通过信息补充器采集当前番号；插队并立即执行。"));
    m_crawlerFields->appendRowWidget(crawlButton, 1);
    m_saveButton = new DesignButton({}, this);
    auto *saveButton = m_saveButton;
    saveButton->setObjectName(QStringLiteral("WorkSaveButton"));
    saveButton->setMinimumHeight(kFormControlMinimumHeight);
    saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(saveButton, &QPushButton::clicked, this, &AddWorkTabPage3::save);
    connect(loadButton, &QPushButton::clicked, this, &AddWorkTabPage3::loadWorkBySerial);
    connect(detailButton, &QPushButton::clicked, this, &AddWorkTabPage3::openCurrentWorkDetail);
    connect(playButton, &QPushButton::clicked, this, &AddWorkTabPage3::playLocalVideo);
    connect(crawlButton, &QPushButton::clicked, this, &AddWorkTabPage3::crawlSelectedFields);
    connect(&m_crawlerScheduler, &CrawlerScheduler::workFetched, this,
            &AddWorkTabPage3::applyCrawledData);
    connect(&m_crawlerScheduler, &CrawlerScheduler::workFailed, this,
            [this](const QString &serial, const QString &message)
            {
                if (serial == currentSerialNumber())
                    Toast::showError(window(), QStringLiteral("采集失败：%1").arg(message),
                                     &m_themes);
            });
    connect(m_imageDrop, &ImageDropWidget::imageChanged, m_imageUrl, &QLineEdit::setText);
    connect(m_imageDrop, &ImageDropWidget::imageRejected, this,
            [this](const QString &message) { Toast::showError(window(), message, &m_themes); });
    connect(m_imageUrl, &QLineEdit::editingFinished, this,
            [this]() { m_imageDrop->setImagePath(m_imageUrl->text()); });
    connect(m_fanart, &FanartStripWidget::fanartChanged, this,
            [this](const QList<FanartEntry> &) {
                if (m_loadingEditor) return;
                m_fanartDirty = true;
                markEditorChanged();
            });
    connect(m_fanart, &FanartStripWidget::imageRejected, this,
            [this](const QString &message) { Toast::showError(window(), message, &m_themes); });
    const auto trackEditorChange = [this] { markEditorChanged(); };
    connect(m_serialNumber, &QLineEdit::textChanged, this, [this] {
        if (!m_loadingEditor)
        {
            m_serialLookupWorkId.reset();
            // Python checks the number while it is entered, so the add/load
            // actions always reflect whether the current serial already exists.
            checkSerialAvailability();
        }
        markEditorChanged();
    });
    connect(m_serialNumber, &QLineEdit::editingFinished,
            this, &AddWorkTabPage3::checkSerialAvailability);
    connect(m_serialNumber, &QLineEdit::returnPressed,
            this, &AddWorkTabPage3::checkSerialAvailability);
    connect(m_chineseTitle, &QPlainTextEdit::textChanged, this, trackEditorChange);
    connect(m_japaneseTitle, &QPlainTextEdit::textChanged, this, trackEditorChange);
    connect(m_director, &QLineEdit::textChanged, this, trackEditorChange);
    connect(m_releaseDate, &QLineEdit::textChanged, this, trackEditorChange);
    connect(m_runtime, qOverload<int>(&QSpinBox::valueChanged), this, trackEditorChange);
    connect(m_maker, qOverload<int>(&QComboBox::currentIndexChanged), this, trackEditorChange);
    connect(m_label, qOverload<int>(&QComboBox::currentIndexChanged), this, trackEditorChange);
    connect(m_series, qOverload<int>(&QComboBox::currentIndexChanged), this, trackEditorChange);
    connect(m_notes, &QTextEdit::textChanged, this, trackEditorChange);
    connect(m_chineseStory, &QPlainTextEdit::textChanged, this, trackEditorChange);
    connect(m_japaneseStory, &QPlainTextEdit::textChanged, this, trackEditorChange);
    connect(m_actresses, &PersonTransferSelector::selectionChanged, this, trackEditorChange);
    connect(m_actors, &PersonTransferSelector::selectionChanged, this, trackEditorChange);
    connect(m_tags, &WorkTagSelector::selectionChanged, this, trackEditorChange);
    connect(m_imageDrop, &ImageDropWidget::imageChanged, this,
            [this](const QString &) { markEditorChanged(); });

    // Python 版将这些内容放入 MyADS 工作区而非一个固定表单；保持相同的独立内容槽，
    // 使用户可以拆分、合并和保存标签布局。
    auto container = [this](const QString &slot, const QString &title) {
        auto *widget = new QWidget(this);
        widget->setProperty("addwork_slot", slot);
        widget->setProperty("addwork_title", title);
        auto *layout = new QVBoxLayout(widget);
        layout->setContentsMargins(8, 8, 8, 8);
        m_slotWidgets.insert(slot, widget);
        return layout;
    };
    auto *coverLayout = container(QStringLiteral("cover"), QStringLiteral("封面栏"));
    coverLayout->addWidget(m_imageDrop, 1);
    auto *basicLayout = container(QStringLiteral("basic"), QStringLiteral("基础信息"));
    QWidget *basicPane = m_slotWidgets.value(QStringLiteral("basic"));
    basicPane->setMinimumSize(kBasicPaneMinimumWidth, kBasicPaneMinimumHeight);
    basicPane->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    basicLayout->addLayout(form);
    basicLayout->addWidget(saveButton);
    auto *cnLayout = container(QStringLiteral("cn_text"), QStringLiteral("中文标题与剧情"));
    cnLayout->addWidget(new QLabel(QStringLiteral("中文标题"), m_slotWidgets.value("cn_text")));
    cnLayout->addWidget(m_chineseTitle, 1);
    cnLayout->addWidget(new QLabel(QStringLiteral("中文简介"), m_slotWidgets.value("cn_text")));
    cnLayout->addWidget(m_chineseStory, 3);
    auto *jpLayout = container(QStringLiteral("jp_text"), QStringLiteral("日文标题与剧情"));
    auto *japaneseTitleRow = new QWidget(m_slotWidgets.value("jp_text"));
    auto *japaneseTitleLayout = new QHBoxLayout(japaneseTitleRow);
    japaneseTitleLayout->setContentsMargins(0, 0, 0, 0);
    japaneseTitleLayout->addWidget(new QLabel(QStringLiteral("日文标题"), japaneseTitleRow));
    japaneseTitleLayout->addStretch();
    m_translateTitleButton = new IconButton(QStringLiteral("languages"), &m_themes,
                                             japaneseTitleRow);
    m_translateTitleButton->setObjectName(QStringLiteral("WorkTranslateJapaneseTitleButton"));
    m_translateTitleButton->setToolTip(
        QStringLiteral("翻译日文标题成中文并写在「中文标题与剧情」窗格标题框内"));
    m_translateTitleButton->setIconPixelSize(16);
    m_translateTitleButton->setButtonPixelSize(16);
    japaneseTitleLayout->addWidget(m_translateTitleButton);
    jpLayout->addWidget(japaneseTitleRow);
    jpLayout->addWidget(m_japaneseTitle, 1);
    auto *japaneseStoryRow = new QWidget(m_slotWidgets.value("jp_text"));
    auto *japaneseStoryLayout = new QHBoxLayout(japaneseStoryRow);
    japaneseStoryLayout->setContentsMargins(0, 0, 0, 0);
    japaneseStoryLayout->addWidget(new QLabel(QStringLiteral("日文简介"), japaneseStoryRow));
    japaneseStoryLayout->addStretch();
    m_translateStoryButton = new IconButton(QStringLiteral("languages"), &m_themes,
                                             japaneseStoryRow);
    m_translateStoryButton->setObjectName(QStringLiteral("WorkTranslateJapaneseStoryButton"));
    m_translateStoryButton->setToolTip(
        QStringLiteral("翻译日文剧情成中文并写在「中文标题与剧情」窗格剧情框内"));
    m_translateStoryButton->setIconPixelSize(16);
    m_translateStoryButton->setButtonPixelSize(16);
    japaneseStoryLayout->addWidget(m_translateStoryButton);
    jpLayout->addWidget(japaneseStoryRow);
    jpLayout->addWidget(m_japaneseStory, 3);
    connect(m_translateTitleButton, &QPushButton::clicked,
            this, &AddWorkTabPage3::translateJapaneseTitle);
    connect(m_translateStoryButton, &QPushButton::clicked,
            this, &AddWorkTabPage3::translateJapaneseStory);
    auto *actressLayout = container(QStringLiteral("actress"), QStringLiteral("女优选择器"));
    actressLayout->addWidget(m_actresses);
    auto *actorLayout = container(QStringLiteral("actor"), QStringLiteral("男优选择器"));
    actorLayout->addWidget(m_actors);
    auto *tagLayout = container(QStringLiteral("tag"), QStringLiteral("标签选择器"));
    tagLayout->addWidget(m_tags, 0, Qt::AlignLeft);
    auto *fanartLayout = container(QStringLiteral("fanart"), QStringLiteral("剧照"));
    fanartLayout->addWidget(m_fanart);
    auto *crawlerLayout = container(QStringLiteral("crawler"), QStringLiteral("信息补充区"));
    m_slotWidgets.value(QStringLiteral("crawler"))->setMinimumHeight(
        kWorkspaceAuxiliaryMinimumHeight);
    crawlerLayout->addWidget(m_crawlerFields);
    auto *editorLayout = container(QStringLiteral("editor"), QStringLiteral("自由记录区"));
    // Python's editor_container has no outer margin and does not cap the
    // WikiTextEdit height, so the editor fills its dock pane naturally.
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->addWidget(m_notes);
    auto *navLayout = container(QStringLiteral("nav"), QStringLiteral("外部导航"));
    m_slotWidgets.value(QStringLiteral("nav"))->setMinimumHeight(
        kWorkspaceAuxiliaryMinimumHeight);
    addManualNavigation(navLayout);
    auto *forceLayout = container(QStringLiteral("force"), QStringLiteral("图谱"));
    auto *forcePlaceholder = new QLabel(QStringLiteral("正在生成力导向图..."),
                                        m_slotWidgets.value("force"));
    forcePlaceholder->setObjectName(QStringLiteral("relationGraphPlaceholder"));
    forcePlaceholder->setAlignment(Qt::AlignCenter);
    forceLayout->addWidget(forcePlaceholder);
    auto *settingsLayout = container(QStringLiteral("settings"), QStringLiteral("设置"));
    auto *saveLayoutButton = new DesignButton(QStringLiteral("保存布局"), m_slotWidgets.value("settings"));
    auto *resetLayoutButton = new DesignButton(QStringLiteral("恢复初始布局"), m_slotWidgets.value("settings"));
    settingsLayout->addWidget(saveLayoutButton);
    settingsLayout->addWidget(resetLayoutButton);
    settingsLayout->addStretch();
    connect(saveLayoutButton, &QPushButton::clicked, this, &AddWorkTabPage3::saveWorkspaceLayout);
    connect(resetLayoutButton, &QPushButton::clicked, this, &AddWorkTabPage3::restoreDefaultWorkspace);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    m_workspace = new myads::WorkspaceWidget(this);
    myads::bindDarkeyeTheme(m_workspace, &m_themes);
    pageLayout->addWidget(m_workspace);
    QString layoutError;
    if (!QFileInfo::exists(m_workspaceLayoutPath) ||
        !m_workspace->loadLayout(m_workspaceLayoutPath,
            [this](const QJsonObject &descriptor) { return createWorkspaceContent(descriptor); },
            &layoutError))
    {
        buildDefaultWorkspace();
    }
    refreshReferences();
    m_currentWork = Work{};
    clearEditor();
    m_serialNumber->setReadOnly(false);
    updateEditorActions();
    initializeRelationGraph();
}

AddWorkTabPage3::~AddWorkTabPage3() = default;

myads::ContentConfig AddWorkTabPage3::contentConfig(const QString &slot) const
{
    myads::ContentConfig config(QStringLiteral("addwork_") + slot);
    QWidget *widget = m_slotWidgets.value(slot);
    return config.setWindowTitle(widget ? widget->property("addwork_title").toString() : slot)
        .setWidget(widget)
        .setCloseable(false);
}

std::optional<myads::ContentConfig>
AddWorkTabPage3::createWorkspaceContent(const QJsonObject &descriptor) const
{
    const QString slot = descriptor.value(QStringLiteral("addwork_slot")).toString();
    if (!m_slotWidgets.contains(slot))
        return std::nullopt;
    myads::ContentConfig config(descriptor.value(QStringLiteral("content_id"))
                                    .toString(QStringLiteral("addwork_") + slot));
    return config.setWindowTitle(descriptor.value(QStringLiteral("title"))
                                     .toString(m_slotWidgets.value(slot)->property("addwork_title").toString()))
        .setWidget(m_slotWidgets.value(slot))
        .setCloseable(descriptor.value(QStringLiteral("closeable")).toBool(false));
}

void AddWorkTabPage3::buildDefaultWorkspace()
{
    auto *root = m_workspace->rootPane();
    auto *basic = m_workspace->split(root, myads::Placement::Right, 70);
    auto *tag = m_workspace->split(basic, myads::Placement::Right, 25);
    auto *cnText = m_workspace->split(basic, myads::Placement::Bottom, 42);
    auto *fanart = m_workspace->split(tag, myads::Placement::Bottom, 20);
    auto *force = m_workspace->split(tag, myads::Placement::Right, 50);
    auto *actress = m_workspace->split(root, myads::Placement::Bottom, 50);
    auto *editor = m_workspace->split(force, myads::Placement::Bottom, 40);
    m_workspace->fillPane(root, contentConfig(QStringLiteral("settings")));
    m_workspace->fillPane(root, contentConfig(QStringLiteral("crawler")));
    m_workspace->fillPane(root, contentConfig(QStringLiteral("nav")));
    m_workspace->fillPane(root, contentConfig(QStringLiteral("cover")));
    m_workspace->fillPane(basic, contentConfig(QStringLiteral("basic")));
    m_workspace->fillPane(cnText, contentConfig(QStringLiteral("jp_text")));
    m_workspace->fillPane(cnText, contentConfig(QStringLiteral("cn_text")));
    m_workspace->fillPane(actress, contentConfig(QStringLiteral("actor")));
    m_workspace->fillPane(actress, contentConfig(QStringLiteral("actress")));
    m_workspace->fillPane(tag, contentConfig(QStringLiteral("tag")));
    m_workspace->fillPane(fanart, contentConfig(QStringLiteral("fanart")));
    m_workspace->fillPane(force, contentConfig(QStringLiteral("force")));
    m_workspace->fillPane(editor, contentConfig(QStringLiteral("editor")));
}

void AddWorkTabPage3::saveWorkspaceLayout()
{
    QString error;
    if (!QDir().mkpath(QFileInfo(m_workspaceLayoutPath).absolutePath()))
    {
        Toast::showError(window(), QStringLiteral("保存布局失败：无法创建数据目录"), &m_themes);
        return;
    }
    const bool saved = m_workspace->saveLayout(m_workspaceLayoutPath,
        [this](const myads::PaneWidget *pane, const QString &contentId) {
            QWidget *widget = pane->contentWidget(contentId);
            const QString slot = widget ? widget->property("addwork_slot").toString() : QString();
            if (slot.isEmpty()) return QJsonObject{};
            return QJsonObject{{QStringLiteral("addwork_slot"), slot},
                               {QStringLiteral("content_id"), contentId},
                               {QStringLiteral("title"), pane->contentTitle(contentId)},
                               {QStringLiteral("closeable"), pane->isContentCloseable(contentId)}};
        }, {}, &error);
    if (saved)
        Toast::showSuccess(window(), QStringLiteral("工作区布局已保存"), &m_themes);
    else
        Toast::showError(window(), QStringLiteral("保存布局失败：%1").arg(error), &m_themes);
}

void AddWorkTabPage3::restoreDefaultWorkspace()
{
    // 布局文件已删除，下一次进入同样会从 Python 基线的默认拆分开始。
    QFile::remove(m_workspaceLayoutPath);
    Toast::showSuccess(window(), QStringLiteral("默认布局将在下次打开页面时恢复"), &m_themes);
}

void AddWorkTabPage3::loadWorkBySerial()
{
    const QString serial = currentSerialNumber();
    if (serial.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("请先输入番号"), &m_themes);
        return;
    }
    QString error;
    const auto id = withShortDatabase(
        m_database.databaseName(), true, &error,
        [&serial, &error](QSqlDatabase connection)
        { return WorkRepository(connection).findIdBySerial(serial, &error); });
    if (!id.has_value())
    {
        Toast::showWarning(window(), error.isEmpty() ? QStringLiteral("未找到该作品") : error,
                           &m_themes);
        return;
    }
    loadWork(*id);
}

void AddWorkTabPage3::checkSerialAvailability()
{
    if (m_loadingEditor || (m_currentWork.has_value() && m_currentWork->id > 0))
        return;

    const QString serial = currentSerialNumber();
    m_serialLookupWorkId.reset();
    if (!serial.isEmpty())
    {
        QString errorMessage;
        m_serialLookupWorkId = withShortDatabase(
            m_database.databaseName(), true, &errorMessage,
            [&serial, &errorMessage](QSqlDatabase connection)
            { return WorkRepository(connection).findIdBySerial(serial, &errorMessage); });
    }
    updateEditorActions();
}

void AddWorkTabPage3::openCurrentWorkDetail()
{
    if (!m_currentWork.has_value() || m_currentWork->id <= 0)
    {
        Toast::showWarning(window(), QStringLiteral("请先加载或保存作品"), &m_themes);
        return;
    }
    emit workLinkRequested(m_currentWork->id);
}

void AddWorkTabPage3::playLocalVideo()
{
    if (!m_currentWork.has_value() || m_currentWork->id <= 0)
    {
        Toast::showWarning(window(), QStringLiteral("请先加载已保存的作品"), &m_themes);
        return;
    }
    QStringList paths;
    for (const QString &path : m_currentWork->videoUrl.split(',', Qt::SkipEmptyParts))
    {
        const QString trimmed = path.trimmed();
        if (!trimmed.isEmpty()) paths.append(trimmed);
    }
    if (paths.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("该作品没有可播放的本地视频"), &m_themes);
        return;
    }
    const AppSettings appSettings = settings::app();
    const auto play = [this, &appSettings](const QString &path) {
        if (!utils::playVideo(path, appSettings.localVideoPlayer))
            Toast::showError(window(), QStringLiteral("无法播放视频：%1").arg(path), &m_themes);
    };
    if (paths.size() == 1)
    {
        play(paths.constFirst());
        return;
    }
    QMenu menu(this);
    for (const QString &path : std::as_const(paths))
    {
        QAction *action = menu.addAction(QFileInfo(path).fileName());
        action->setData(path);
    }
    if (QAction *selected = menu.exec(QCursor::pos()))
        play(selected->data().toString());
}

void AddWorkTabPage3::addManualNavigation(QVBoxLayout *layout)
{
    QFile file(settings::Paths().crawlerNavButtonsFile());
    if (!file.open(QIODevice::ReadOnly))
    {
        auto *message = new QLabel(QStringLiteral("未找到 crawler_nav_buttons.json"),
                                   m_slotWidgets.value(QStringLiteral("nav")));
        message->setAlignment(Qt::AlignCenter);
        layout->addWidget(message);
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isArray())
    {
        auto *message = new QLabel(QStringLiteral("导航配置格式无效：%1").arg(parseError.errorString()),
                                   m_slotWidgets.value(QStringLiteral("nav")));
        message->setWordWrap(true);
        layout->addWidget(message);
        return;
    }
    // 与 Python CrawlerManualNavPage 相同：按钮网格位于滚动区内，不能把 MyADS 窗格
    // 的最小高度顶大。
    auto *scroll = new QScrollArea(m_slotWidgets.value(QStringLiteral("nav")));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumSize(0, 0);
    scroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    auto *gridHost = new QWidget(scroll);
    gridHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    constexpr int columns = 2;
    int count = 0;
    for (const QJsonValue &value : document.array())
    {
        const QJsonObject config = value.toObject();
        const QString name = config.value(QStringLiteral("name")).toString().trimmed();
        const QString templateUrl = config.value(QStringLiteral("url")).toString().trimmed();
        if (name.isEmpty() || templateUrl.isEmpty()) continue;
        auto *button = new DesignButton(name, gridHost);
        button->setToolTip(config.value(QStringLiteral("description")).toString());
        grid->addWidget(button, count / columns, count % columns);
        ++count;
        connect(button, &QPushButton::clicked, this, [this, config, templateUrl] {
            QString url = templateUrl;
            if (url.contains(QStringLiteral("{serial}")))
            {
                QString serial = currentSerialNumber();
                const QString transform = config.value(QStringLiteral("serial_transform")).toString();
                if (transform == QStringLiteral("fanza")) serial = serial::convertFanza(serial);
                else if (transform == QStringLiteral("supjav") && serial.startsWith(QStringLiteral("FC2-"), Qt::CaseInsensitive))
                    serial = serial.section('-', -1);
                if (serial.isEmpty())
                {
                    Toast::showWarning(window(), QStringLiteral("请输入番号后再打开此链接"), &m_themes);
                    return;
                }
                url.replace(QStringLiteral("{serial}"), QString::fromUtf8(QUrl::toPercentEncoding(serial)));
            }
            if (!QDesktopServices::openUrl(QUrl(url)))
                Toast::showError(window(), QStringLiteral("无法打开链接"), &m_themes);
        });
    }
    auto *reveal = new DesignButton(QStringLiteral("定位 JSON 配置文件"), gridHost);
    reveal->setToolTip(QStringLiteral("在文件管理器中选中 crawler_nav_buttons.json"));
    grid->addWidget(reveal, (count + columns - 1) / columns, 0, 1, columns);
    connect(reveal, &QPushButton::clicked, this, [this] {
        if (!utils::revealInFileManager(settings::Paths().crawlerNavButtonsFile()))
            Toast::showError(window(), QStringLiteral("无法打开配置文件位置"), &m_themes);
    });
    scroll->setWidget(gridHost);
    layout->addWidget(scroll, 1);
}

void AddWorkTabPage3::beginCreate()
{
    if (!m_associationsLoaded)
        refreshAssociations();
    m_loadingEditor = true;
    m_currentWork = Work{};
    m_serialLookupWorkId.reset();
    clearEditor();
    m_loadingEditor = false;
    m_serialNumber->setReadOnly(false);
    m_serialNumber->setFocus();
    updateEditorActions();
}

void AddWorkTabPage3::beginCreateAndCrawl(const QString &serialNumber)
{
    beginCreate();
    const QString serial = serialNumber.trimmed().toUpper();
    if (serial.isEmpty())
        return;
    m_serialNumber->setText(serial);
    m_crawlerFields->clearSelection(); // 空集合按 Python 约定表示全字段采集。
    crawlSelectedFields();
}

bool AddWorkTabPage3::loadWork(qint64 workId)
{
    if (!m_associationsLoaded)
        refreshAssociations();
    QString errorMessage;
    const std::optional<WorkDetails> details = withShortDatabase(
        m_database.databaseName(), true, &errorMessage,
        [workId, &errorMessage](QSqlDatabase connection)
        { return WorkRepository(connection).findDetailsById(workId, &errorMessage); });
    if (!details.has_value())
    {
        Toast::showError(window(),
                         errorMessage.isEmpty() ? QStringLiteral("作品不存在") : errorMessage,
                         &m_themes);
        return false;
    }
    m_loadingEditor = true;
    m_currentWork = details->work;
    m_serialLookupWorkId.reset();
    applyWork(details->work);
    QList<qint64> actressIds;
    for (const WorkPersonReference &person : details->actresses)
        actressIds.append(person.id);
    QList<qint64> actorIds;
    for (const WorkPersonReference &person : details->actors)
        actorIds.append(person.id);
    QList<qint64> tagIds;
    for (const TagOption &tag : details->tags)
        tagIds.append(tag.id);
    m_actresses->setSelectedIds(actressIds);
    m_actors->setSelectedIds(actorIds);
    m_tags->setSelectedIds(tagIds);
    m_loadedActressIds = actressIds;
    m_loadedActorIds = actorIds;
    m_loadedTagIds = tagIds;
    m_loadingEditor = false;
    m_serialNumber->setReadOnly(true);
    updateEditorActions();
    updateRelationGraph();
    return true;
}

void AddWorkTabPage3::refreshAssociations()
{
    const auto loadPeople = [this](PersonKind kind)
    {
        PersonSearch search;
        search.kind = kind;
        QString errorMessage;
        const QList<IdLabelOption> options = withShortDatabase(
            m_database.databaseName(), true, &errorMessage,
            [&search, &errorMessage](QSqlDatabase connection)
            {
                PersonRepository repository(connection);
                const std::optional<int> count = repository.count(search, &errorMessage);
                if (!count.has_value()) return QList<IdLabelOption>{};
                search.limit = qMax(1, *count);
                QList<IdLabelOption> people;
                for (const PersonSummary &person : repository.search(search, &errorMessage))
                    people.append({person.id, person.name, {}});
                return people;
            });
        if (!errorMessage.isEmpty())
            Toast::showError(window(), errorMessage, &m_themes);
        return options;
    };
    m_actresses->setOptions(loadPeople(PersonKind::Actress));
    m_actors->setOptions(loadPeople(PersonKind::Actor));
    m_tags->reloadTags();
    m_associationsLoaded = true;
}

void AddWorkTabPage3::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_associationsLoaded)
        refreshAssociations();
}

void AddWorkTabPage3::refreshReferences()
{
    populateReferenceCombo(m_maker, ReferenceKind::Maker,
                           m_currentWork.has_value() ? m_currentWork->makerId : std::nullopt);
    populateReferenceCombo(m_label, ReferenceKind::Label,
                           m_currentWork.has_value() ? m_currentWork->labelId : std::nullopt);
    populateReferenceCombo(m_series, ReferenceKind::Series,
                           m_currentWork.has_value() ? m_currentWork->seriesId : std::nullopt);
}

QString AddWorkTabPage3::currentSerialNumber() const
{
    return m_serialNumber->text().trimmed();
}

void AddWorkTabPage3::translateJapaneseTitle()
{
    const QString source = m_japaneseTitle->toPlainText();
    // Python resolves translation settings at click time, rather than when
    // this persistent editor is constructed.  Do the same so changes saved
    // in Settings take effect immediately without reopening the editor.
    auto *translator = new LlmTranslationService(settings::translation(), this);
    connect(translator, &LlmTranslationService::translationFinished, this,
            [this, translator](quint64, const QString &translation, const QString &errorMessage)
            {
                translator->deleteLater();
                if (!errorMessage.isEmpty() || translation.trimmed().isEmpty())
                {
                    Toast::showWarning(window(),
                                       QStringLiteral("翻译失败：网络/代理不稳定或触发限流，请稍后重试。"),
                                       &m_themes);
                    return;
                }
                m_chineseTitle->setPlainText(translation);
            });
    static_cast<void>(translator->translate(source));
}

void AddWorkTabPage3::translateJapaneseStory()
{
    const QString source = m_japaneseStory->toPlainText();
    auto *translator = new LlmTranslationService(settings::translation(), this);
    connect(translator, &LlmTranslationService::translationFinished, this,
            [this, translator](quint64, const QString &translation, const QString &errorMessage)
            {
                translator->deleteLater();
                if (!errorMessage.isEmpty() || translation.trimmed().isEmpty())
                {
                    Toast::showWarning(window(),
                                       QStringLiteral("翻译失败：网络/代理不稳定或触发限流，请稍后重试。"),
                                       &m_themes);
                    return;
                }
                m_chineseStory->setPlainText(translation);
            });
    static_cast<void>(translator->translate(source));
}

void AddWorkTabPage3::applyWork(const Work &work)
{
    m_serialNumber->setText(work.serialNumber);
    m_chineseTitle->setPlainText(work.chineseTitle);
    m_japaneseTitle->setPlainText(work.japaneseTitle);
    m_director->setText(work.director);
    m_releaseDate->setText(work.releaseDate);
    m_runtime->setValue(work.runtime.value_or(0));
    m_imageUrl->setText(work.imageUrl);
    m_imageDrop->setImagePath(work.imageUrl);
    m_videoUrl->setText(work.videoUrl);
    m_notes->setPlainText(work.notes);
    m_chineseStory->setPlainText(work.chineseStory);
    m_japaneseStory->setPlainText(work.japaneseStory);
    QList<FanartEntry> fanartEntries;
    QString fanartError;
    if (!FanartStripWidget::parseJson(work.fanartJson, &fanartEntries, &fanartError))
        Toast::showWarning(window(), fanartError, &m_themes);
    m_fanart->setEntries(fanartEntries);
    m_fanartDirty = false;
    refreshReferences();
}

void AddWorkTabPage3::initializeRelationGraph()
{
    if (m_relationGraph != nullptr) return;
    auto *container = m_slotWidgets.value(QStringLiteral("force"));
    if (container == nullptr) return;
    // The Python page owns a per-editor ForceDirectedViewWidget and starts it
    // with an EmptyFilter.  Keep the same isolation here rather than sharing
    // the full-graph page's filter state.
    m_graphManager = std::make_unique<graph::GraphManager>(m_database);
    m_relationGraph = new ForceDirectPage(m_themes, *m_graphManager, container);
    auto *layout = qobject_cast<QVBoxLayout *>(container->layout());
    if (layout == nullptr) return;
    if (auto *placeholder = container->findChild<QLabel *>(QStringLiteral("relationGraphPlaceholder"))) {
        layout->removeWidget(placeholder);
        placeholder->deleteLater();
    }
    layout->addWidget(m_relationGraph);
    connect(m_relationGraph, &ForceDirectPage::workRequested,
            this, &AddWorkTabPage3::workLinkRequested);
    connect(m_relationGraph, &ForceDirectPage::actressRequested,
            this, &AddWorkTabPage3::actressLinkRequested);
    // Python AddWorkTabPage3 keeps this as an ego graph only.  The favorite
    // filter belongs to the standalone relationship page, not this editor pane.
    m_relationGraph->setFavoriteFilterToggleVisible(false);
    m_relationGraph->showEmptyGraph();
}

void AddWorkTabPage3::updateRelationGraph()
{
    if (m_relationGraph == nullptr || !m_currentWork.has_value()
        || m_currentWork->id <= 0) return;
    m_relationGraph->setEgoGraph(QStringLiteral("w%1").arg(m_currentWork->id), 3);
}

void AddWorkTabPage3::crawlSelectedFields()
{
    const QString serial = currentSerialNumber();
    if (serial.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("请先输入番号"), &m_themes);
        return;
    }
    m_crawlerScheduler.enqueue({serial}, true, m_crawlerFields->selectedFields(), true);
}

void AddWorkTabPage3::applyCrawledData(const QString &serialNumber, const QJsonObject &payload,
                                       const QSet<QString> &selectedFields, bool withGui)
{
    if (!withGui)
        return;
    if (serialNumber != currentSerialNumber())
    {
        m_crawlerScheduler.complete(serialNumber, false);
        return;
    }
    const auto wants = [&selectedFields](const QString &field)
    {
        return selectedFields.isEmpty() || selectedFields.contains(field);
    };
    const auto text = [&payload](const QString &key) { return payload.value(key).toString(); };
    m_loadingEditor = true;
    if (wants(QStringLiteral("release_date")))
        m_releaseDate->setText(text(QStringLiteral("release_date")));
    if (wants(QStringLiteral("director")))
        m_director->setText(text(QStringLiteral("director")));
    if (wants(QStringLiteral("runtime")))
        m_runtime->setValue(payload.value(QStringLiteral("runtime")).toInt());
    if (wants(QStringLiteral("cn_title")))
        m_chineseTitle->setPlainText(text(QStringLiteral("cn_title")));
    if (wants(QStringLiteral("jp_title")))
        m_japaneseTitle->setPlainText(text(QStringLiteral("jp_title")));
    if (wants(QStringLiteral("cn_story")))
        m_chineseStory->setPlainText(text(QStringLiteral("cn_story")));
    if (wants(QStringLiteral("jp_story")))
        m_japaneseStory->setPlainText(text(QStringLiteral("jp_story")));

    const auto jsonStrings = [&payload](const QString &key)
    {
        QStringList values;
        for (const QJsonValue &value : payload.value(key).toArray())
        {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty() && !values.contains(text))
                values.append(text);
        }
        return values;
    };
    const auto ensureReference = [this](ReferenceKind kind, const QString &name)
    {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty())
            return std::optional<qint64>{};
        QString errorMessage;
        std::optional<qint64> id = m_references.findByName(kind, trimmed, &errorMessage);
        if (!id.has_value() && errorMessage.isEmpty())
            id = m_references.create(kind, trimmed, &errorMessage);
        if (!errorMessage.isEmpty())
            Toast::showWarning(window(), QStringLiteral("创建关联资料失败：%1").arg(errorMessage),
                               &m_themes);
        return id;
    };
    const auto selectReference = [this, &payload, &wants, &ensureReference](
                                     QComboBox *combo, const QString &field, ReferenceKind kind)
    {
        if (!wants(field))
            return;
        const std::optional<qint64> id = ensureReference(kind, payload.value(field).toString());
        const int index = id.has_value() ? combo->findData(*id) : -1;
        if (index >= 0)
            combo->setCurrentIndex(index);
    };
    if (wants(QStringLiteral("maker")))
        ensureReference(ReferenceKind::Maker, text(QStringLiteral("maker")));
    if (wants(QStringLiteral("label")))
        ensureReference(ReferenceKind::Label, text(QStringLiteral("label")));
    if (wants(QStringLiteral("series")))
        ensureReference(ReferenceKind::Series, text(QStringLiteral("series")));
    refreshReferences();
    selectReference(m_maker, QStringLiteral("maker"), ReferenceKind::Maker);
    selectReference(m_label, QStringLiteral("label"), ReferenceKind::Label);
    selectReference(m_series, QStringLiteral("series"), ReferenceKind::Series);

    const auto ensurePeople = [this](PersonKind kind, const QStringList &names,
                                     QList<qint64> *newlyCreated = nullptr)
    {
        QString errorMessage;
        const QList<qint64> created = withShortDatabase(
            m_database.databaseName(), false, &errorMessage,
            [kind, &names, &errorMessage, newlyCreated](QSqlDatabase connection)
            {
                PersonRepository repository(connection);
                QList<qint64> result;
                for (const QString &name : names)
                {
                    std::optional<qint64> id = repository.findByName(kind, name, &errorMessage);
                    if (!id.has_value() && errorMessage.isEmpty())
                    {
                        id = repository.create(kind, {}, name, &errorMessage);
                        if (id.has_value() && newlyCreated != nullptr)
                            newlyCreated->append(*id);
                    }
                    if (!id.has_value() || !errorMessage.isEmpty())
                        return QList<qint64>{};
                    result.append(*id);
                }
                return result;
            });
        if (!errorMessage.isEmpty())
            Toast::showWarning(window(), QStringLiteral("创建演员资料失败：%1").arg(errorMessage),
                               &m_themes);
        return created;
    };
    QList<qint64> createdActressIds;
    const QList<qint64> actressIds = wants(QStringLiteral("actress"))
        ? ensurePeople(PersonKind::Actress, jsonStrings(QStringLiteral("actress_list")),
                       &createdActressIds)
        : QList<qint64>{};
    const QList<qint64> actorIds = wants(QStringLiteral("actor"))
        ? ensurePeople(PersonKind::Actor, jsonStrings(QStringLiteral("actor_list")))
        : QList<qint64>{};

    QList<qint64> tagIds;
    if (wants(QStringLiteral("tag")))
    {
        QString errorMessage;
        QList<TagRecord> records = m_references.listTags(&errorMessage);
        for (const QString &name : jsonStrings(QStringLiteral("tag_list")))
        {
            auto found = std::find_if(records.cbegin(), records.cend(), [&name](const TagRecord &tag)
            { return tag.name == name || tag.aliases.contains(name); });
            std::optional<qint64> id;
            if (found != records.cend())
                id = found->id;
            else if (errorMessage.isEmpty())
                id = m_references.createTag(name, std::nullopt, {}, {}, &errorMessage);
            if (id.has_value())
                tagIds.append(*id);
        }
        if (!errorMessage.isEmpty())
            Toast::showWarning(window(), QStringLiteral("创建标签失败：%1").arg(errorMessage),
                               &m_themes);
    }
    if (!actressIds.isEmpty() || !actorIds.isEmpty() || !tagIds.isEmpty())
        refreshAssociations();
    if (wants(QStringLiteral("actress")))
        m_actresses->setSelectedIds(actressIds);
    if (wants(QStringLiteral("actor")))
        m_actors->setSelectedIds(actorIds);
    if (wants(QStringLiteral("tag")))
        m_tags->setSelectedIds(tagIds);
    if (!createdActressIds.isEmpty())
        emit actressesCreated(createdActressIds);
    m_loadingEditor = false;
    markEditorChanged();
    if (wants(QStringLiteral("cover")))
    {
        m_crawlCoverUrls = jsonStrings(QStringLiteral("cover_url_list"));
        if (!m_crawlCoverUrls.isEmpty())
        {
            m_crawlSerial = serialNumber;
            m_crawlCoverIndex = 0;
            fetchNextCrawledCover();
            return;
        }
    }
    m_crawlerScheduler.complete(serialNumber, true);
    Toast::showSuccess(window(), QStringLiteral("采集完成"), &m_themes);
}

void AddWorkTabPage3::fetchNextCrawledCover()
{
    if (m_crawlSerial.isEmpty() || m_crawlCoverIndex < 0
        || m_crawlCoverIndex >= m_crawlCoverUrls.size())
        return;
    const QString cacheRoot = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation);
    const QString crawlDirectory = QDir(cacheRoot).filePath(QStringLiteral("crawler-covers"));
    QDir().mkpath(crawlDirectory);
    const QString destination = QDir(crawlDirectory).filePath(
        QStringLiteral("%1.jpg").arg(m_crawlSerial.toUpper()));
    const quint64 requestId = m_crawlerImageFetch->fetchToJpeg(
        QUrl(m_crawlCoverUrls.at(m_crawlCoverIndex)), destination);
    Q_UNUSED(requestId);
}

Work AddWorkTabPage3::editorWork() const
{
    Work work = m_currentWork.value_or(Work{});
    work.serialNumber = m_serialNumber->text().trimmed();
    work.chineseTitle = m_chineseTitle->toPlainText();
    work.japaneseTitle = m_japaneseTitle->toPlainText();
    work.director = m_director->text();
    work.releaseDate = m_releaseDate->text();
    work.runtime = m_runtime->value() == 0 ? std::nullopt : std::optional<int>(m_runtime->value());
    work.imageUrl = m_imageUrl->text();
    work.videoUrl = m_videoUrl->text();
    work.notes = m_notes->toPlainText();
    work.chineseStory = m_chineseStory->toPlainText();
    work.japaneseStory = m_japaneseStory->toPlainText();
    const auto selectedId = [](const QComboBox *combo) -> std::optional<qint64>
    {
        const qint64 id = combo->currentData().toLongLong();
        return id > 0 ? std::optional<qint64>(id) : std::nullopt;
    };
    work.makerId = selectedId(m_maker);
    work.labelId = selectedId(m_label);
    work.seriesId = selectedId(m_series);
    return work;
}

void AddWorkTabPage3::save()
{
    Work work = editorWork();
    if (work.serialNumber.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("番号不能为空"), &m_themes);
        return;
    }
    QString errorMessage;
    const QString oldFanartJson = work.fanartJson;
    QList<FanartEntry> finalizedFanart;
    QStringList createdFanartFiles;
    if (m_fanartDirty)
    {
        if (!m_fanart->finalizedEntries(work.serialNumber, &finalizedFanart, &createdFanartFiles,
                                        &errorMessage))
        {
            Toast::showError(window(), QStringLiteral("保存失败：%1").arg(errorMessage), &m_themes);
            return;
        }
        work.fanartJson = FanartStripWidget::toJson(finalizedFanart);
    }
    const auto rollbackFanart = [&]()
    {
        bool succeeded = true;
        for (const QString &createdFile : std::as_const(createdFanartFiles))
            succeeded =
                (!QFileInfo::exists(createdFile) || QFile::remove(createdFile)) && succeeded;
        return succeeded;
    };
    QString coverRelativePath;
    QString coverTargetPath;
    QString coverBackupPath;
    QTemporaryDir backupDirectory;
    bool coverWritten = false;
    bool hadExistingCover = false;
    if (m_imageDrop->isDirty() && !m_imageDrop->imagePath().isEmpty())
    {
        QString coverFileName = work.serialNumber.toUpper();
        for (const QChar character : QStringLiteral("\\/:*?\"<>|"))
            coverFileName.replace(character, QChar('_'));
        if (!coverFileName.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive))
            coverFileName += QStringLiteral(".jpg");
        coverTargetPath = QDir(m_coverDirectory).filePath(coverFileName);
        hadExistingCover = QFileInfo::exists(coverTargetPath);
        if (hadExistingCover)
        {
            if (!backupDirectory.isValid())
            {
                rollbackFanart();
                Toast::showError(window(), QStringLiteral("保存失败：无法创建封面备份目录"),
                                 &m_themes);
                return;
            }
            coverBackupPath = backupDirectory.filePath(coverFileName);
            if (!QFile::copy(coverTargetPath, coverBackupPath))
            {
                rollbackFanart();
                Toast::showError(window(), QStringLiteral("保存失败：无法备份原封面"), &m_themes);
                return;
            }
        }
        if (!m_imageDrop->persistAsJpeg(coverFileName, &coverRelativePath, &errorMessage))
        {
            rollbackFanart();
            Toast::showError(window(), QStringLiteral("保存失败：%1").arg(errorMessage), &m_themes);
            return;
        }
        work.imageUrl = coverRelativePath;
        coverWritten = true;
    }
    else if (m_imageDrop->isDirty())
    {
        work.imageUrl.clear();
    }

    const auto rollbackFiles = [&]()
    {
        const bool succeeded = rollbackFanart();
        if (!coverWritten)
            return succeeded;
        const bool removed = !QFileInfo::exists(coverTargetPath) || QFile::remove(coverTargetPath);
        const bool restored = !hadExistingCover || QFile::copy(coverBackupPath, coverTargetPath);
        return succeeded && removed && restored;
    };
    const bool created = work.id <= 0;
    qint64 workId = work.id;
    if (created)
    {
        const auto inserted = withShortDatabase(
            m_database.databaseName(), false, &errorMessage,
            [&work, this, &errorMessage](QSqlDatabase connection)
            {
                return WorkRepository(connection).insertComplete(
                    work, m_actresses->selectedIds(), m_actors->selectedIds(),
                    m_tags->selectedIds(), &errorMessage);
            });
        if (!inserted.has_value())
        {
            if (!rollbackFiles())
                errorMessage += QStringLiteral("；图片回滚失败");
            Toast::showError(window(), QStringLiteral("添加失败：%1").arg(errorMessage), &m_themes);
            return;
        }
        workId = *inserted;
        work.id = workId;
        m_currentWork = work;
        m_serialNumber->setReadOnly(true);
    }
    else if (!withShortDatabase(
                 m_database.databaseName(), false, &errorMessage,
                 [&work, this, &errorMessage](QSqlDatabase connection)
                 {
                     return WorkRepository(connection).updateComplete(
                         work, m_actresses->selectedIds(), m_actors->selectedIds(),
                         m_tags->selectedIds(), &errorMessage);
                 }))
    {
        if (!rollbackFiles())
            errorMessage += QStringLiteral("；图片回滚失败");
        Toast::showError(window(), QStringLiteral("保存失败：%1").arg(errorMessage), &m_themes);
        return;
    }
    else
    {
        m_currentWork = work;
    }
    m_imageUrl->setText(work.imageUrl);
    m_imageDrop->setImagePath(work.imageUrl);
    if (m_fanartDirty)
    {
        m_fanart->setEntries(finalizedFanart);
        m_fanartDirty = false;
        QList<FanartEntry> oldEntries;
        QList<FanartEntry> newEntries;
        if (FanartStripWidget::parseJson(oldFanartJson, &oldEntries) &&
            FanartStripWidget::parseJson(work.fanartJson, &newEntries))
        {
            QSet<QString> retained;
            for (const FanartEntry &entry : std::as_const(newEntries))
                retained.insert(entry.file);
            for (const FanartEntry &entry : std::as_const(oldEntries))
            {
                const QString file = QDir::cleanPath(entry.file);
                if (file.isEmpty() || retained.contains(file) || QFileInfo(file).isAbsolute() ||
                    file == QStringLiteral("..") || file.startsWith(QStringLiteral("../")))
                    continue;
                const QString fanartPath = QDir(m_fanartDirectory).filePath(file);
                if (QFileInfo::exists(fanartPath))
                    QFile::remove(fanartPath);
                else
                {
                    const QString legacyPath = QDir(m_coverDirectory).filePath(file);
                    if (QFileInfo::exists(legacyPath))
                        QFile::remove(legacyPath);
                }
            }
        }
    }
    if (m_graphManager != nullptr) {
        m_graphManager->scheduleRefreshWork(workId);
        updateRelationGraph();
    }
    m_loadedActressIds = m_actresses->selectedIds();
    m_loadedActorIds = m_actors->selectedIds();
    m_loadedTagIds = m_tags->selectedIds();
    updateEditorActions();
    emit workSaved(workId, created);
    Toast::showSuccess(window(),
                       created ? QStringLiteral("作品添加成功") : QStringLiteral("作品信息已保存"),
                       &m_themes);
}

void AddWorkTabPage3::clearEditor()
{
    for (QLineEdit *edit : {m_serialNumber, m_director, m_releaseDate, m_imageUrl, m_videoUrl})
        edit->clear();
    m_notes->clear();
    for (QPlainTextEdit *edit : {m_chineseTitle, m_japaneseTitle, m_chineseStory,
                                 m_japaneseStory})
        edit->clear();
    m_runtime->setValue(0);
    m_maker->setCurrentIndex(0);
    m_label->setCurrentIndex(0);
    m_series->setCurrentIndex(0);
    m_imageDrop->setImagePath({});
    m_fanart->setEntries({});
    m_fanartDirty = false;
    m_actresses->clearSelection();
    m_actors->clearSelection();
    m_tags->clearSelection();
    m_loadedActressIds.clear();
    m_loadedActorIds.clear();
    m_loadedTagIds.clear();
    if (m_relationGraph != nullptr) m_relationGraph->showEmptyGraph();
}

void AddWorkTabPage3::markEditorChanged()
{
    if (!m_loadingEditor)
        updateEditorActions();
}

void AddWorkTabPage3::updateEditorActions()
{
    const auto sameIds = [](QList<qint64> first, QList<qint64> second) {
        std::sort(first.begin(), first.end());
        std::sort(second.begin(), second.end());
        return first == second;
    };
    const bool hasSavedWork = m_currentWork.has_value() && m_currentWork->id > 0;
    const bool hasSerialNumber = !currentSerialNumber().isEmpty();
    const bool hasExistingSerial = m_serialLookupWorkId.has_value();
    const bool hasLocalVideo = hasSavedWork
        && !m_currentWork->videoUrl.trimmed().isEmpty();

    m_detailButton->setEnabled(hasSavedWork);
    m_playButton->setEnabled(hasLocalVideo);
    const bool canLoad = hasExistingSerial || hasSavedWork;
    m_loadButton->setEnabled(canLoad);
    m_loadButton->setVariant(canLoad
                                 ? QStringLiteral("warning")
                                 : QStringLiteral("disabled"));

    if (!hasSavedWork)
    {
        updateModifiedFieldHighlights();
        const bool canCreate = hasSerialNumber && !hasExistingSerial;
        m_saveButton->setText(canCreate ? QStringLiteral("添加") : QStringLiteral("----"));
        m_saveButton->setVariant(canCreate ? QStringLiteral("add")
                                            : QStringLiteral("disabled"));
        m_saveButton->setEnabled(canCreate);
        return;
    }

    // The Python editor only enables the modify action when a loaded work has
    // changed.  Its disabled state deliberately has no action label.
    const Work editedWork = editorWork();
    const bool changed = editedWork.serialNumber != m_currentWork->serialNumber
        || editedWork.chineseTitle != m_currentWork->chineseTitle
        || editedWork.japaneseTitle != m_currentWork->japaneseTitle
        || editedWork.director != m_currentWork->director
        || editedWork.releaseDate != m_currentWork->releaseDate
        || !sameRuntime(editedWork.runtime, m_currentWork->runtime)
        || editedWork.imageUrl != m_currentWork->imageUrl
        || editedWork.notes != m_currentWork->notes
        || editedWork.chineseStory != m_currentWork->chineseStory
        || editedWork.japaneseStory != m_currentWork->japaneseStory
        || editedWork.makerId != m_currentWork->makerId
        || editedWork.labelId != m_currentWork->labelId
        || editedWork.seriesId != m_currentWork->seriesId
        || !sameIds(m_actresses->selectedIds(), m_loadedActressIds)
        || !sameIds(m_actors->selectedIds(), m_loadedActorIds)
        || !sameIds(m_tags->selectedIds(), m_loadedTagIds)
        || m_fanartDirty
        || m_imageDrop->isDirty();
    updateModifiedFieldHighlights();
    m_saveButton->setText(changed ? QStringLiteral("修改") : QStringLiteral("----"));
    m_saveButton->setVariant(changed ? QStringLiteral("modify")
                                     : QStringLiteral("disabled"));
    m_saveButton->setEnabled(changed);
}

void AddWorkTabPage3::updateModifiedFieldHighlights()
{
    const auto setModified = [](QWidget *widget, bool modified) {
        widget->setProperty("addWorkModified", modified);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    };

    if (!m_currentWork.has_value() || m_currentWork->id <= 0)
    {
        for (QWidget *widget : {static_cast<QWidget *>(m_chineseTitle),
                                static_cast<QWidget *>(m_japaneseTitle),
                                static_cast<QWidget *>(m_director),
                                static_cast<QWidget *>(m_releaseDate),
                                static_cast<QWidget *>(m_runtime),
                                static_cast<QWidget *>(m_maker),
                                static_cast<QWidget *>(m_label),
                                static_cast<QWidget *>(m_series),
                                static_cast<QWidget *>(m_notes),
                                static_cast<QWidget *>(m_chineseStory),
                                static_cast<QWidget *>(m_japaneseStory),
                                static_cast<QWidget *>(m_imageDrop),
                                static_cast<QWidget *>(m_actresses),
                                static_cast<QWidget *>(m_actors),
                                static_cast<QWidget *>(m_tags),
                                static_cast<QWidget *>(m_fanart)})
            setModified(widget, false);
        return;
    }

    const Work editedWork = editorWork();
    const auto sameIds = [](QList<qint64> first, QList<qint64> second) {
        std::sort(first.begin(), first.end());
        std::sort(second.begin(), second.end());
        return first == second;
    };
    setModified(m_chineseTitle, editedWork.chineseTitle != m_currentWork->chineseTitle);
    setModified(m_japaneseTitle, editedWork.japaneseTitle != m_currentWork->japaneseTitle);
    setModified(m_director, editedWork.director != m_currentWork->director);
    setModified(m_releaseDate, editedWork.releaseDate != m_currentWork->releaseDate);
    setModified(m_runtime, !sameRuntime(editedWork.runtime, m_currentWork->runtime));
    setModified(m_maker, editedWork.makerId != m_currentWork->makerId);
    setModified(m_label, editedWork.labelId != m_currentWork->labelId);
    setModified(m_series, editedWork.seriesId != m_currentWork->seriesId);
    setModified(m_notes, editedWork.notes != m_currentWork->notes);
    setModified(m_chineseStory, editedWork.chineseStory != m_currentWork->chineseStory);
    setModified(m_japaneseStory, editedWork.japaneseStory != m_currentWork->japaneseStory);
    setModified(m_imageDrop, m_imageDrop->isDirty());
    setModified(m_actresses, !sameIds(m_actresses->selectedIds(), m_loadedActressIds));
    setModified(m_actors, !sameIds(m_actors->selectedIds(), m_loadedActorIds));
    setModified(m_tags, !sameIds(m_tags->selectedIds(), m_loadedTagIds));
    setModified(m_fanart, m_fanartDirty);
}

void AddWorkTabPage3::populateReferenceCombo(QComboBox *combo, ReferenceKind kind,
                                              std::optional<qint64> selectedId)
{
    QString errorMessage;
    const QList<ReferenceRecord> records = withShortDatabase(
        m_database.databaseName(), true, &errorMessage,
        [kind, &errorMessage](QSqlDatabase connection)
        { return ReferenceRepository(connection).list(kind, &errorMessage); });
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    combo->clear();
    combo->addItem(QStringLiteral("未选择"), QVariant());
    for (const ReferenceRecord &record : records)
    {
        const QString name =
            record.chineseName.trimmed().isEmpty() ? record.japaneseName : record.chineseName;
        combo->addItem(name, record.id);
    }
    combo->setCurrentIndex(selectedId.has_value() ? combo->findData(*selectedId) : 0);
    if (combo->currentIndex() < 0)
        combo->setCurrentIndex(0);
}

} // namespace darkeye
