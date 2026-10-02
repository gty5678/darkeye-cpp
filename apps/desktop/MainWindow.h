#pragma once

#include "settings/Paths.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "domain/Person.h"
#include "settings/Settings.h"
#include "graph/GraphManager.h"
#include "crawler/CrawlerScheduler.h"
#include "services/CrawlerPersistenceService.h"
#include "services/ActressSyncService.h"

#include <QMainWindow>
#include <QMap>
#include <QQueue>
#include <QSqlDatabase>
#include <QSet>

#include <memory>

class QComboBox;
class QStackedWidget;

namespace darkeye
{

class Sidebar;
class DashboardPage;
class HomePage;
class ActorDetailPage;
class ActorPage;
class ActressDetailPage;
class ActressPage;
class PersonDetailPage;
class ModifyActressPage;
class ModifyActorPage;
class ManagementPage;
class PersonPage;
class StatisticsPage;
class WorkPage;
class ShelfPage;
class ForceDirectPage;
class FanartBrowserPage;
class LocalApiServer;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(ThemeService &themeService,
                        QSqlDatabase publicDatabase = {}, QSqlDatabase privateDatabase = {},
                        settings::Paths paths = {},
                        QWidget *parent = nullptr);

    void showInitial();
    /// Connects the HTTP bridge after both the window and local API are constructed.
    void setLocalApiServer(LocalApiServer &api);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void updateWindowTitle();
    void registerPage(const QString &menuTitle, const QString &routeName);
    QWidget *createPage(const QString &menuTitle, const QString &routeName,
                        QWidget *parent);
    QWidget *ensurePage(const QString &routeName);
    void populateThemeSelector();
    void changeTheme(int index);
    void restoreWindowState();
    void saveWindowState() const;
    void navigateTo(const QString &routeName, bool recordHistory = true);
    void navigateBackward();
    void navigateForward();
    void openPersonEditor(PersonKind kind, qint64 personId);
    void refreshRelationshipGraph();
    void refreshRelationshipGraphWork(qint64 workId);
    void showWorkInShelf(qint64 workId);
    void registerApplicationActions();
    void focusWorkSearch();
    [[nodiscard]] QWidget *partialCaptureTarget();
    void captureWidget(QWidget *widget);
    void attachLocalApi(LocalApiServer &api);
    void startNextActressSync();
    void enqueueActressSyncs(const QList<qint64> &actressIds);

    ThemeService &m_themeService;
    settings::Paths m_paths;
    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
    std::unique_ptr<graph::GraphManager> m_graphManager;
    std::unique_ptr<CrawlerScheduler> m_crawlerScheduler;
    std::unique_ptr<CrawlerPersistenceService> m_crawlerPersistence;
    std::unique_ptr<ActressSyncService> m_backgroundActressSync;
    QQueue<qint64> m_pendingActressSyncs;
    QSet<qint64> m_pendingActressSyncIds;
    Sidebar *m_sidebar = nullptr;
    HomePage *m_homePage = nullptr;
    DashboardPage *m_dashboardPage = nullptr;
    WorkPage *m_workPage = nullptr;
    ShelfPage *m_shelfPage = nullptr;
    ForceDirectPage *m_forceDirectPage = nullptr;
    FanartBrowserPage *m_fanartBrowserPage = nullptr;
    ActressPage *m_actressPage = nullptr;
    ActorPage *m_actorPage = nullptr;
    ActressDetailPage *m_actressDetailPage = nullptr;
    ActorDetailPage *m_actorDetailPage = nullptr;
    ModifyActressPage *m_actressEditorPage = nullptr;
    ModifyActorPage *m_actorEditorPage = nullptr;
    ManagementPage *m_managementPage = nullptr;
    StatisticsPage *m_statisticsPage = nullptr;
    QComboBox *m_themeSelector = nullptr;
    QStackedWidget *m_pages = nullptr;
    QMap<QString, int> m_routeIndexes;
    QMap<QString, QString> m_routeTitles;
    QMap<QString, QWidget *> m_routePages;
    QStringList m_history;
    int m_historyIndex = -1;
};

} // namespace darkeye
