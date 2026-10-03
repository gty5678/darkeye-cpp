#pragma once

#include "settings/Paths.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/DatabaseManager.h"
#include "settings/Settings.h"

#include <QApplication>
#include <QProcess>
#include <memory>

class QWidget;
class ForceViewRhiWidget;
class QQuickWidget;

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
    void prepareGraphicsPrewarm();
    void finishGraphicsPrewarm();
    void startBackgroundServices();
    void checkForUpdatesAutomatically();
    void stopLlamaServer();

    QApplication m_application;
    settings::Paths m_paths;
    ThemeService m_themeService;
    DatabaseManager m_databaseManager;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<QWidget> m_graphicsPrewarmWindow;
    QQuickWidget *m_quickPrewarmView = nullptr;
    ForceViewRhiWidget *m_graphPrewarmView = nullptr;
    bool m_graphPrewarmFrameSubmitted = false;
    std::unique_ptr<LocalApiServer> m_localApiServer;
    std::unique_ptr<ManagedCollector> m_managedCollector;
    std::unique_ptr<QProcess> m_llamaServer;
};

} // namespace darkeye
