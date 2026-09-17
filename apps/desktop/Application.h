#pragma once

#include "app/AppPaths.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/DatabaseManager.h"
#include "settings/Settings.h"

#include <QApplication>
#include <memory>

namespace darkeye {

class MainWindow;

class Application final
{
public:
    Application(int &argc, char **argv);
    ~Application();

    int run();

private:
    void configureIdentity();
    void applyInitialTheme();

    QApplication m_application;
    ThemeService m_themeService;
    AppPaths m_paths;
    Settings m_settings;
    DatabaseManager m_databaseManager;
    std::unique_ptr<MainWindow> m_mainWindow;
};

} // namespace darkeye
