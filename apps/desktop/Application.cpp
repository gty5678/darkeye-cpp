#include "Application.h"

#include "app/LogService.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "MainWindow.h"
#include "ui/dialogs/TermsDialog.h"

#include <QIcon>
#include <QMessageBox>

namespace darkeye {

Application::Application(int &argc, char **argv)
    : m_application(argc, argv), m_themeService(m_application), m_paths(),
      m_settings(m_paths.settingsFile())
{
    configureIdentity();
    m_application.setWindowIcon(QIcon(QStringLiteral(":/sql/logo.svg")));

    QString errorMessage;
    if (!m_paths.ensureRuntimeDirectories(&errorMessage)) {
        QMessageBox::critical(nullptr, QStringLiteral("Darkeye 启动失败"), errorMessage);
        return;
    }

    LogService::initialize(m_paths.logFile());
    qInfo() << "Darkeye C++ starting" << QCoreApplication::applicationVersion();
    qInfo() << "Runtime data directory:" << m_paths.dataDirectory();
    qInfo() << "Public database:" << m_paths.publicDatabase();
    if (!m_databaseManager.initialize(m_paths, &errorMessage)) {
        qCritical() << "Database initialization failed:" << errorMessage;
        QMessageBox::critical(nullptr, QStringLiteral("Darkeye 数据库错误"), errorMessage);
        return;
    }
    applyInitialTheme();
    m_mainWindow = std::make_unique<MainWindow>(
        m_paths, m_settings, m_themeService, m_databaseManager.publicConnection().database(),
        m_databaseManager.privateConnection().database());
}

Application::~Application()
{
    qInfo() << "Darkeye C++ stopped";
    LogService::shutdown();
}

int Application::run()
{
    if (!m_mainWindow) {
        return 1;
    }
    AppSettings settings = m_settings.app();
    if (settings.firstLaunch)
    {
        TermsDialog terms;
        if (terms.exec() != QDialog::Accepted)
        {
            settings.firstLaunch = true;
            m_settings.saveApp(settings);
            return 0;
        }
        settings.firstLaunch = false;
        m_settings.saveApp(settings);
    }
    m_mainWindow->showInitial();
    return m_application.exec();
}

void Application::configureIdentity()
{
    QCoreApplication::setOrganizationName(QStringLiteral("Darkeye"));
    QCoreApplication::setApplicationName(QStringLiteral("Darkeye"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DARKEYE_VERSION));
}

void Application::applyInitialTheme()
{
    const AppSettings settings = m_settings.app();
    m_themeService.setTheme(ThemeService::fromSettings(settings.themeId),
                            settings.customPrimary);
}

} // namespace darkeye
