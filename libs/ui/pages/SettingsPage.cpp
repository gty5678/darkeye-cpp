#include "ui/pages/SettingsPage.h"

#include "darkeye_ui/components/AnimatedIndicators.h"
#include "darkeye_ui/components/ColorPicker.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/LinkCard.h"
#include "darkeye_ui/components/ModernScrollMenu.h"
#include "darkeye_ui/components/TokenControls.h"
#include "ui/components/PathManagement.h"
#include "darkeye_ui/components/TokenViews.h"
#include "services/VideoLibraryService.h"

#include <QAction>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMessageBox>
#include <QSaveFile>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QUrl>

#include <tuple>
#include <utility>

namespace darkeye
{
namespace
{

QWidget *pendingSettingsPage(const QString &name, QWidget *parent)
{
    auto *page = new QWidget(parent);
    page->setObjectName(name + QStringLiteral("SettingsPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *label = new DesignLabel(QStringLiteral("该设置页正在逐项迁移"), page);
    label->setTone(QStringLiteral("muted"));
    layout->addWidget(label);
    layout->addStretch();
    return page;
}

} // namespace

AboutSettingsPage::AboutSettingsPage(ThemeService &themeService, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("AboutSettingsPage"));
    auto *layout = new QVBoxLayout(this);

    auto *versionRow = new QHBoxLayout;
    auto *version = new DesignLabel(
        QStringLiteral("当前版本 %1").arg(QStringLiteral(DARKEYE_VERSION)), this);
    version->setObjectName(QStringLiteral("AboutVersionLabel"));
    versionRow->addWidget(version);

    auto *checkUpdate = new DesignButton(QStringLiteral("检查更新"), this);
    checkUpdate->setObjectName(QStringLiteral("CheckUpdateButton"));
    checkUpdate->setEnabled(false);
    checkUpdate->setToolTip(QStringLiteral("等待 C++ 更新服务迁移"));
    versionRow->addWidget(checkUpdate);

    auto *localUpdate = new DesignButton(QStringLiteral("使用本地安装包更新…"), this);
    localUpdate->setObjectName(QStringLiteral("LocalPackageUpdateButton"));
    localUpdate->setEnabled(false);
    localUpdate->setToolTip(QStringLiteral("等待 C++ 更新程序迁移"));
    versionRow->addWidget(localUpdate);

    const auto addExternalButton = [this, versionRow](const QString &text,
                                                       const QString &objectName,
                                                       const QString &url)
    {
        auto *button = new DesignButton(text, this);
        button->setObjectName(objectName);
        connect(button, &QPushButton::clicked, this,
                [url] { QDesktopServices::openUrl(QUrl(url)); });
        versionRow->addWidget(button);
    };
    addExternalButton(QStringLiteral("意见反馈"), QStringLiteral("FeedbackButton"),
                      QStringLiteral("https://github.com/de4321/darkeye/issues"));
    addExternalButton(QStringLiteral("版本记录"), QStringLiteral("ChangelogButton"),
                      QStringLiteral("https://de4321.github.io/darkeye/CHANGELOG/"));
    versionRow->addStretch();
    layout->addLayout(versionRow);

    auto *updateOptions = new QHBoxLayout;
    auto *automaticUpdate = new TokenRadioButton(QStringLiteral("自动更新"), this);
    automaticUpdate->setObjectName(QStringLiteral("AutomaticUpdateOption"));
    automaticUpdate->setEnabled(false);
    auto *updateNotification =
        new TokenRadioButton(QStringLiteral("有新版本时提醒我"), this);
    updateNotification->setObjectName(QStringLiteral("UpdateNotificationOption"));
    updateNotification->setEnabled(false);
    updateOptions->addWidget(automaticUpdate);
    updateOptions->addWidget(updateNotification);
    updateOptions->addStretch();
    layout->addLayout(updateOptions);

    auto *downloadRow = new QHBoxLayout;
    downloadRow->addWidget(new DesignLabel(QStringLiteral("下载移动客户端"), this));
    auto *android = new DesignButton(QStringLiteral("Android 版"), this);
    android->setObjectName(QStringLiteral("AndroidDownloadButton"));
    android->setEnabled(false);
    downloadRow->addWidget(android);
    downloadRow->addSpacing(16);
    downloadRow->addWidget(new DesignLabel(QStringLiteral("下载浏览器插件"), this));
    const auto addDownloadButton = [this, downloadRow](const QString &text,
                                                        const QString &objectName)
    {
        auto *button = new DesignButton(text, this);
        button->setObjectName(objectName);
        connect(button, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(
                QUrl(QStringLiteral("https://github.com/de4321/darkeye/releases")));
        });
        downloadRow->addWidget(button);
    };
    addDownloadButton(QStringLiteral("Firefox 插件"), QStringLiteral("FirefoxPluginButton"));
    addDownloadButton(QStringLiteral("Chrome/Edge 插件"),
                      QStringLiteral("ChromiumPluginButton"));
    downloadRow->addStretch();
    layout->addLayout(downloadRow);

    auto *links = new QHBoxLayout;
    auto *projectLinks = new QVBoxLayout;
    projectLinks->addWidget(new DesignLabel(QStringLiteral("项目链接"), this));
    const QList<std::tuple<QString, QString, QString>> projects = {
        {QStringLiteral("GitHub"), QStringLiteral("源代码仓库与问题反馈"),
         QStringLiteral("https://github.com/de4321/darkeye")},
        {QStringLiteral("Discord"), QStringLiteral("社区讨论与支持频道"),
         QStringLiteral("https://discord.gg/N7wJVNVA")},
        {QStringLiteral("官网"), QStringLiteral("产品介绍与主页"),
         QStringLiteral("https://de4321.github.io/darkeye-webpage/")},
        {QStringLiteral("文档"), QStringLiteral("使用说明与开发文档"),
         QStringLiteral("https://de4321.github.io/darkeye/")},
    };
    for (const auto &[title, description, url] : projects)
    {
        projectLinks->addWidget(new TokenLinkCard(title, description, url, &themeService, this));
    }

    auto *referenceLinks = new QVBoxLayout;
    referenceLinks->addWidget(new DesignLabel(QStringLiteral("参考项目"), this));
    const QList<std::tuple<QString, QString, QString>> references = {
        {QStringLiteral("mdcz"), QStringLiteral("开源媒体库元数据刮削与管理"),
         QStringLiteral("https://github.com/ShotHeadman/mdcz")},
        {QStringLiteral("Jvedio"), QStringLiteral("Windows 本地影片管理与刮削工具"),
         QStringLiteral("https://github.com/hitchao/Jvedio")},
        {QStringLiteral("JavSP"), QStringLiteral("JAV 刮削工具"),
         QStringLiteral("https://github.com/Yuukiy/JavSP")},
        {QStringLiteral("JAV-JHS"), QStringLiteral("油猴脚本，站点体验增强"),
         QStringLiteral("https://sleazyfork.org/zh-CN/scripts/558525-jav-jhs")},
    };
    for (const auto &[title, description, url] : references)
    {
        referenceLinks->addWidget(new TokenLinkCard(title, description, url, &themeService, this));
    }
    links->addLayout(projectLinks);
    links->addLayout(referenceLinks);
    links->addStretch();
    layout->addLayout(links);
    layout->addStretch();
}

VideoSettingsPage::VideoSettingsPage(Settings &settings, QSqlDatabase publicDatabase,
                                     QWidget *parent)
    : QWidget(parent), m_settings(settings), m_publicDatabase(std::move(publicDatabase))
{
    setObjectName(QStringLiteral("VideoSettingsPage"));
    auto *layout = new QVBoxLayout(this);

    auto *playerRow = new QHBoxLayout;
    playerRow->addWidget(new DesignLabel(QStringLiteral("本地播放器（可选）："), this));
    m_player = new DesignLineEdit(this);
    m_player->setObjectName(QStringLiteral("LocalVideoPlayerEdit"));
    m_player->setPlaceholderText(
        QStringLiteral("留空则使用系统默认程序；书架/DVD 与作品页播放本地文件时生效"));
    m_player->setClearButtonEnabled(true);
    const AppSettings appSettings = m_settings.app();
    m_player->setText(appSettings.localVideoPlayer);
    playerRow->addWidget(m_player, 1);
    auto *browse = new DesignButton(QStringLiteral("浏览…"), this);
    browse->setObjectName(QStringLiteral("BrowseLocalVideoPlayerButton"));
    browse->setToolTip(QStringLiteral("选择播放器可执行文件（如 VLC、MPC-HC 等）"));
    playerRow->addWidget(browse);
    layout->addLayout(playerRow);

    m_paths = new MultiplePathManagement(QStringLiteral("视频文件夹路径管理："), this);
    m_paths->setObjectName(QStringLiteral("VideoPathManagement"));
    m_paths->setMinimumHeight(300);
    m_paths->loadPaths(appSettings.videoPaths);
    layout->addWidget(m_paths);

    auto *scan = new DesignButton(QStringLiteral("扫描本地视频提取番号并录入数据库"), this);
    scan->setObjectName(QStringLiteral("ScanLocalVideosButton"));
    scan->setToolTip(
        QStringLiteral("扫描本地视频的路径下的所有视频，并提取视频番号，将没有的番号尝试去抓取信息"));
    layout->addWidget(scan);
    auto *match = new DesignButton(QStringLiteral("同步作品本地视频路径"), this);
    match->setObjectName(QStringLiteral("MatchLocalVideosButton"));
    match->setToolTip(QStringLiteral(
        "扫描已配置文件夹中的视频，从文件名提取番号并与库中作品匹配，"
        "将匹配到的本地绝对路径写入作品表的 video_url（多条英文逗号分隔、去重）；"
        "以本次扫描结果为准完全覆盖，不保留库中旧路径"));
    layout->addWidget(match);

    connect(m_player, &QLineEdit::editingFinished, this, &VideoSettingsPage::savePlayer);
    connect(browse, &QPushButton::clicked, this, &VideoSettingsPage::browsePlayer);
    connect(m_paths->table(), &QTableWidget::itemChanged, this,
            &VideoSettingsPage::savePaths);
    connect(m_paths->findChild<QPushButton *>(QStringLiteral("MultiplePathAddButton")),
            &QPushButton::clicked, this, &VideoSettingsPage::savePaths);
    connect(m_paths->findChild<QPushButton *>(QStringLiteral("MultiplePathDeleteButton")),
            &QPushButton::clicked, this, &VideoSettingsPage::savePaths);
    connect(scan, &QPushButton::clicked, this, &VideoSettingsPage::scanMissingSerials);
    connect(match, &QPushButton::clicked, this, &VideoSettingsPage::synchronizeVideoUrls);
}

void VideoSettingsPage::savePlayer()
{
    AppSettings settings = m_settings.app();
    settings.localVideoPlayer = m_player->text().trimmed();
    m_settings.saveApp(settings);
}

void VideoSettingsPage::savePaths()
{
    QStringList paths;
    for (const QString &path : m_paths->paths())
    {
        const QString normalized = path.trimmed();
        if (!normalized.isEmpty() && normalized != QStringLiteral("."))
        {
            paths.append(normalized);
        }
    }
    AppSettings settings = m_settings.app();
    settings.videoPaths = paths;
    m_settings.saveApp(settings);
}

void VideoSettingsPage::browsePlayer()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择播放器可执行文件"), QString{},
        QStringLiteral("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (!path.isEmpty())
    {
        m_player->setText(path);
        savePlayer();
    }
}

QStringList VideoSettingsPage::configuredPaths() const
{
    QStringList paths;
    for (const QString &path : m_paths->paths())
    {
        const QString normalized = path.trimmed();
        if (!normalized.isEmpty() && normalized != QStringLiteral("."))
            paths.append(normalized);
    }
    return paths;
}

void VideoSettingsPage::showFilesWithoutSerial(
    const QList<QPair<QString, QString>> &entries)
{
    if (entries.isEmpty()) return;
    constexpr qsizetype limit = 80;
    QStringList lines;
    for (qsizetype index = 0; index < qMin(limit, entries.size()); ++index)
        lines.append(entries.at(index).first + QLatin1Char('\n') + entries.at(index).second);
    QString suffix;
    if (entries.size() > limit)
        suffix = QStringLiteral("\n\n… 另有 %1 条未列出（共 %2 个文件）")
                     .arg(entries.size() - limit).arg(entries.size());
    QMessageBox::warning(this, QStringLiteral("无法提取番号"),
                         QStringLiteral("以下视频未能从文件名识别番号：\n\n")
                             + lines.join(QStringLiteral("\n\n")) + suffix);
}

void VideoSettingsPage::scanMissingSerials()
{
    const QStringList paths = configuredPaths();
    if (paths.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在上方配置至少一个视频文件夹路径"));
        return;
    }
    const VideoLibraryScanResult result =
        VideoLibraryService(m_publicDatabase).scanMissingSerials(paths);
    if (!result.succeeded)
    {
        QMessageBox::critical(this, QStringLiteral("扫描失败"), result.errorMessage);
        return;
    }
    showFilesWithoutSerial(result.filesWithoutSerial);
    if (result.missingSerials.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("本地视频的番号均已存在于数据库中"));
        return;
    }
    QMessageBox::information(
        this, QStringLiteral("发现数据库缺失番号"),
        QStringLiteral("共扫描 %1 个视频，发现 %2 个待添加番号：\n\n%3\n\n"
                       "批量采集对话框迁移完成后可从这里继续录入。")
            .arg(result.scannedFiles).arg(result.missingSerials.size())
            .arg(result.missingSerials.join(QStringLiteral("\n"))));
}

void VideoSettingsPage::synchronizeVideoUrls()
{
    const QStringList paths = configuredPaths();
    if (paths.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在上方配置至少一个视频文件夹路径"));
        return;
    }
    const VideoLibraryScanResult result =
        VideoLibraryService(m_publicDatabase).synchronizeVideoUrls(paths);
    if (!result.succeeded)
    {
        QMessageBox::critical(this, QStringLiteral("同步失败"), result.errorMessage);
        return;
    }
    showFilesWithoutSerial(result.filesWithoutSerial);
    if (result.updatedWorks > 0) emit worksChanged();
    QMessageBox::information(
        this, QStringLiteral("完成"),
        QStringLiteral("共扫描 %1 个视频文件；无法提取番号 %2 个；"
                       "番号在库中无匹配 %3 个；已更新/清理 %4 条作品的 video_url。")
            .arg(result.scannedFiles).arg(result.filesWithoutSerial.size())
            .arg(result.unmatchedSerials).arg(result.updatedWorks));
}

ShortcutSettingsPage::ShortcutSettingsPage(const QString &shortcutsFile, QWidget *parent)
    : QWidget(parent), m_shortcutsFile(shortcutsFile)
{
    setObjectName(QStringLiteral("ShortcutSettingsPage"));
    QFile file(m_shortcutsFile);
    if (file.open(QIODevice::ReadOnly))
    {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error == QJsonParseError::NoError && document.isObject())
        {
            m_userShortcuts = document.object();
        }
    }

    struct ShortcutDefinition
    {
        QString id;
        QString name;
        QString key;
    };
    const QList<ShortcutDefinition> definitions = {
        {QStringLiteral("add_masturbation_record"), QStringLiteral("添加撸管记录"),
         QStringLiteral("M")},
        {QStringLiteral("add_quick_work"), QStringLiteral("快速添加番号"), QStringLiteral("W")},
        {QStringLiteral("add_makelove_record"), QStringLiteral("添加做爱记录"),
         QStringLiteral("L")},
        {QStringLiteral("add_sexual_rousal_record"), QStringLiteral("添加晨勃记录"),
         QStringLiteral("A")},
        {QStringLiteral("open_help"), QStringLiteral("打开文档"), QStringLiteral("H")},
        {QStringLiteral("search"), QStringLiteral("搜索"), QStringLiteral("Ctrl+F")},
        {QStringLiteral("capture"), QStringLiteral("部分截图"), QStringLiteral("C")},
        {QStringLiteral("allcapture"), QStringLiteral("全软件截图"), QStringLiteral("Shift+C")},
    };

    auto *layout = new QVBoxLayout(this);
    for (const auto &definition : definitions)
    {
        auto *row = new QWidget(this);
        row->setObjectName(QStringLiteral("ShortcutSettingRow_%1").arg(definition.id));
        auto *rowLayout = new QHBoxLayout(row);
        auto *label = new DesignLabel(definition.name, row);
        label->setFixedWidth(100);
        auto *editor = new TokenKeySequenceEdit(row);
        editor->setObjectName(QStringLiteral("ShortcutEditor_%1").arg(definition.id));
        editor->setFixedWidth(150);
        editor->setKeySequence(QKeySequence(
            m_userShortcuts.value(definition.id).toString(definition.key)));
        auto *reset = new DesignButton(QStringLiteral("恢复"), row);
        reset->setObjectName(QStringLiteral("ShortcutReset_%1").arg(definition.id));
        reset->setFixedWidth(50);
        rowLayout->addWidget(label);
        rowLayout->addWidget(editor);
        rowLayout->addWidget(reset);
        rowLayout->addStretch();
        layout->addWidget(row);
        connect(editor, &QKeySequenceEdit::editingFinished, this,
                [this, id = definition.id, editor] { applyShortcut(id, editor); });
        connect(reset, &QPushButton::clicked, this,
                [this, id = definition.id, key = definition.key, editor] {
                    resetShortcut(id, key, editor);
                });
    }
    layout->addStretch();
    layout->addWidget(
        new DesignLabel(QStringLiteral("<small>配置将自动保存到 data/shortcuts.json</small>"),
                        this));
}

void ShortcutSettingsPage::applyShortcut(const QString &actionId,
                                         TokenKeySequenceEdit *editor)
{
    const QString shortcut = editor->keySequence().toString();
    m_userShortcuts.insert(actionId, shortcut);
    save();
    if (auto *action = window()->findChild<QAction *>(actionId))
    {
        action->setShortcut(QKeySequence(shortcut));
    }
}

void ShortcutSettingsPage::resetShortcut(const QString &actionId, const QString &defaultKey,
                                         TokenKeySequenceEdit *editor)
{
    m_userShortcuts.remove(actionId);
    save();
    editor->setKeySequence(QKeySequence(defaultKey));
    if (auto *action = window()->findChild<QAction *>(actionId))
    {
        action->setShortcut(QKeySequence(defaultKey));
    }
}

void ShortcutSettingsPage::save() const
{
    QSaveFile file(m_shortcutsFile);
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    file.write(QJsonDocument(m_userShortcuts).toJson(QJsonDocument::Indented));
    file.commit();
}

CommonSettingsPage::CommonSettingsPage(ThemeService &themeService, Settings &settings,
                                       QWidget *parent)
    : QWidget(parent), m_themeService(themeService), m_settings(settings)
{
    setObjectName(QStringLiteral("CommonSettingsPage"));
    auto *layout = new QFormLayout(this);

    m_primaryColorRow = new QWidget(this);
    m_primaryColorRow->setObjectName(QStringLiteral("PrimaryColorRow"));
    auto *primaryLayout = new QHBoxLayout(m_primaryColorRow);
    primaryLayout->setContentsMargins(0, 0, 0, 0);
    const QString initialPrimary = m_themeService.customPrimary().isEmpty()
        ? ThemeService::tokens(m_themeService.current()).primary
        : m_themeService.customPrimary();
    m_colorPicker = new ColorPicker(QColor(initialPrimary), false, ColorPicker::Shape::Circle,
                                    m_primaryColorRow);
    m_colorPicker->setObjectName(QStringLiteral("primaryColorPicker"));
    primaryLayout->addWidget(m_colorPicker);
    primaryLayout->addStretch();

    m_themeSelector = new DesignComboBox(this);
    m_themeSelector->setObjectName(QStringLiteral("themeSelector"));
    m_themeSelector->setAccessibleName(QStringLiteral("主题"));

    m_greenMode = new ToggleSwitch(48, 24, &m_themeService, this);
    m_greenMode->setObjectName(QStringLiteral("greenModeSwitch"));
    m_greenMode->setChecked(m_settings.app().greenMode);

    layout->addRow(new DesignLabel(QStringLiteral("主色"), this), m_primaryColorRow);
    layout->addRow(new DesignLabel(QStringLiteral("主题"), this), m_themeSelector);
    layout->addRow(new DesignLabel(QStringLiteral("绿色模式"), this), m_greenMode);

    connect(m_colorPicker, &ColorPicker::colorConfirmed, this,
            &CommonSettingsPage::savePrimaryColor);
    connect(&m_themeService, &ThemeService::themeChanged, this,
            [this](ThemeId) { updatePrimaryPickerState(); });
    connect(m_greenMode, &ToggleSwitch::toggled, this, [this](bool enabled) {
        AppSettings settings = m_settings.app();
        settings.greenMode = enabled;
        m_settings.saveApp(settings);
        emit greenModeChanged(enabled);
    });
    updatePrimaryPickerState();
}

QComboBox *CommonSettingsPage::themeSelector() const
{
    return m_themeSelector;
}

void CommonSettingsPage::updatePrimaryPickerState()
{
    const ThemeId theme = m_themeService.current();
    const bool supportsCustomPrimary = theme == ThemeId::Light || theme == ThemeId::Dark;
    m_primaryColorRow->setEnabled(supportsCustomPrimary);
    if (!supportsCustomPrimary)
    {
        AppSettings settings = m_settings.app();
        settings.customPrimary.clear();
        m_settings.saveApp(settings);
        return;
    }
    const QString color = m_themeService.customPrimary().isEmpty()
        ? ThemeService::tokens(theme).primary
        : m_themeService.customPrimary();
    m_colorPicker->setColor(color);
}

void CommonSettingsPage::savePrimaryColor(const QString &color)
{
    if (m_themeService.current() != ThemeId::Light && m_themeService.current() != ThemeId::Dark)
    {
        return;
    }
    m_themeService.setTheme(m_themeService.current(), color);
    AppSettings settings = m_settings.app();
    settings.customPrimary = color;
    m_settings.saveApp(settings);
}

CrawlerSettingsPage::CrawlerSettingsPage(Settings &settings, QWidget *parent)
    : QWidget(parent), m_settings(settings)
{
    setObjectName(QStringLiteral("CrawlerSettingsPage"));
    const CrawlerSettings values = m_settings.crawler();
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new DesignLabel(QStringLiteral("<h3>信息补充器相关设置</h3>"), this));
    auto *form = new QFormLayout;

    const auto addUrlRow = [this, form](const QString &label, const QString &objectName,
                                        const QUrl &value, QLineEdit **field) {
        *field = new DesignLineEdit(this);
        (*field)->setObjectName(objectName);
        (*field)->setText(value.toString());
        (*field)->setClearButtonEnabled(true);
        form->addRow(new DesignLabel(label, this), *field);
        connect(*field, &QLineEdit::editingFinished, this, &CrawlerSettingsPage::save);
    };
    addUrlRow(QStringLiteral("作品 API 前缀"), QStringLiteral("CrawlerWorkApiEdit"),
              values.workApiBaseUrl, &m_workApi);
    addUrlRow(QStringLiteral("女优 API 前缀"), QStringLiteral("CrawlerActressApiEdit"),
              values.actressApiBaseUrl, &m_actressApi);
    addUrlRow(QStringLiteral("图片下载 API"), QStringLiteral("CrawlerCoverApiEdit"),
              values.coverFetchApiUrl, &m_coverApi);
    addUrlRow(QStringLiteral("热门女优 API"), QStringLiteral("CrawlerTopActressesApiEdit"),
              values.topActressesApiUrl, &m_topActressesApi);
    form->addRow(new DesignLabel(QStringLiteral("说明"), this),
                 new DesignLabel(QStringLiteral("作品/女优为完整前缀，程序会追加 /{serial} 或 /{name}；"
                                                 "图片下载、热门女优请填写完整地址。"), this));

    m_collectorExecutable = new DesignLineEdit(this);
    m_collectorExecutable->setObjectName(QStringLiteral("CollectorExecutableEdit"));
    m_collectorExecutable->setText(values.collectorExecutable);
    m_collectorExecutable->setPlaceholderText(QStringLiteral("可选：信息补充器的可执行文件"));
    m_collectorExecutable->setClearButtonEnabled(true);
    auto *collectorRow = new QHBoxLayout;
    collectorRow->addWidget(m_collectorExecutable, 1);
    auto *browse = new DesignButton(QStringLiteral("浏览…"), this);
    browse->setObjectName(QStringLiteral("BrowseCollectorExecutableButton"));
    collectorRow->addWidget(browse);
    form->addRow(new DesignLabel(QStringLiteral("信息补充器可执行文件"), this), collectorRow);
    m_autoStartCollector = new ToggleSwitch(48, 24, nullptr, this);
    m_autoStartCollector->setObjectName(QStringLiteral("CollectorAutoStartSwitch"));
    m_autoStartCollector->setChecked(values.autoStartCollector);
    form->addRow(new DesignLabel(QStringLiteral("打开软件自动启动信息补充器"), this),
                 m_autoStartCollector);
    layout->addLayout(form);
    layout->addWidget(new DesignLabel(
        QStringLiteral("此处仅保存外部信息补充器的地址和启动选项，不会在应用内创建服务器。"),
        this));
    layout->addStretch();

    connect(m_collectorExecutable, &QLineEdit::editingFinished, this, &CrawlerSettingsPage::save);
    connect(browse, &QPushButton::clicked, this, &CrawlerSettingsPage::browseCollector);
    connect(m_autoStartCollector, &ToggleSwitch::toggled, this,
            [this](bool) { save(); });
}

void CrawlerSettingsPage::save()
{
    CrawlerSettings values = m_settings.crawler();
    values.workApiBaseUrl = QUrl::fromUserInput(m_workApi->text().trimmed());
    values.actressApiBaseUrl = QUrl::fromUserInput(m_actressApi->text().trimmed());
    values.coverFetchApiUrl = QUrl::fromUserInput(m_coverApi->text().trimmed());
    values.topActressesApiUrl = QUrl::fromUserInput(m_topActressesApi->text().trimmed());
    values.collectorExecutable = m_collectorExecutable->text().trimmed();
    values.autoStartCollector = m_autoStartCollector->isChecked();
    m_settings.saveCrawler(values);
}

void CrawlerSettingsPage::browseCollector()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择信息补充器可执行文件"), QString{},
        QStringLiteral("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (!path.isEmpty())
    {
        m_collectorExecutable->setText(path);
        save();
    }
}

TranslationSettingsPage::TranslationSettingsPage(Settings &settings, QWidget *parent)
    : QWidget(parent), m_settings(settings)
{
    setObjectName(QStringLiteral("TranslationSettingsPage"));
    const TranslationSettings values = m_settings.translation();
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    const auto spin = [this](int minimum, int maximum, int value, const QString &name) {
        auto *control = new QSpinBox(this);
        control->setObjectName(name);
        control->setRange(minimum, maximum);
        control->setValue(value);
        connect(control, &QSpinBox::valueChanged, this, [this](int) { save(); });
        return control;
    };

    m_engine = new DesignComboBox(this);
    m_engine->setObjectName(QStringLiteral("TranslationEngineCombo"));
    m_engine->addItem(QStringLiteral("Google"), QStringLiteral("google"));
    m_engine->addItem(QStringLiteral("LLM（OpenAI 兼容）"), QStringLiteral("llm"));
    m_engine->setCurrentIndex(m_engine->findData(values.engine.trimmed().toLower()));
    if (m_engine->currentIndex() < 0) m_engine->setCurrentIndex(0);
    form->addRow(new DesignLabel(QStringLiteral("翻译引擎"), this), m_engine);
    const auto line = [this](QFormLayout *target, const QString &label, const QString &name,
                             const QString &value, QLineEdit **field) {
        *field = new DesignLineEdit(this);
        (*field)->setObjectName(name);
        (*field)->setText(value);
        (*field)->setClearButtonEnabled(true);
        target->addRow(new DesignLabel(label, this), *field);
        connect(*field, &QLineEdit::editingFinished, this, &TranslationSettingsPage::save);
    };
    line(form, QStringLiteral("模型"), QStringLiteral("TranslationModelEdit"), values.model, &m_model);
    line(form, QStringLiteral("Base URL"), QStringLiteral("TranslationBaseUrlEdit"), values.baseUrl,
         &m_baseUrl);
    line(form, QStringLiteral("API Key"), QStringLiteral("TranslationApiKeyEdit"), values.apiKey,
         &m_apiKey);
    m_timeout = spin(1, 120, values.timeoutSeconds, QStringLiteral("TranslationTimeoutSpin"));
    m_retries = spin(0, 10, values.retries, QStringLiteral("TranslationRetriesSpin"));
    form->addRow(new DesignLabel(QStringLiteral("超时（秒）"), this), m_timeout);
    form->addRow(new DesignLabel(QStringLiteral("重试次数"), this), m_retries);
    m_fallback = new DesignComboBox(this);
    m_fallback->setObjectName(QStringLiteral("TranslationFallbackCombo"));
    m_fallback->addItem(QStringLiteral("失败返回空字符串"), QStringLiteral("empty"));
    m_fallback->addItem(QStringLiteral("失败返回原文"), QStringLiteral("source"));
    m_fallback->setCurrentIndex(m_fallback->findData(values.fallback.trimmed().toLower()));
    if (m_fallback->currentIndex() < 0) m_fallback->setCurrentIndex(0);
    form->addRow(new DesignLabel(QStringLiteral("失败回退"), this), m_fallback);
    layout->addLayout(form);

    layout->addWidget(new DesignLabel(QStringLiteral("<h3>llama.cpp 辅助</h3>"), this));
    auto *llamaForm = new QFormLayout;
    line(llamaForm, QStringLiteral("llama-server.exe"), QStringLiteral("LlamaServerExecutableEdit"),
         values.llama.serverExecutable, &m_serverExecutable);
    line(llamaForm, QStringLiteral("GGUF 模型"), QStringLiteral("LlamaModelPathEdit"), values.llama.modelPath,
         &m_modelPath);
    auto *serverBrowse = new DesignButton(QStringLiteral("浏览…"), this);
    auto *modelBrowse = new DesignButton(QStringLiteral("浏览…"), this);
    auto *serverRow = new QHBoxLayout;
    serverRow->addWidget(m_serverExecutable, 1);
    serverRow->addWidget(serverBrowse);
    auto *modelRow = new QHBoxLayout;
    modelRow->addWidget(m_modelPath, 1);
    modelRow->addWidget(modelBrowse);
    llamaForm->addRow(new DesignLabel(QStringLiteral("选择服务程序"), this), serverRow);
    llamaForm->addRow(new DesignLabel(QStringLiteral("选择模型文件"), this), modelRow);
    m_host = new DesignLineEdit(this);
    m_host->setObjectName(QStringLiteral("LlamaHostEdit"));
    m_host->setText(values.llama.host);
    connect(m_host, &QLineEdit::editingFinished, this, &TranslationSettingsPage::save);
    m_port = spin(1, 65535, values.llama.port, QStringLiteral("LlamaPortSpin"));
    auto *hostRow = new QHBoxLayout;
    hostRow->addWidget(m_host, 1);
    hostRow->addWidget(m_port);
    llamaForm->addRow(new DesignLabel(QStringLiteral("监听地址"), this), hostRow);
    m_mode = new DesignComboBox(this);
    m_mode->addItem(QStringLiteral("GPU"), QStringLiteral("gpu"));
    m_mode->addItem(QStringLiteral("CPU"), QStringLiteral("cpu"));
    m_mode->setCurrentIndex(m_mode->findData(values.llama.mode.trimmed().toLower()));
    if (m_mode->currentIndex() < 0) m_mode->setCurrentIndex(0);
    llamaForm->addRow(new DesignLabel(QStringLiteral("运行模式"), this), m_mode);
    m_contextSize = spin(256, 32768, values.llama.contextSize, QStringLiteral("LlamaContextSpin"));
    m_gpuLayers = spin(0, 200, values.llama.gpuLayers, QStringLiteral("LlamaGpuLayersSpin"));
    m_threads = spin(1, 256, values.llama.threads, QStringLiteral("LlamaThreadsSpin"));
    m_threadsBatch = spin(1, 512, values.llama.threadsBatch, QStringLiteral("LlamaThreadsBatchSpin"));
    m_batchSize = spin(1, 8192, values.llama.batchSize, QStringLiteral("LlamaBatchSpin"));
    m_microBatchSize = spin(1, 4096, values.llama.microBatchSize, QStringLiteral("LlamaMicroBatchSpin"));
    llamaForm->addRow(new DesignLabel(QStringLiteral("上下文大小"), this), m_contextSize);
    llamaForm->addRow(new DesignLabel(QStringLiteral("GPU layers"), this), m_gpuLayers);
    llamaForm->addRow(new DesignLabel(QStringLiteral("threads"), this), m_threads);
    llamaForm->addRow(new DesignLabel(QStringLiteral("threads-batch"), this), m_threadsBatch);
    llamaForm->addRow(new DesignLabel(QStringLiteral("batch-size"), this), m_batchSize);
    llamaForm->addRow(new DesignLabel(QStringLiteral("ubatch-size"), this), m_microBatchSize);
    m_mlock = new ToggleSwitch(48, 24, nullptr, this);
    m_autoSync = new ToggleSwitch(48, 24, nullptr, this);
    m_autoStart = new ToggleSwitch(48, 24, nullptr, this);
    m_mlock->setChecked(values.llama.mlock);
    m_autoSync->setChecked(values.llama.autoSyncTranslation);
    m_autoStart->setChecked(values.llama.autoStart);
    llamaForm->addRow(new DesignLabel(QStringLiteral("mlock"), this), m_mlock);
    llamaForm->addRow(new DesignLabel(QStringLiteral("自动回填翻译配置"), this), m_autoSync);
    llamaForm->addRow(new DesignLabel(QStringLiteral("打开软件自动启动"), this), m_autoStart);
    layout->addLayout(llamaForm);
    layout->addWidget(new DesignLabel(
        QStringLiteral("与 Python 版一致：这里配置外部 llama-server.exe；当前 C++ 版不负责启动或托管它。"),
        this));
    layout->addStretch();

    connect(m_engine, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateLlmFields(); save(); });
    connect(m_fallback, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { save(); });
    connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateModeFields(); save(); });
    connect(m_mlock, &ToggleSwitch::toggled, this, [this](bool) { save(); });
    connect(m_autoSync, &ToggleSwitch::toggled, this, [this](bool) { save(); });
    connect(m_autoStart, &ToggleSwitch::toggled, this, [this](bool) { save(); });
    connect(serverBrowse, &QPushButton::clicked, this, &TranslationSettingsPage::browseServerExecutable);
    connect(modelBrowse, &QPushButton::clicked, this, &TranslationSettingsPage::browseModel);
    updateLlmFields();
    updateModeFields();
}

void TranslationSettingsPage::save()
{
    TranslationSettings values = m_settings.translation();
    values.engine = m_engine->currentData().toString();
    values.model = m_model->text().trimmed();
    values.baseUrl = m_baseUrl->text().trimmed();
    values.apiKey = m_apiKey->text();
    values.timeoutSeconds = m_timeout->value();
    values.retries = m_retries->value();
    values.fallback = m_fallback->currentData().toString();
    values.llama.serverExecutable = m_serverExecutable->text().trimmed();
    values.llama.modelPath = m_modelPath->text().trimmed();
    values.llama.host = m_host->text().trimmed();
    values.llama.port = m_port->value();
    values.llama.mode = m_mode->currentData().toString();
    values.llama.contextSize = m_contextSize->value();
    values.llama.gpuLayers = m_gpuLayers->value();
    values.llama.threads = m_threads->value();
    values.llama.threadsBatch = m_threadsBatch->value();
    values.llama.batchSize = m_batchSize->value();
    values.llama.microBatchSize = m_microBatchSize->value();
    values.llama.mlock = m_mlock->isChecked();
    values.llama.autoSyncTranslation = m_autoSync->isChecked();
    values.llama.autoStart = m_autoStart->isChecked();
    m_settings.saveTranslation(values);
}

void TranslationSettingsPage::updateLlmFields()
{
    const bool isLlm = m_engine->currentData().toString() == QStringLiteral("llm");
    m_model->setEnabled(isLlm);
    m_baseUrl->setEnabled(isLlm);
    m_apiKey->setEnabled(isLlm);
}

void TranslationSettingsPage::updateModeFields()
{
    m_gpuLayers->setEnabled(m_mode->currentData().toString() == QStringLiteral("gpu"));
}

void TranslationSettingsPage::browseServerExecutable()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 llama-server.exe"), QString{},
        QStringLiteral("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (!path.isEmpty()) { m_serverExecutable->setText(path); save(); }
}

void TranslationSettingsPage::browseModel()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 GGUF 模型文件"), QString{},
        QStringLiteral("GGUF (*.gguf);;所有文件 (*.*)"));
    if (!path.isEmpty()) { m_modelPath->setText(path); save(); }
}

SettingsPage::SettingsPage(ThemeService &themeService, Settings &settings,
                           const QString &shortcutsFile,
                           QSqlDatabase publicDatabase,
                           QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("SettingsPage"));
    m_commonPage = new CommonSettingsPage(themeService, settings, this);
    const QList<ModernScrollMenu::Section> sections = {
        {QStringLiteral("常规"), m_commonPage},
        {QStringLiteral("视频"), new VideoSettingsPage(settings, publicDatabase, this)},
        {QStringLiteral("NFO"), pendingSettingsPage(QStringLiteral("Nfo"), this)},
        {QStringLiteral("信息补充器"), new CrawlerSettingsPage(settings, this)},
        {QStringLiteral("翻译"), new TranslationSettingsPage(settings, this)},
        {QStringLiteral("数据库"), pendingSettingsPage(QStringLiteral("Database"), this)},
        {QStringLiteral("快捷键"), new ShortcutSettingsPage(shortcutsFile, this)},
        {QStringLiteral("关于软件"), new AboutSettingsPage(themeService, this)},
    };
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new ModernScrollMenu(sections, this));
    if (auto *videoPage = findChild<VideoSettingsPage *>())
        connect(videoPage, &VideoSettingsPage::worksChanged, this, &SettingsPage::worksChanged);
}

QComboBox *SettingsPage::themeSelector() const
{
    return m_commonPage->themeSelector();
}

} // namespace darkeye
