#pragma once

#include "app/AppPaths.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "domain/Person.h"
#include "settings/Settings.h"

#include <QMainWindow>
#include <QMap>
#include <QSqlDatabase>

class QComboBox;
class QStackedWidget;

namespace darkeye
{

class Sidebar;
class DashboardPage;
class PersonDetailPage;
class PersonPage;
class StatisticsPage;
class WorkDetailPage;
class WorkPage;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(const AppPaths &paths, Settings &settings,
                        ThemeService &themeService,
                        QSqlDatabase publicDatabase = {}, QSqlDatabase privateDatabase = {},
                        QWidget *parent = nullptr);

    void showInitial();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
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
    void registerApplicationActions();

    AppPaths m_paths;
    Settings &m_settings;
    ThemeService &m_themeService;
    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
    Sidebar *m_sidebar = nullptr;
    DashboardPage *m_dashboardPage = nullptr;
    WorkPage *m_workPage = nullptr;
    WorkDetailPage *m_workDetailPage = nullptr;
    PersonPage *m_actressPage = nullptr;
    PersonPage *m_actorPage = nullptr;
    PersonDetailPage *m_actressDetailPage = nullptr;
    PersonDetailPage *m_actorDetailPage = nullptr;
    StatisticsPage *m_statisticsPage = nullptr;
    QComboBox *m_themeSelector = nullptr;
    QStackedWidget *m_pages = nullptr;
    QMap<QString, int> m_routeIndexes;
    QMap<QString, QWidget *> m_routePages;
    QStringList m_history;
    int m_historyIndex = -1;
};

} // namespace darkeye
