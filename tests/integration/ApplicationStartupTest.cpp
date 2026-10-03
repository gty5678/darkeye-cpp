#include "Application.h"
#include "MainWindow.h"
#include "settings/Settings.h"
#include "ui/dialogs/TermsDialog.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSplashScreen>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

// Application owns QApplication, so this integration test uses its own main.
// Each process checks one consent path using fresh temporary databases.
int main(int argc, char **argv)
{
    const bool accept = argc > 1 && QByteArray(argv[1]) == "accept";
    QTemporaryDir databases;
    if (!databases.isValid()) return 4;
    darkeye::Application application(argc, argv);
    QSplashScreen *splash = nullptr;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == QStringLiteral("startupSplash"))
            splash = qobject_cast<QSplashScreen *>(widget);
    }
    if (!splash || !splash->isVisible() || splash->isEnabled()
        || splash->pixmap().isNull()
        || !splash->testAttribute(Qt::WA_TranslucentBackground)
        || splash->pixmap().toImage().pixelColor(0, 0).alpha() != 0
        || splash->message() != QStringLiteral("正在准备运行环境"))
        return 2;
    if (!splash->grab().save(QDir(QCoreApplication::applicationDirPath())
                               .filePath(QStringLiteral("startup-splash.png"))))
        return 3;

    darkeye::settings::Paths paths;
    QSettings config(paths.settingsFile(), QSettings::IniFormat);
    const QString publicDb = databases.filePath(QStringLiteral("public.db"));
    const QString privateDb = databases.filePath(QStringLiteral("private.db"));
    config.setValue(QStringLiteral("Paths/Database"), publicDb);
    config.setValue(QStringLiteral("Paths/PrivateDatabase"), privateDb);
    config.sync();
    auto appSettings = darkeye::settings::app();
    appSettings.firstLaunch = true;
    appSettings.update.automaticCheck = false;
    darkeye::settings::saveApp(appSettings);

    bool consentSeen = false;
    bool mainWindowSeen = false;
    bool splashClosed = false;
    bool failed = false;
    QTimer observer;
    observer.setInterval(20);
    QObject::connect(&observer, &QTimer::timeout, qApp, [&] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *terms = qobject_cast<darkeye::TermsDialog *>(widget);
                terms && terms->isVisible()) {
                consentSeen = true;
                // Rejecting consent must happen before databases are opened.
                failed |= QFileInfo::exists(publicDb) || QFileInfo::exists(privateDb);
                accept ? terms->accept() : terms->reject();
                return;
            }
            if (auto *window = dynamic_cast<darkeye::MainWindow *>(widget);
                window && window->isVisible() && !mainWindowSeen) {
                mainWindowSeen = true;
                // Check after startup's finish() has returned and the first
                // background services have had a chance to execute.
                QTimer::singleShot(1500, qApp, [&] {
                    splashClosed = true;
                    for (QWidget *topLevel : QApplication::topLevelWidgets())
                        if (topLevel->objectName() == QStringLiteral("startupSplash"))
                            splashClosed = false;
                    QCoreApplication::quit();
                });
            }
        }
    });
    observer.start();
    QTimer::singleShot(20000, qApp, [&] { failed = true; QCoreApplication::exit(5); });
    const int result = application.run();
    if (result != 0 || failed || !consentSeen) return 6;
    if (accept) {
        if (!mainWindowSeen || !splashClosed || darkeye::settings::app().firstLaunch
            || !QFileInfo::exists(publicDb) || !QFileInfo::exists(privateDb))
            return 7;
    } else if (mainWindowSeen || !darkeye::settings::app().firstLaunch
               || QFileInfo::exists(publicDb) || QFileInfo::exists(privateDb)) {
        return 8;
    }
    return 0;
}
