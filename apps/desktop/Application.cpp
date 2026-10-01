#include "Application.h"

#include "app/LogService.h"
#include "crawler/ManagedCollector.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "http/LocalApiServer.h"
#include "MainWindow.h"
#include "graph_view/ForceViewRhiWidget.h"
#include "services/UpdateService.h"
#include "ui/dialogs/TermsDialog.h"

#include <QColor>
#include <QDate>
#include <QDir>
#include <QEventLoop>
#include <QIcon>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

namespace darkeye {

namespace
{

QString latestManifestUrl()
{
    const QString configPath = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("resources/config/update.ini"));
    QSettings config(configPath, QSettings::IniFormat);
    const QString configured = config.value(QStringLiteral("Update/LatestJsonUrl")).toString().trimmed();
    return configured.isEmpty() ? QStringLiteral("https://darkeye.win/latest.json") : configured;
}

QString currentIsoWeek()
{
    int year = 0;
    const int week = QDate::currentDate().weekNumber(&year);
    return QStringLiteral("%1-%2").arg(year).arg(week, 2, 10, QLatin1Char('0'));
}

bool startUpdater()
{
    const QString updater = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("DarkEyeUpdater.exe"));
    if (!QFileInfo::exists(updater))
        return false;
    const QStringList arguments = {
        QStringLiteral("--install-dir"), QCoreApplication::applicationDirPath(),
        QStringLiteral("--current-version"), QStringLiteral(DARKEYE_VERSION),
        QStringLiteral("--main-exe"), QStringLiteral("DarkEye.exe"),
        QStringLiteral("--latest-json-url"), latestManifestUrl(),
        QStringLiteral("--keep"), QStringLiteral("data"),
        QStringLiteral("--pid"), QString::number(QCoreApplication::applicationPid())};
    return QProcess::startDetached(updater, arguments, QCoreApplication::applicationDirPath());
}

} // namespace

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

void Application::prewarmGraphRenderer()
{
    // Python renders a small ForceViewRhiWidget during startup. On Windows,
    // a fully offscreen window may never submit a frame, so keep this renderer
    // visible underneath an opaque page widget.  It must be parented to the
    // page stack rather than centralWidget: the latter starts at x = 0 and
    // overlays the sidebar's transparent menu rows.
    auto *pageStack = m_mainWindow->centralWidget()->findChild<QStackedWidget *>(
        QStringLiteral("mainPages"));
    if (pageStack == nullptr) {
        qWarning() << "Graph renderer prewarm skipped: main page stack is unavailable";
        return;
    }
    m_graphPrewarmWindow = std::make_unique<QWidget>(pageStack);
    m_graphPrewarmWindow->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_graphPrewarmWindow->setGeometry(0, 0, 240, 200);

    auto *layout = new QVBoxLayout(m_graphPrewarmWindow.get());
    layout->setContentsMargins(0, 0, 0, 0);
    auto *view = new ForceViewRhiWidget(m_graphPrewarmWindow.get());
    layout->addWidget(view);
    view->setGraph(1, {}, {0.0f, 0.0f}, {QStringLiteral("prewarm")},
                   {QStringLiteral("prewarm")}, {4.0f}, {QColor(QStringLiteral("#808080"))});

    bool frameSubmitted = false;
    const QMetaObject::Connection connection = QObject::connect(
        view, &ForceViewRhiWidget::firstFrameSubmitted,
        view, [&frameSubmitted] { frameSubmitted = true; });
    m_graphPrewarmWindow->show();
    m_graphPrewarmWindow->lower();
    for (int frame = 0; frame < 10; ++frame) {
        m_application.processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(16);
    }
    QObject::disconnect(connection);
    view->pauseSimulation();
    // Prewarming requires one visible frame on Windows, but the helper must
    // not remain in the compositing tree afterwards: transparent application
    // widgets would otherwise reveal it.
    m_graphPrewarmWindow->hide();
    if (!frameSubmitted) {
        qWarning() << "Graph renderer prewarm did not submit a frame";
    }
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
    prewarmGraphRenderer();
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

    checkForUpdatesAutomatically();

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

void Application::checkForUpdatesAutomatically()
{
    AppSettings appSettings = settings::app();
    if (!appSettings.update.automaticCheck)
        return;

    const QString week = currentIsoWeek();
    if (appSettings.update.lastAutoCheckWeek == week)
        return;

    // Persist the attempt before issuing the request. This matches the Python
    // client's once-per-week policy and avoids a network failure delaying startup
    // with another check on every launch.
    appSettings.update.lastAutoCheckWeek = week;
    settings::saveApp(appSettings);

    auto *service = new UpdateService(&m_application);
    QObject::connect(service, &UpdateService::finished, &m_application,
                     [this, service, showNotification = appSettings.update.updateNotification]
                     (const utils::UpdateCheckResult &result)
                     {
                service->deleteLater();
                if (!result.success) {
                    qWarning() << "Automatic update check failed:" << result.message;
                    return;
                }
                if (!result.updateAvailable || !showNotification || m_mainWindow == nullptr)
                    return;

                const QString prompt = result.message
                    + QStringLiteral("\n\n是否立即更新？软件将退出以完成更新。");
                if (QMessageBox::question(m_mainWindow.get(), QStringLiteral("发现新版本"), prompt,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::Yes) != QMessageBox::Yes)
                    return;
                if (!startUpdater()) {
                    QMessageBox::critical(m_mainWindow.get(), QStringLiteral("更新失败"),
                                          QStringLiteral("无法启动更新程序。"));
                    return;
                }
                QCoreApplication::quit();
                     });
    service->check(QUrl::fromUserInput(latestManifestUrl()), QStringLiteral(DARKEYE_VERSION));
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
