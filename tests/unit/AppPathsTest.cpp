#include "app/AppPaths.h"
#include "settings/Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class AppPathsTest final : public QObject
{
    Q_OBJECT

private slots:
    void usesPortableLayoutByDefault();
    void prefersPreparedPreviewData();
    void developmentBuildUsesProjectData();
    void createsEveryRuntimeDirectory();
    void honorsLegacyRelativeAndAbsoluteDatabasePaths();
    void readsAndWritesTypedSettings();
};

void AppPathsTest::developmentBuildUsesProjectData()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    QVERIFY(QFile(root.filePath(QStringLiteral("CMakeLists.txt"))).open(QIODevice::WriteOnly));
    QVERIFY(root.mkpath(QStringLiteral("data/public")));
    QFile readme(root.filePath(QStringLiteral("data/README.md")));
    QVERIFY(readme.open(QIODevice::WriteOnly));
    readme.close();
    QFile database(root.filePath(QStringLiteral("data/public/public.db")));
    QVERIFY(database.open(QIODevice::WriteOnly));
    database.close();
    QSettings settings(root.filePath(QStringLiteral("data/settings.ini")),
                       QSettings::IniFormat);
    settings.setValue(QStringLiteral("Paths/Database"),
                      QStringLiteral("data/public/public.db"));
    settings.sync();
    QVERIFY(root.mkpath(QStringLiteral("build/windows-msvc-debug-tests")));

    darkeye::AppPaths paths(root.filePath(QStringLiteral("build/windows-msvc-debug-tests")));
    QCOMPARE(QDir::fromNativeSeparators(paths.dataDirectory()),
             QDir::fromNativeSeparators(root.filePath(QStringLiteral("data"))));
    QCOMPARE(QDir::fromNativeSeparators(paths.publicDatabase()),
             QDir::fromNativeSeparators(
                 root.filePath(QStringLiteral("data/public/public.db"))));
}

void AppPathsTest::usesPortableLayoutByDefault()
{
    darkeye::AppPaths paths(QStringLiteral("C:/portable/Darkeye"));
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
    const darkeye::CrawlerSettings crawler = darkeye::Settings(paths.settingsFile()).crawler();
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

void AppPathsTest::prefersPreparedPreviewData()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString previewPublic =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("preview-data/public"));
    QVERIFY(QDir().mkpath(previewPublic));
    QFile database(QDir(previewPublic).filePath(QStringLiteral("public.db")));
    QVERIFY(database.open(QIODevice::WriteOnly));
    database.close();

    darkeye::AppPaths paths(temporaryDirectory.path());
    QCOMPARE(QDir::fromNativeSeparators(paths.dataDirectory()),
             QDir::fromNativeSeparators(
                 QDir(temporaryDirectory.path()).filePath(QStringLiteral("preview-data"))));
}

void AppPathsTest::createsEveryRuntimeDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    darkeye::AppPaths paths(temporaryDirectory.path());
    QString errorMessage;
    QVERIFY2(paths.ensureRuntimeDirectories(&errorMessage), qPrintable(errorMessage));

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
}

void AppPathsTest::honorsLegacyRelativeAndAbsoluteDatabasePaths()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString absolutePrivate =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("external/private.db"));

    darkeye::AppPaths paths(temporaryDirectory.path());
    QVERIFY(paths.ensureRuntimeDirectories());
    QSettings settings(paths.settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Paths/Database"), QStringLiteral("legacy/public/custom.db"));
    settings.setValue(QStringLiteral("Paths/PrivateDatabase"), absolutePrivate);
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
    const darkeye::CrawlerSettings crawler = darkeye::Settings(paths.settingsFile()).crawler();
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
    darkeye::Settings service(settingsFile);

    darkeye::AppSettings app = service.app();
    QCOMPARE(app.themeId, QStringLiteral("LIGHT"));
    QVERIFY(app.firstLaunch);
    app.themeId = QStringLiteral("DARK");
    app.customPrimary = QStringLiteral("#123456");
    app.firstLaunch = false;
    app.windowSize = QSize(1440, 900);
    app.videoPaths = {QStringLiteral(" C:/Videos "), QStringLiteral("."), QString()};
    service.saveApp(app);

    const darkeye::AppSettings loadedApp = service.app();
    QCOMPARE(loadedApp.themeId, QStringLiteral("DARK"));
    QCOMPARE(loadedApp.customPrimary, QStringLiteral("#123456"));
    QVERIFY(!loadedApp.firstLaunch);
    QCOMPARE(loadedApp.windowSize, QSize(1440, 900));
    QCOMPARE(loadedApp.videoPaths, QStringList{QStringLiteral("C:/Videos")});

    darkeye::CrawlerSettings crawler = service.crawler();
    crawler.workApiBaseUrl = QUrl(QStringLiteral("http://127.0.0.1:61234/work"));
    crawler.autoStartCollector = true;
    service.saveCrawler(crawler);
    const darkeye::CrawlerSettings loadedCrawler = service.crawler();
    QCOMPARE(loadedCrawler.workApiBaseUrl,
             QUrl(QStringLiteral("http://127.0.0.1:61234/work")));
    QVERIFY(loadedCrawler.autoStartCollector);

    darkeye::TranslationSettings translation = service.translation();
    translation.timeoutSeconds = 90;
    translation.llama.contextSize = 4096;
    service.saveTranslation(translation);
    const darkeye::TranslationSettings loadedTranslation = service.translation();
    QCOMPARE(loadedTranslation.timeoutSeconds, 90);
    QCOMPARE(loadedTranslation.llama.contextSize, 4096);
}

QTEST_MAIN(AppPathsTest)
#include "AppPathsTest.moc"
