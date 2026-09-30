#include "Application.h"

#include "app/LogService.h"
#include "crawler/ManagedCollector.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "http/LocalApiServer.h"
#include "MainWindow.h"
#include "ui/dialogs/TermsDialog.h"

#include <QIcon>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

namespace darkeye {

Application::Application(int &argc, char **argv)
    : m_application(argc, argv), m_themeService(m_application)
{
    configureIdentity();
    m_application.setWindowIcon(QIcon(QStringLiteral(":/sql/logo.svg")));

    QString errorMessage;
    if (!m_paths.ensureRuntimeDirectories(&errorMessage)) {
        QMessageBox::critical(nullptr, QStringLiteral("Darkeye 启动失败"), errorMessage);
        return;
    }
    if (!settings::ensureDefaults(&errorMessage)) {
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
        m_themeService, m_databaseManager.publicConnection().database(),
        m_databaseManager.privateConnection().database(), m_paths);
    m_localApiServer = std::make_unique<LocalApiServer>(m_databaseManager.publicConnection().database());
    m_managedCollector = std::make_unique<ManagedCollector>();
    m_llamaServer = std::make_unique<QProcess>();
    m_mainWindow->setLocalApiServer(*m_localApiServer);
    QObject::connect(&m_application, &QCoreApplication::aboutToQuit, &m_application,
                     [this] { stopLlamaServer(); });
}

Application::~Application()
{
    if (m_localApiServer) m_localApiServer->stop();
    if (m_managedCollector) m_managedCollector->stop();
    stopLlamaServer();
    qInfo() << "Darkeye C++ stopped";
    LogService::shutdown();
}

int Application::run()
{
    if (!m_mainWindow) {
        return 1;
    }
    AppSettings appSettings = settings::app();
    if (appSettings.firstLaunch)
    {
        TermsDialog terms;
        if (terms.exec() != QDialog::Accepted)
        {
            appSettings.firstLaunch = true;
            settings::saveApp(appSettings);
            return 0;
        }
        appSettings.firstLaunch = false;
        settings::saveApp(appSettings);
    }
    m_mainWindow->showInitial();
    QTimer::singleShot(0, &m_application, [this] { startBackgroundServices(); });
    return m_application.exec();
}

void Application::startBackgroundServices()
{
    QString errorMessage;
    if (!m_localApiServer->start(56789, &errorMessage))
        qWarning() << "Local HTTP API did not start:" << errorMessage;
    else
        qInfo() << "Local HTTP API listening on 127.0.0.1:" << m_localApiServer->port();

    const CrawlerSettings crawlerSettings = settings::crawler();
    if (crawlerSettings.autoStartCollector && !crawlerSettings.collectorExecutable.trimmed().isEmpty()
        && !m_managedCollector->start(crawlerSettings.collectorExecutable, &errorMessage))
        qWarning() << "Collector did not start:" << errorMessage;

    const TranslationSettings translation = settings::translation();
    const LlamaCppSettings &llama = translation.llama;
    if (!llama.autoStart) return;
    if (llama.serverExecutable.trimmed().isEmpty() || llama.modelPath.trimmed().isEmpty()) {
        qWarning() << "llama-server auto start skipped: executable or model is not configured";
        return;
    }
    QStringList arguments{QStringLiteral("-m"), llama.modelPath,
                          QStringLiteral("--host"), llama.host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : llama.host,
                          QStringLiteral("--port"), QString::number(llama.port),
                          QStringLiteral("-c"), QString::number(llama.contextSize),
                          QStringLiteral("-t"), QString::number(llama.threads),
                          QStringLiteral("-tb"), QString::number(llama.threadsBatch),
                          QStringLiteral("-b"), QString::number(llama.batchSize),
                          QStringLiteral("-ub"), QString::number(llama.microBatchSize)};
    if (llama.mode == QStringLiteral("gpu")) arguments << QStringLiteral("-ngl") << QString::number(llama.gpuLayers);
    if (llama.mlock) arguments << QStringLiteral("--mlock");
    m_llamaServer->setProgram(llama.serverExecutable);
    m_llamaServer->setArguments(arguments);
    m_llamaServer->setWorkingDirectory(QFileInfo(llama.serverExecutable).absolutePath());
    m_llamaServer->start();
    if (!m_llamaServer->waitForStarted(5000))
        qWarning() << "llama-server auto start failed:" << m_llamaServer->errorString();
    else
        qInfo() << "llama-server started with PID" << m_llamaServer->processId();
}

void Application::stopLlamaServer()
{
    if (!m_llamaServer || m_llamaServer->state() == QProcess::NotRunning) return;

    const qint64 processId = m_llamaServer->processId();
    qInfo() << "Stopping managed llama-server with PID" << processId;
    m_llamaServer->terminate();
    if (m_llamaServer->waitForFinished(3000)) return;

#ifdef Q_OS_WIN
    // The server can leave worker processes behind on Windows.  End the entire
    // tree, matching the lifecycle policy used for Python's managed services.
    QProcess::execute(QStringLiteral("taskkill"),
                      {QStringLiteral("/PID"), QString::number(processId),
                       QStringLiteral("/T"), QStringLiteral("/F")});
#else
    m_llamaServer->kill();
#endif
    if (!m_llamaServer->waitForFinished(1000))
        qWarning() << "llama-server did not exit after forced shutdown, PID" << processId;
}

void Application::configureIdentity()
{
    QCoreApplication::setOrganizationName(QStringLiteral("Darkeye"));
    QCoreApplication::setApplicationName(QStringLiteral("Darkeye"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DARKEYE_VERSION));
}

void Application::applyInitialTheme()
{
    const AppSettings appSettings = settings::app();
    m_themeService.setTheme(ThemeService::fromSettings(appSettings.themeId),
                            appSettings.customPrimary);
}

} // namespace darkeye
