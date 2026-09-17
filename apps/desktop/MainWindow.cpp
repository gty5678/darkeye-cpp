#include "MainWindow.h"

#include "ui/pages/DashboardPage.h"
#include "ui/pages/ManagementPage.h"
#include "ui/pages/PersonDetailPage.h"
#include "ui/pages/PersonPage.h"
#include "ui/pages/StatisticsPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/PlaceholderPage.h"
#include "ui/pages/WorkDetailPage.h"
#include "ui/pages/WorkPage.h"
#include "darkeye_ui/base/LazyWidget.h"
#include "darkeye_ui/components/Sidebar.h"
#include "ui/dialogs/PersonEditorDialog.h"
#include "ui/dialogs/AddMakeLoveDialog.h"
#include "ui/dialogs/AddMasturbationDialog.h"
#include "ui/dialogs/AddSexualArousalDialog.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QStackedWidget>
#include <QUrl>

namespace darkeye
{

MainWindow::MainWindow(const AppPaths &paths, Settings &settings, ThemeService &themeService,
                       QSqlDatabase publicDatabase, QSqlDatabase privateDatabase, QWidget *parent)
    : QMainWindow(parent), m_paths(paths), m_settings(settings), m_themeService(themeService),
      m_publicDatabase(std::move(publicDatabase)), m_privateDatabase(std::move(privateDatabase))
{
    setWindowTitle(QStringLiteral("暗之眼 V%1").arg(QStringLiteral(DARKEYE_VERSION)));
    setMinimumSize(900, 560);
    buildUi();
    restoreWindowState();
}

void MainWindow::showInitial()
{
    if (m_settings.app().maximized)
    {
        showMaximized();
    }
    else
    {
        show();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindow::buildUi()
{
    auto *centralWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const QList<SidebarMenuDefinition> menus = {
        {QStringLiteral("home"), QStringLiteral("首页"), QStringLiteral("house")},
        {QStringLiteral("work"), QStringLiteral("作品"), QStringLiteral("film")},
        {QStringLiteral("actress"), QStringLiteral("女演员"), QStringLiteral("venus")},
        {QStringLiteral("actor"), QStringLiteral("男演员"), QStringLiteral("mars")},
        {QStringLiteral("database"), QStringLiteral("管理"), QStringLiteral("database")},
        {QStringLiteral("chart"), QStringLiteral("统计"), QStringLiteral("chart_line")},
        {QStringLiteral("graph"), QStringLiteral("关系图"), QStringLiteral("share_2")},
        {QStringLiteral("shelf"), QStringLiteral("书架"), QStringLiteral("library_big")},
        {QStringLiteral("av"), QStringLiteral("暗黑界"), QStringLiteral("scroll_text")},
        {QStringLiteral("bell"), QStringLiteral("通知"), QStringLiteral("bell")},
    };
    m_sidebar = new Sidebar(menus, &m_themeService, centralWidget);

    m_pages = new QStackedWidget(centralWidget);
    m_pages->setObjectName(QStringLiteral("mainPages"));

    layout->addWidget(m_sidebar);
    layout->addWidget(m_pages, 1);
    setCentralWidget(centralWidget);

    registerPage(QStringLiteral("首页"), QStringLiteral("home"));
    registerPage(QStringLiteral("作品"), QStringLiteral("mutiwork"));
    registerPage(QStringLiteral("作品详情"), QStringLiteral("work"));
    registerPage(QStringLiteral("女演员"), QStringLiteral("actress"));
    registerPage(QStringLiteral("男演员"), QStringLiteral("actor"));
    registerPage(QStringLiteral("女演员详情"), QStringLiteral("actress_detail"));
    registerPage(QStringLiteral("男演员详情"), QStringLiteral("actor_detail"));
    registerPage(QStringLiteral("管理"), QStringLiteral("database"));
    registerPage(QStringLiteral("统计"), QStringLiteral("chart"));
    registerPage(QStringLiteral("关系图"), QStringLiteral("graph"));
    registerPage(QStringLiteral("书架"), QStringLiteral("shelf"));
    registerPage(QStringLiteral("暗黑界"), QStringLiteral("av"));
    registerPage(QStringLiteral("通知"), QStringLiteral("inbox"));
    registerPage(QStringLiteral("设置"), QStringLiteral("setting"));

    const QMap<QString, QString> menuRoutes = {
        {QStringLiteral("home"), QStringLiteral("home")},
        {QStringLiteral("work"), QStringLiteral("mutiwork")},
        {QStringLiteral("actress"), QStringLiteral("actress")},
        {QStringLiteral("actor"), QStringLiteral("actor")},
        {QStringLiteral("database"), QStringLiteral("database")},
        {QStringLiteral("chart"), QStringLiteral("chart")},
        {QStringLiteral("graph"), QStringLiteral("graph")},
        {QStringLiteral("shelf"), QStringLiteral("shelf")},
        {QStringLiteral("av"), QStringLiteral("av")},
        {QStringLiteral("bell"), QStringLiteral("inbox")},
        {QStringLiteral("setting"), QStringLiteral("setting")},
    };
    connect(m_sidebar, &Sidebar::itemClicked, this,
            [this, menuRoutes](const QString &menuId)
            {
                const QString route = menuRoutes.value(menuId);
                if (m_routeIndexes.contains(route))
                    navigateTo(route);
            });
    connect(m_sidebar, &Sidebar::backwardClicked, this, &MainWindow::navigateBackward);
    connect(m_sidebar, &Sidebar::forwardClicked, this, &MainWindow::navigateForward);
    registerApplicationActions();
    navigateTo(QStringLiteral("home"));
}

void MainWindow::registerApplicationActions()
{
    QJsonObject userShortcuts;
    QFile shortcutFile(m_paths.shortcutsFile());
    if (shortcutFile.open(QIODevice::ReadOnly))
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(shortcutFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
        {
            userShortcuts = document.object();
        }
    }
    const auto shortcut = [&userShortcuts](const QString &id, const QString &fallback)
    {
        const QString configured = userShortcuts.value(id).toString().trimmed();
        return QKeySequence(configured.isEmpty() ? fallback : configured);
    };
    const auto addAction = [this, &shortcut](const QString &id, const QString &title,
                                             const QString &fallback, auto callback)
    {
        auto *action = new QAction(title, this);
        action->setObjectName(id);
        action->setShortcut(shortcut(id, fallback));
        connect(action, &QAction::triggered, this, callback);
        this->addAction(action);
    };

    addAction(QStringLiteral("add_masturbation_record"), QStringLiteral("添加自慰记录"),
              QStringLiteral("M"), [this]
              {
                  AddMasturbationDialog dialog(m_publicDatabase, m_privateDatabase, this);
                  connect(&dialog, &AddMasturbationDialog::recordAdded, this, [this] {
                      if (m_statisticsPage)
                          m_statisticsPage->refresh();
                  });
                  dialog.exec();
              });
    addAction(QStringLiteral("add_quick_work"), QStringLiteral("快速添加番号"),
              QStringLiteral("W"), [this] {
                  auto *page = static_cast<WorkPage *>(ensurePage(QStringLiteral("mutiwork")));
                  page->openCreateEditor();
                  navigateTo(QStringLiteral("mutiwork"));
              });
    addAction(QStringLiteral("add_makelove_record"), QStringLiteral("添加做爱记录"),
              QStringLiteral("L"), [this]
              {
                  AddMakeLoveDialog dialog(m_privateDatabase, this);
                  connect(&dialog, &AddMakeLoveDialog::recordAdded, this, [this] {
                      if (m_statisticsPage)
                          m_statisticsPage->refresh();
                  });
                  dialog.exec();
              });
    addAction(QStringLiteral("add_sexual_rousal_record"), QStringLiteral("添加晨勃记录"),
              QStringLiteral("A"), [this]
              {
                  AddSexualArousalDialog dialog(m_privateDatabase, this);
                  connect(&dialog, &AddSexualArousalDialog::recordAdded, this, [this] {
                      if (m_statisticsPage)
                          m_statisticsPage->refresh();
                  });
                  dialog.exec();
              });
    addAction(QStringLiteral("open_help"), QStringLiteral("打开文档"), QStringLiteral("H"),
              [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://de4321.github.io/darkeye/"))); });
}

void MainWindow::populateThemeSelector()
{
    const QList<QPair<ThemeId, QString>> themes = {
        {ThemeId::Light, QStringLiteral("亮色主题")},
        {ThemeId::Dark, QStringLiteral("暗色主题")},
        {ThemeId::Red, QStringLiteral("红色")},
        {ThemeId::Yellow, QStringLiteral("黄色")},
        {ThemeId::Green, QStringLiteral("绿色")},
        {ThemeId::Blue, QStringLiteral("蓝色")},
        {ThemeId::Purple, QStringLiteral("紫色")},
    };
    for (const auto &[theme, label] : themes)
    {
        m_themeSelector->addItem(label, static_cast<int>(theme));
        if (theme == m_themeService.current())
        {
            m_themeSelector->setCurrentIndex(m_themeSelector->count() - 1);
        }
    }
}

void MainWindow::changeTheme(int index)
{
    const ThemeId theme = static_cast<ThemeId>(m_themeSelector->itemData(index).toInt());
    if (!m_themeService.setTheme(theme, m_themeService.customPrimary()))
    {
        return;
    }

    AppSettings settings = m_settings.app();
    settings.themeId = ThemeService::toSettings(theme);
    m_settings.saveApp(settings);
}

void MainWindow::registerPage(const QString &menuTitle, const QString &routeName)
{
    auto *page = createPage(menuTitle, routeName, m_pages);
    const int index = m_pages->addWidget(page);
    m_routeIndexes.insert(routeName, index);
    m_routePages.insert(routeName, page);
}

QWidget *MainWindow::ensurePage(const QString &routeName)
{
    const auto page = m_routePages.constFind(routeName);
    if (page == m_routePages.cend())
    {
        return nullptr;
    }
    QWidget *widget = page.value();
    if (auto *lazyWidget = dynamic_cast<LazyWidget *>(widget))
    {
        lazyWidget->initialize();
    }
    return widget;
}

QWidget *MainWindow::createPage(const QString &menuTitle, const QString &routeName,
                                QWidget *parent)
{
    if (routeName == QStringLiteral("home"))
    {
        m_dashboardPage =
            new DashboardPage(m_publicDatabase, m_privateDatabase, parent);
        return m_dashboardPage;
    }
    if (routeName == QStringLiteral("mutiwork"))
    {
        m_workPage = new WorkPage(m_publicDatabase, m_themeService, m_settings,
                                  m_paths.workCoverDirectory(), m_privateDatabase,
                                  m_paths.fanartDirectory(),
                                  m_settings.crawler().coverFetchApiUrl, parent);
        connect(m_workPage, &WorkPage::detailRequested, this, [this](qint64 workId) {
            auto *detail = static_cast<WorkDetailPage *>(ensurePage(QStringLiteral("work")));
            if (detail->showWork(workId))
                navigateTo(QStringLiteral("work"));
        });
        return m_workPage;
    }
    if (routeName == QStringLiteral("work"))
    {
        m_workDetailPage = new WorkDetailPage(m_publicDatabase, m_privateDatabase, m_themeService,
                                              m_paths.workCoverDirectory(), parent);
        connect(m_workDetailPage, &WorkDetailPage::editRequested, this, [this](qint64 workId) {
            auto *work = static_cast<WorkPage *>(ensurePage(QStringLiteral("mutiwork")));
            work->openEditor(workId);
        });
        connect(m_workDetailPage, &WorkDetailPage::workDeleted, this, [this](qint64) {
            if (m_workPage)
                m_workPage->refresh();
            navigateTo(QStringLiteral("mutiwork"));
        });
        return m_workDetailPage;
    }
    if (routeName == QStringLiteral("actress") || routeName == QStringLiteral("actor"))
    {
        const PersonKind kind = routeName == QStringLiteral("actress") ? PersonKind::Actress
                                                                          : PersonKind::Actor;
        const QString imageDirectory = kind == PersonKind::Actress ? m_paths.actressImageDirectory()
                                                                     : m_paths.actorImageDirectory();
        auto *personPage = new PersonPage(kind, m_publicDatabase, m_privateDatabase,
                                          m_themeService, imageDirectory, parent);
        if (kind == PersonKind::Actress)
            m_actressPage = personPage;
        else
            m_actorPage = personPage;
        connect(personPage, &PersonPage::detailRequested, this,
                [this](PersonKind requestedKind, qint64 personId) {
                    const QString detailRoute = requestedKind == PersonKind::Actress
                        ? QStringLiteral("actress_detail")
                        : QStringLiteral("actor_detail");
                    auto *detail = static_cast<PersonDetailPage *>(ensurePage(detailRoute));
                    if (detail->showPerson(personId))
                        navigateTo(detailRoute);
                });
        connect(personPage, &PersonPage::editRequested, this, &MainWindow::openPersonEditor);
        return personPage;
    }
    if (routeName == QStringLiteral("actress_detail") ||
        routeName == QStringLiteral("actor_detail"))
    {
        const PersonKind kind = routeName == QStringLiteral("actress_detail")
            ? PersonKind::Actress
            : PersonKind::Actor;
        const QString imageDirectory = kind == PersonKind::Actress ? m_paths.actressImageDirectory()
                                                                     : m_paths.actorImageDirectory();
        auto *detail = new PersonDetailPage(kind, m_publicDatabase, m_privateDatabase,
                                            m_themeService, imageDirectory, parent,
                                            m_paths.workCoverDirectory());
        if (kind == PersonKind::Actress)
            m_actressDetailPage = detail;
        else
            m_actorDetailPage = detail;
        connect(detail, &PersonDetailPage::editRequested, this, &MainWindow::openPersonEditor);
        connect(detail, &PersonDetailPage::workRequested, this, [this](qint64 workId) {
            auto *workDetail = static_cast<WorkDetailPage *>(ensurePage(QStringLiteral("work")));
            if (workDetail->showWork(workId))
                navigateTo(QStringLiteral("work"));
        });
        connect(detail, &PersonDetailPage::favoriteChanged, this, [this, kind] {
            PersonPage *list = kind == PersonKind::Actress ? m_actressPage : m_actorPage;
            if (list)
                list->refresh();
        });
        return detail;
    }
    if (routeName == QStringLiteral("database"))
    {
        auto *management =
            new ManagementPage(m_publicDatabase, m_themeService, m_paths.workCoverDirectory(),
                               m_paths.fanartDirectory(),
                               m_settings.crawler().coverFetchApiUrl,
                               m_settings.crawler().topActressesApiUrl, parent);
        connect(management, &ManagementPage::referencesChanged, this,
                [this](ReferenceKind) {
                    if (m_workPage)
                        m_workPage->refreshReferences();
                });
        connect(management, &ManagementPage::tagsChanged, this, [this] {
            if (m_workPage)
                m_workPage->refreshTags();
        });
        connect(management, &ManagementPage::worksChanged, this, [this] {
            if (m_workPage)
                m_workPage->refresh();
        });
        connect(management, &ManagementPage::actressesChanged, this, [this] {
            if (m_actressPage)
                m_actressPage->refresh();
        });
        connect(management, &ManagementPage::workRequested, this, [this](qint64 workId) {
            auto *detail = static_cast<WorkDetailPage *>(ensurePage(QStringLiteral("work")));
            if (detail->showWork(workId))
                navigateTo(QStringLiteral("work"));
        });
        connect(management, &ManagementPage::actressRequested, this, [this](qint64 actressId) {
            auto *detail =
                static_cast<PersonDetailPage *>(ensurePage(QStringLiteral("actress_detail")));
            if (detail->showPerson(actressId))
                navigateTo(QStringLiteral("actress_detail"));
        });
        return management;
    }
    if (routeName == QStringLiteral("chart"))
    {
        m_statisticsPage =
            new StatisticsPage(m_publicDatabase, m_privateDatabase, m_themeService,
                               m_paths.actressImageDirectory(), parent);
        return m_statisticsPage;
    }
    if (routeName == QStringLiteral("setting"))
    {
        auto *settings = new SettingsPage(m_themeService, m_settings, m_paths.shortcutsFile(),
                                          m_publicDatabase, parent);
        connect(settings, &SettingsPage::worksChanged, this, [this] {
            if (m_workPage)
                m_workPage->refresh();
        });
        m_themeSelector = settings->themeSelector();
        populateThemeSelector();
        connect(m_themeSelector, &QComboBox::currentIndexChanged, this, &MainWindow::changeTheme);
        return settings;
    }
    return new PlaceholderPage(menuTitle, routeName, parent);
}

void MainWindow::navigateTo(const QString &routeName, bool recordHistory)
{
    if (!m_routeIndexes.contains(routeName))
        return;
    if (!ensurePage(routeName))
        return;
    if (recordHistory)
    {
        if (m_historyIndex >= 0 && m_history.value(m_historyIndex) == routeName)
        {
            m_pages->setCurrentIndex(m_routeIndexes.value(routeName));
            return;
        }
        while (m_history.size() > m_historyIndex + 1)
            m_history.removeLast();
        m_history.append(routeName);
        m_historyIndex = m_history.size() - 1;
    }
    m_pages->setCurrentIndex(m_routeIndexes.value(routeName));
    if (routeName == QStringLiteral("chart"))
    {
        m_statisticsPage->refresh();
    }
    if (routeName == QStringLiteral("home"))
    {
        m_dashboardPage->refresh();
    }
    if (routeName == QStringLiteral("setting"))
    {
        m_sidebar->clearSelection();
    }
    else if (routeName == QStringLiteral("work") || routeName == QStringLiteral("mutiwork"))
    {
        m_sidebar->select(QStringLiteral("work"));
    }
    else if (routeName == QStringLiteral("actress_detail"))
    {
        m_sidebar->select(QStringLiteral("actress"));
    }
    else if (routeName == QStringLiteral("actor_detail"))
    {
        m_sidebar->select(QStringLiteral("actor"));
    }
    else
    {
        const QMap<QString, QString> routeMenus{
            {QStringLiteral("home"), QStringLiteral("home")},
            {QStringLiteral("actress"), QStringLiteral("actress")},
            {QStringLiteral("actor"), QStringLiteral("actor")},
            {QStringLiteral("database"), QStringLiteral("database")},
            {QStringLiteral("chart"), QStringLiteral("chart")},
            {QStringLiteral("graph"), QStringLiteral("graph")},
            {QStringLiteral("shelf"), QStringLiteral("shelf")},
            {QStringLiteral("av"), QStringLiteral("av")},
            {QStringLiteral("inbox"), QStringLiteral("bell")}};
        if (routeMenus.contains(routeName))
            m_sidebar->select(routeMenus.value(routeName));
    }
}

void MainWindow::openPersonEditor(PersonKind kind, qint64 personId)
{
    const QString imageDirectory = kind == PersonKind::Actress ? m_paths.actressImageDirectory()
                                                               : m_paths.actorImageDirectory();
    auto *editor = new PersonEditorDialog(m_publicDatabase, m_themeService, imageDirectory, this,
                                          m_paths.workCoverDirectory());
    editor->setAttribute(Qt::WA_DeleteOnClose);
    if (!editor->loadPerson(kind, personId))
    {
        editor->deleteLater();
        return;
    }
    connect(editor, &PersonEditorDialog::personSaved, this,
            [this](PersonKind savedKind, qint64 savedId)
            {
                PersonPage *list = savedKind == PersonKind::Actress ? m_actressPage : m_actorPage;
                PersonDetailPage *detail =
                    savedKind == PersonKind::Actress ? m_actressDetailPage : m_actorDetailPage;
                if (list)
                    list->refresh();
                if (detail && detail->currentPersonId() == savedId)
                    detail->showPerson(savedId);
            });
    connect(editor, &PersonEditorDialog::workLinkRequested, this, [this](qint64 workId) {
        auto *detail = static_cast<WorkDetailPage *>(ensurePage(QStringLiteral("work")));
        if (detail->showWork(workId))
            navigateTo(QStringLiteral("work"));
    });
    connect(editor, &PersonEditorDialog::actressLinkRequested, this, [this](qint64 actressId) {
        auto *detail =
            static_cast<PersonDetailPage *>(ensurePage(QStringLiteral("actress_detail")));
        if (detail->showPerson(actressId))
            navigateTo(QStringLiteral("actress_detail"));
    });
    editor->open();
}

void MainWindow::navigateBackward()
{
    if (m_historyIndex <= 0)
        return;
    --m_historyIndex;
    navigateTo(m_history.at(m_historyIndex), false);
}

void MainWindow::navigateForward()
{
    if (m_historyIndex + 1 >= m_history.size())
        return;
    ++m_historyIndex;
    navigateTo(m_history.at(m_historyIndex), false);
}

void MainWindow::restoreWindowState()
{
    const AppSettings settings = m_settings.app();
    resize(settings.windowSize);
    move(settings.windowPosition);
}

void MainWindow::saveWindowState() const
{
    AppSettings settings = m_settings.app();
    settings.maximized = isMaximized();
    if (!isMaximized())
    {
        settings.windowSize = size();
        settings.windowPosition = pos();
    }
    m_settings.saveApp(settings);
}

} // namespace darkeye
