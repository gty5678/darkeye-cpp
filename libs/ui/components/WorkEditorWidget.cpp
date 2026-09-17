#include "ui/components/WorkEditorWidget.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "ui/components/IdCheckList.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenControls.h"
#include "ui/components/CrawlerFieldSelector.h"
#include "ui/components/FanartStripWidget.h"
#include "ui/components/ImageDropWidget.h"
#include "ui/components/WikiTextEdit.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <utility>

namespace darkeye
{

WorkEditorWidget::WorkEditorWidget(QSqlDatabase database, ThemeService &themes,
                                   QString coverDirectory, QString fanartDirectory,
                                   QUrl imageFetchEndpoint, QWidget *parent)
    : QWidget(parent), m_repository(database), m_references(database),
      m_people(std::move(database)), m_themes(themes), m_coverDirectory(std::move(coverDirectory)),
      m_fanartDirectory(std::move(fanartDirectory)),
      m_imageFetchEndpoint(std::move(imageFetchEndpoint))
{
    if (m_fanartDirectory.isEmpty() && !m_coverDirectory.isEmpty())
        m_fanartDirectory =
            QDir(QFileInfo(m_coverDirectory).absolutePath()).filePath(QStringLiteral("fanart"));
    setObjectName(QStringLiteral("WorkEditorWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    m_imageDrop = new ImageDropWidget(m_coverDirectory, this);
    m_imageDrop->setObjectName(QStringLiteral("WorkCoverDropWidget"));
    m_imageDrop->setPurpose(QStringLiteral("作品封面"));
    m_imageDrop->setMaximumSize(240, 280);
    root->addWidget(m_imageDrop, 0, Qt::AlignHCenter);

    auto *form = new QFormLayout;
    m_serialNumber = new DesignLineEdit(this);
    m_serialNumber->setObjectName(QStringLiteral("WorkSerialInput"));
    m_chineseTitle = new DesignLineEdit(this);
    m_chineseTitle->setObjectName(QStringLiteral("WorkChineseTitleInput"));
    m_japaneseTitle = new DesignLineEdit(this);
    m_japaneseTitle->setObjectName(QStringLiteral("WorkJapaneseTitleInput"));
    m_director = new DesignLineEdit(this);
    m_director->setObjectName(QStringLiteral("WorkDirectorInput"));
    m_releaseDate = new DesignLineEdit(this);
    m_releaseDate->setObjectName(QStringLiteral("WorkReleaseDateInput"));
    m_releaseDate->setPlaceholderText(QStringLiteral("YYYY-MM-DD"));
    m_runtime = new TokenSpinBox(this);
    m_runtime->setObjectName(QStringLiteral("WorkRuntimeInput"));
    m_runtime->setRange(0, 24 * 60);
    m_runtime->setSpecialValueText(QStringLiteral("未知"));
    m_runtime->setSuffix(QStringLiteral(" 分钟"));
    m_imageUrl = new DesignLineEdit(this);
    m_imageUrl->setObjectName(QStringLiteral("WorkImageUrlInput"));
    m_videoUrl = new DesignLineEdit(this);
    m_videoUrl->setObjectName(QStringLiteral("WorkVideoUrlInput"));
    m_maker = new DesignComboBox(this);
    m_maker->setObjectName(QStringLiteral("WorkMakerSelector"));
    m_label = new DesignComboBox(this);
    m_label->setObjectName(QStringLiteral("WorkLabelSelector"));
    m_series = new DesignComboBox(this);
    m_series->setObjectName(QStringLiteral("WorkSeriesSelector"));
    m_notes = new WikiTextEdit(this);
    m_notes->setObjectName(QStringLiteral("WorkNotesInput"));
    m_notes->setCompleterList(m_repository.serialSuggestions());
    m_notes->setWorkIdResolver([this](const QString &serial) {
        return m_repository.findIdBySerial(serial);
    });
    m_notes->setImageResolver([this](const QString &serial) {
        const auto id = m_repository.findIdBySerial(serial);
        if (!id) return QString();
        const auto work = m_repository.findById(*id);
        if (!work || work->imageUrl.trimmed().isEmpty()) return QString();
        return QFileInfo(work->imageUrl).isAbsolute()
            ? work->imageUrl : QDir(m_coverDirectory).filePath(work->imageUrl);
    });
    connect(m_notes, &WikiTextEdit::workLinkRequested,
            this, &WorkEditorWidget::workLinkRequested);
    connect(m_notes, &WikiTextEdit::actressLinkRequested,
            this, &WorkEditorWidget::actressLinkRequested);
    m_chineseStory = new DesignPlainTextEdit(this);
    m_chineseStory->setObjectName(QStringLiteral("WorkChineseStoryInput"));
    m_japaneseStory = new DesignPlainTextEdit(this);
    m_japaneseStory->setObjectName(QStringLiteral("WorkJapaneseStoryInput"));
    m_notes->setMaximumHeight(80);
    for (QPlainTextEdit *edit : {m_chineseStory, m_japaneseStory}) edit->setMaximumHeight(80);
    form->addRow(QStringLiteral("番号"), m_serialNumber);
    form->addRow(QStringLiteral("中文标题"), m_chineseTitle);
    form->addRow(QStringLiteral("日文标题"), m_japaneseTitle);
    form->addRow(QStringLiteral("导演"), m_director);
    form->addRow(QStringLiteral("发布日期"), m_releaseDate);
    form->addRow(QStringLiteral("时长"), m_runtime);
    form->addRow(QStringLiteral("片商"), m_maker);
    form->addRow(QStringLiteral("厂牌"), m_label);
    form->addRow(QStringLiteral("系列"), m_series);
    form->addRow(QStringLiteral("封面地址"), m_imageUrl);
    form->addRow(QStringLiteral("视频地址"), m_videoUrl);
    form->addRow(QStringLiteral("备注"), m_notes);
    form->addRow(QStringLiteral("中文简介"), m_chineseStory);
    form->addRow(QStringLiteral("日文简介"), m_japaneseStory);
    root->addLayout(form);

    auto *relations = new TokenTabWidget(this);
    relations->setObjectName(QStringLiteral("WorkRelationTabs"));
    relations->setMinimumHeight(180);
    m_actresses = new IdCheckList(QStringLiteral("已选女优"), relations);
    m_actresses->setObjectName(QStringLiteral("WorkActressSelector"));
    m_actors = new IdCheckList(QStringLiteral("已选男优"), relations);
    m_actors->setObjectName(QStringLiteral("WorkActorSelector"));
    m_tags = new IdCheckList(QStringLiteral("已选标签"), relations);
    m_tags->setObjectName(QStringLiteral("WorkTagEditorSelector"));
    m_fanart =
        new FanartStripWidget(m_fanartDirectory, m_coverDirectory, m_imageFetchEndpoint, relations);
    m_fanart->setObjectName(QStringLiteral("WorkFanartStrip"));
    m_fanart->setCanAdd(false);
    m_crawlerFields = new CrawlerFieldSelector(relations);
    m_crawlerFields->setObjectName(QStringLiteral("WorkCrawlerFieldSelector"));
    auto *crawlButton = new DesignButton(QStringLiteral("采集所选字段"), m_crawlerFields);
    crawlButton->setObjectName(QStringLiteral("WorkCrawlerFetchButton"));
    crawlButton->setToolTip(QStringLiteral("等待 Collector 作品采集客户端迁移"));
    crawlButton->setEnabled(false);
    m_crawlerFields->appendRowWidget(crawlButton, 1);
    relations->addTab(m_actresses, QStringLiteral("女优"));
    relations->addTab(m_actors, QStringLiteral("男优"));
    relations->addTab(m_tags, QStringLiteral("标签"));
    relations->addTab(m_fanart, QStringLiteral("剧照"));
    relations->addTab(m_crawlerFields, QStringLiteral("采集字段"));
    root->addWidget(relations, 1);

    auto *buttons = new QHBoxLayout;
    auto *clearButton = new DesignButton(QStringLiteral("清空"), this);
    clearButton->setObjectName(QStringLiteral("WorkEditorClearButton"));
    auto *saveButton = new DesignButton(QStringLiteral("保存作品"), this);
    saveButton->setObjectName(QStringLiteral("WorkSaveButton"));
    saveButton->setVariant(QStringLiteral("primary"));
    buttons->addStretch();
    buttons->addWidget(clearButton);
    buttons->addWidget(saveButton);
    root->addLayout(buttons);
    connect(clearButton, &QPushButton::clicked, this, &WorkEditorWidget::beginCreate);
    connect(saveButton, &QPushButton::clicked, this, &WorkEditorWidget::save);
    connect(m_imageDrop, &ImageDropWidget::imageChanged, m_imageUrl, &QLineEdit::setText);
    connect(m_imageDrop, &ImageDropWidget::imageRejected, this,
            [this](const QString &message) { Toast::showError(window(), message, &m_themes); });
    connect(m_imageUrl, &QLineEdit::editingFinished, this,
            [this]() { m_imageDrop->setImagePath(m_imageUrl->text()); });
    connect(m_serialNumber, &QLineEdit::textChanged, this,
            [this](const QString &serial) { m_fanart->setCanAdd(!serial.trimmed().isEmpty()); });
    connect(m_fanart, &FanartStripWidget::fanartChanged, this,
            [this](const QList<FanartEntry> &) { m_fanartDirty = true; });
    connect(m_fanart, &FanartStripWidget::imageRejected, this,
            [this](const QString &message) { Toast::showError(window(), message, &m_themes); });
    refreshReferences();
    m_currentWork = Work{};
    clearEditor();
    m_serialNumber->setReadOnly(false);
}

void WorkEditorWidget::beginCreate()
{
    if (!m_associationsLoaded)
        refreshAssociations();
    m_currentWork = Work{};
    clearEditor();
    m_serialNumber->setReadOnly(false);
    m_serialNumber->setFocus();
}

bool WorkEditorWidget::loadWork(qint64 workId)
{
    if (!m_associationsLoaded)
        refreshAssociations();
    QString errorMessage;
    const std::optional<WorkDetails> details = m_repository.findDetailsById(workId, &errorMessage);
    if (!details.has_value())
    {
        Toast::showError(window(),
                         errorMessage.isEmpty() ? QStringLiteral("作品不存在") : errorMessage,
                         &m_themes);
        return false;
    }
    m_currentWork = details->work;
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
    m_serialNumber->setReadOnly(true);
    return true;
}

void WorkEditorWidget::refreshAssociations()
{
    const auto loadPeople = [this](PersonKind kind)
    {
        PersonSearch search;
        search.kind = kind;
        QString errorMessage;
        const std::optional<int> count = m_people.count(search, &errorMessage);
        if (!count.has_value())
        {
            Toast::showError(window(), errorMessage, &m_themes);
            return QList<IdLabelOption>{};
        }
        search.limit = qMax(1, *count);
        QList<IdLabelOption> options;
        for (const PersonSummary &person : m_people.search(search, &errorMessage))
            options.append({person.id, person.name, {}});
        if (!errorMessage.isEmpty())
            Toast::showError(window(), errorMessage, &m_themes);
        return options;
    };
    m_actresses->setOptions(loadPeople(PersonKind::Actress));
    m_actors->setOptions(loadPeople(PersonKind::Actor));
    QList<IdLabelOption> tags;
    for (const TagOption &tag : m_repository.tagOptions())
        tags.append({tag.id, tag.name, tag.typeName});
    m_tags->setOptions(tags);
    m_associationsLoaded = true;
}

void WorkEditorWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_associationsLoaded)
        refreshAssociations();
}

void WorkEditorWidget::refreshReferences()
{
    populateReferenceCombo(m_maker, ReferenceKind::Maker,
                           m_currentWork.has_value() ? m_currentWork->makerId : std::nullopt);
    populateReferenceCombo(m_label, ReferenceKind::Label,
                           m_currentWork.has_value() ? m_currentWork->labelId : std::nullopt);
    populateReferenceCombo(m_series, ReferenceKind::Series,
                           m_currentWork.has_value() ? m_currentWork->seriesId : std::nullopt);
}

QString WorkEditorWidget::currentSerialNumber() const
{
    return m_serialNumber->text().trimmed();
}

void WorkEditorWidget::applyWork(const Work &work)
{
    m_serialNumber->setText(work.serialNumber);
    m_chineseTitle->setText(work.chineseTitle);
    m_japaneseTitle->setText(work.japaneseTitle);
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

Work WorkEditorWidget::editorWork() const
{
    Work work = m_currentWork.value_or(Work{});
    work.serialNumber = m_serialNumber->text().trimmed();
    work.chineseTitle = m_chineseTitle->text();
    work.japaneseTitle = m_japaneseTitle->text();
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

void WorkEditorWidget::save()
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
        const auto inserted =
            m_repository.insertComplete(work, m_actresses->selectedIds(), m_actors->selectedIds(),
                                        m_tags->selectedIds(), &errorMessage);
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
    else if (!m_repository.updateComplete(work, m_actresses->selectedIds(), m_actors->selectedIds(),
                                          m_tags->selectedIds(), &errorMessage))
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
    emit workSaved(workId, created);
    Toast::showSuccess(window(),
                       created ? QStringLiteral("作品添加成功") : QStringLiteral("作品信息已保存"),
                       &m_themes);
}

void WorkEditorWidget::clearEditor()
{
    for (QLineEdit *edit : {m_serialNumber, m_chineseTitle, m_japaneseTitle, m_director,
                            m_releaseDate, m_imageUrl, m_videoUrl})
        edit->clear();
    m_notes->clear();
    for (QPlainTextEdit *edit : {m_chineseStory, m_japaneseStory}) edit->clear();
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
}

void WorkEditorWidget::populateReferenceCombo(QComboBox *combo, ReferenceKind kind,
                                              std::optional<qint64> selectedId)
{
    QString errorMessage;
    const QList<ReferenceRecord> records = m_references.list(kind, &errorMessage);
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

