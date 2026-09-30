#include "settings/Paths.h"
#include "settings/Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class AppPathsTest final : public QObject
{
    Q_OBJECT

private slots:
    void usesPortableLayoutByDefault();
    void createsEveryRuntimeDirectory();
    void initializesMissingSettingsFile();
    void honorsLegacyRelativeAndAbsoluteDatabasePaths();
    void readsAndWritesTypedSettings();
};

void AppPathsTest::usesPortableLayoutByDefault()
{
    const QString applicationDirectory = QStringLiteral("C:/portable/Darkeye");
    const darkeye::settings::Paths paths(applicationDirectory);
    QCOMPARE(QDir::fromNativeSeparators(paths.dataDirectory()),
             QStringLiteral("C:/portable/Darkeye/data"));
    QCOMPARE(QDir::fromNativeSeparators(paths.publicDatabase()),
             QStringLiteral("C:/portable/Darkeye/data/public/public.db"));
    QCOMPARE(QDir::fromNativeSeparators(paths.privateDatabase()),
             QStringLiteral("C:/portable/Darkeye/data/private/private.db"));
    QCOMPARE(QDir::fromNativeSeparators(paths.publicBackupDirectory()),
             QStringLiteral("C:/portable/Darkeye/data/public/public_backup"));
    QCOMPARE(QDir::fromNativeSeparators(paths.workCoverDirectory()),
             QStringLiteral("C:/portable/Darkeye/data/public/workcovers"));
    QCOMPARE(QDir::fromNativeSeparators(paths.fanartDirectory()),
             QStringLiteral("C:/portable/Darkeye/data/public/fanart"));
    const darkeye::CrawlerSettings crawler = darkeye::settings::crawler(paths.settingsFile());
    QCOMPARE(crawler.coverFetchApiUrl, QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/image")));
    QCOMPARE(crawler.workApiBaseUrl, QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/work")));
    QCOMPARE(crawler.actressApiBaseUrl,
             QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/actress")));
    QCOMPARE(crawler.topActressesApiUrl,
             QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/top-actresses")));
    QCOMPARE(QDir::fromNativeSeparators(paths.actressImageDirectory()),
             QStringLiteral("C:/portable/Darkeye/data/public/actressimages"));
    QCOMPARE(QDir::fromNativeSeparators(paths.actorImageDirectory()),
             QStringLiteral("C:/portable/Darkeye/data/public/actorimages"));
    QCOMPARE(QDir::fromNativeSeparators(paths.privateBackupDirectory()),
             QStringLiteral("C:/portable/Darkeye/data/private/private_backup"));
    QCOMPARE(QDir::fromNativeSeparators(paths.shortcutsFile()),
             QStringLiteral("C:/portable/Darkeye/data/shortcuts.json"));
    QCOMPARE(QDir::fromNativeSeparators(paths.crawlerNavButtonsFile()),
             QStringLiteral("C:/portable/Darkeye/data/crawler_nav_buttons.json"));
    QCOMPARE(QDir::fromNativeSeparators(paths.actressNavButtonsFile()),
             QStringLiteral("C:/portable/Darkeye/data/actress_nav_buttons.json"));
    QCOMPARE(QDir::fromNativeSeparators(paths.addWorkWorkspaceLayoutFile()),
             QStringLiteral("C:/portable/Darkeye/data/add_work_workspace_layout.json"));
}

void AppPathsTest::createsEveryRuntimeDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString applicationDirectory = temporaryDirectory.path();
    const darkeye::settings::Paths paths(applicationDirectory);
    QString errorMessage;
    QVERIFY2(paths.ensureRuntimeDirectories(&errorMessage),
             qPrintable(errorMessage));

    const QStringList expectedDirectories = {
        QStringLiteral("public/public_backup"),
        QStringLiteral("public/workcovers"),
        QStringLiteral("public/fanart"),
        QStringLiteral("public/actressimages"),
        QStringLiteral("public/actorimages"),
        QStringLiteral("private/private_backup"),
        QStringLiteral("cache/images"),
        QStringLiteral("cache/graph"),
        QStringLiteral("cache/translations"),
        QStringLiteral("temp"),
        QStringLiteral("logs"),
    };

    for (const QString &relativeDirectory : expectedDirectories)
    {
        QVERIFY2(QFileInfo::exists(QDir(paths.dataDirectory()).filePath(relativeDirectory)),
                 qPrintable(relativeDirectory));
    }

    const auto readNavigationExample = [&paths](const QString &fileName) {
        QFile file(QDir(paths.dataDirectory()).filePath(fileName));
        if (!file.open(QIODevice::ReadOnly)) return QJsonArray{};
        return QJsonDocument::fromJson(file.readAll()).array();
    };
    const QJsonArray workNavigation = readNavigationExample(QStringLiteral("crawler_nav_buttons.json"));
    QCOMPARE(workNavigation.size(), 1);
    QCOMPARE(workNavigation.first().toObject().value(QStringLiteral("url")).toString(),
             QStringLiteral("https://www.google.com/search?q={serial}"));
    const QJsonArray actressNavigation = readNavigationExample(QStringLiteral("actress_nav_buttons.json"));
    QCOMPARE(actressNavigation.size(), 1);
    QCOMPARE(actressNavigation.first().toObject().value(QStringLiteral("url")).toString(),
             QStringLiteral("https://www.google.com/search?q={jp_name}"));

    // 用户自定义的导航配置不可被后续启动覆盖。
    QFile customFile(paths.crawlerNavButtonsFile());
    QVERIFY(customFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(customFile.write("[]") == 2);
    customFile.close();
    QVERIFY(paths.ensureRuntimeDirectories());
    QVERIFY(customFile.open(QIODevice::ReadOnly));
    QCOMPARE(customFile.readAll(), QByteArray("[]"));
}

void AppPathsTest::initializesMissingSettingsFile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsFile =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("data/settings.ini"));
    QVERIFY(!QFileInfo::exists(settingsFile));
    QVERIFY(darkeye::settings::ensureDefaults(settingsFile));
    QVERIFY(QFileInfo::exists(settingsFile));

    QSettings ini(settingsFile, QSettings::IniFormat);
    QCOMPARE(ini.value(QStringLiteral("window/size")).toSize(), QSize(800, 600));
    QCOMPARE(ini.value(QStringLiteral("App/Theme")).toString(), QStringLiteral("LIGHT"));
    QCOMPARE(ini.value(QStringLiteral("Paths/Database")).toString(),
             QStringLiteral("data/public/public.db"));
    QCOMPARE(ini.value(QStringLiteral("Crawler/WorkApiBaseUrl")).toString(),
             QStringLiteral("http://127.0.0.1:56790/api/v1/work"));
    QCOMPARE(ini.value(QStringLiteral("WebDAV/RemoteRoot")).toString(),
             QStringLiteral("/darkeye"));
    QCOMPARE(ini.value(QStringLiteral("Translation/Engine")).toString(),
             QStringLiteral("google"));
    QCOMPARE(ini.value(QStringLiteral("LlamaCpp/Mode")).toString(), QStringLiteral("cpu"));

    ini.setValue(QStringLiteral("App/Theme"), QStringLiteral("DARK"));
    ini.sync();
    QVERIFY(darkeye::settings::ensureDefaults(settingsFile));
    QCOMPARE(darkeye::settings::app(settingsFile).themeId, QStringLiteral("DARK"));
}

void AppPathsTest::honorsLegacyRelativeAndAbsoluteDatabasePaths()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString absolutePrivate =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("external/private.db"));

    const QString applicationDirectory = temporaryDirectory.path();
    const darkeye::settings::Paths paths(applicationDirectory);
    QVERIFY(paths.ensureRuntimeDirectories());
    QSettings settings(paths.settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Paths/Database"), QStringLiteral("legacy/public/custom.db"));
    settings.setValue(QStringLiteral("Paths/PrivateDatabase"), absolutePrivate);
    settings.setValue(QStringLiteral("Paths/Actressimages"),
                      QStringLiteral("legacy/public/actressimages"));
    settings.setValue(QStringLiteral("Paths/Actorimages"),
                      QStringLiteral("legacy/public/actorimages"));
    settings.setValue(QStringLiteral("Crawler/CoverFetchApiUrl"),
                      QStringLiteral("http://127.0.0.1:61234/custom/image"));
    settings.setValue(QStringLiteral("Crawler/WorkApiBaseUrl"),
                      QStringLiteral("http://127.0.0.1:61234/custom/work/"));
    settings.setValue(QStringLiteral("Crawler/ActressApiBaseUrl"), QString());
    settings.setValue(QStringLiteral("Crawler/TopActressesApiUrl"),
                      QStringLiteral("http://127.0.0.1:61234/custom/top/"));
    settings.sync();

    QCOMPARE(
        QDir::fromNativeSeparators(paths.publicDatabase()),
        QDir::fromNativeSeparators(
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("legacy/public/custom.db"))));
    QCOMPARE(QDir::fromNativeSeparators(paths.privateDatabase()),
             QDir::fromNativeSeparators(absolutePrivate));
    QCOMPARE(QDir::fromNativeSeparators(paths.actressImageDirectory()),
             QDir::fromNativeSeparators(QDir(temporaryDirectory.path()).filePath(
                 QStringLiteral("legacy/public/actressimages"))));
    QCOMPARE(QDir::fromNativeSeparators(paths.actorImageDirectory()),
             QDir::fromNativeSeparators(QDir(temporaryDirectory.path()).filePath(
                 QStringLiteral("legacy/public/actorimages"))));
    const darkeye::CrawlerSettings crawler = darkeye::settings::crawler(paths.settingsFile());
    QCOMPARE(crawler.coverFetchApiUrl, QUrl(QStringLiteral("http://127.0.0.1:61234/custom/image")));
    QCOMPARE(crawler.workApiBaseUrl, QUrl(QStringLiteral("http://127.0.0.1:61234/custom/work")));
    QCOMPARE(crawler.actressApiBaseUrl,
             QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/actress")));
    QCOMPARE(crawler.topActressesApiUrl, QUrl(QStringLiteral("http://127.0.0.1:61234/custom/top")));
}

void AppPathsTest::readsAndWritesTypedSettings()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString settingsFile =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("settings.ini"));

    darkeye::AppSettings app = darkeye::settings::app(settingsFile);
    QCOMPARE(app.themeId, QStringLiteral("LIGHT"));
    QVERIFY(app.firstLaunch);
    QCOMPARE(app.windowSize, QSize(800, 600));
    app.themeId = QStringLiteral("DARK");
    app.customPrimary = QStringLiteral("#123456");
    app.firstLaunch = false;
    app.windowSize = QSize(1440, 900);
    app.videoPaths = {QStringLiteral(" C:/Videos "), QStringLiteral("."), QString()};
    darkeye::settings::saveApp(app, settingsFile);

    const darkeye::AppSettings loadedApp = darkeye::settings::app(settingsFile);
    QCOMPARE(loadedApp.themeId, QStringLiteral("DARK"));
    QCOMPARE(loadedApp.customPrimary, QStringLiteral("#123456"));
    QVERIFY(!loadedApp.firstLaunch);
    QCOMPARE(loadedApp.windowSize, QSize(1440, 900));
    QCOMPARE(loadedApp.videoPaths, QStringList{QStringLiteral("C:/Videos")});

    darkeye::CrawlerSettings crawler = darkeye::settings::crawler(settingsFile);
    crawler.workApiBaseUrl = QUrl(QStringLiteral("http://127.0.0.1:61234/work"));
    crawler.autoStartCollector = true;
    crawler.webDav.remoteRoot = QStringLiteral("/darkeye/backups/");
    darkeye::settings::saveCrawler(crawler, settingsFile);
    const darkeye::CrawlerSettings loadedCrawler = darkeye::settings::crawler(settingsFile);
    QCOMPARE(loadedCrawler.workApiBaseUrl,
             QUrl(QStringLiteral("http://127.0.0.1:61234/work")));
    QVERIFY(loadedCrawler.autoStartCollector);
    QCOMPARE(loadedCrawler.webDav.remoteRoot, QStringLiteral("/darkeye/backups"));

    darkeye::TranslationSettings translation = darkeye::settings::translation(settingsFile);
    QCOMPARE(translation.engine, QStringLiteral("google"));
    QCOMPARE(translation.timeoutSeconds, 12);
    QCOMPARE(translation.retries, 2);
    QCOMPARE(translation.llama.mode, QStringLiteral("cpu"));
    QCOMPARE(translation.llama.gpuLayers, 99);
    QVERIFY(translation.llama.mlock);
    translation.timeoutSeconds = 90;
    translation.llama.contextSize = 4096;
    translation.llama.port = 0;
    translation.llama.mode = QStringLiteral("unsupported");
    darkeye::settings::saveTranslation(translation, settingsFile);
    const darkeye::TranslationSettings loadedTranslation = darkeye::settings::translation(settingsFile);
    QCOMPARE(loadedTranslation.timeoutSeconds, 90);
    QCOMPARE(loadedTranslation.llama.contextSize, 4096);
    QCOMPARE(loadedTranslation.llama.port, 1);
    QCOMPARE(loadedTranslation.llama.mode, QStringLiteral("cpu"));
}

QTEST_MAIN(AppPathsTest)
#include "AppPathsTest.moc"
