#pragma once

#include "settings/Paths.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/DatabaseManager.h"
#include "settings/Settings.h"

#include <QApplication>
#include <QProcess>
#include <memory>

class QWidget;

namespace darkeye {

class MainWindow;
class LocalApiServer;
class ManagedCollector;

class Application final
{
public:
    Application(int &argc, char **argv);
    ~Application();

    int run();

private:
    void configureIdentity();
    void applyInitialTheme();
    void prewarmGraphRenderer();
    void startBackgroundServices();
    void stopLlamaServer();

    QApplication m_application;
    settings::Paths m_paths;
    ThemeService m_themeService;
    DatabaseManager m_databaseManager;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<QWidget> m_graphPrewarmWindow;
    std::unique_ptr<LocalApiServer> m_localApiServer;
    std::unique_ptr<ManagedCollector> m_managedCollector;
    std::unique_ptr<QProcess> m_llamaServer;
};

} // namespace darkeye
