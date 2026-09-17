#include "app/AppPaths.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/SqliteConnection.h"
#include "MainWindow.h"
#include "ui/pages/PersonPage.h"
#include "ui/pages/WorkPage.h"
#include "darkeye_ui/components/ColorWheel.h"
#include "darkeye_ui/components/Sidebar.h"
#include "ui/components/FanartStripWidget.h"
#include "ui/dialogs/PersonEditorDialog.h"
#include "ui/pages/DashboardPage.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 3)
        return 2;
    const QString dataDirectory = QDir::cleanPath(application.arguments().at(1));
    const QString outputDirectory = QDir::cleanPath(application.arguments().at(2));
    qputenv("DARKEYE_DATA_DIR", dataDirectory.toUtf8());
    QDir().mkpath(outputDirectory);

    darkeye::AppPaths paths(QCoreApplication::applicationDirPath());
    darkeye::SqliteConnection publicConnection;
    darkeye::SqliteConnection privateConnection;
    QString errorMessage;
    if (!publicConnection.open(paths.publicDatabase(), true, &errorMessage) ||
        !privateConnection.open(paths.privateDatabase(), true, &errorMessage))
    {
        return 3;
    }
    darkeye::ThemeService themes(application);
    darkeye::Settings settings(paths.settingsFile());
    darkeye::MainWindow window(paths, settings, themes, publicConnection.database(),
                               privateConnection.database());
    // Sidebar occupies 62px; this leaves WorkPage at the same 1340px width used
    // by PythonWorkSnapshot.py for a direct visual comparison.
    window.resize(1402, 800);
    window.show();
    QApplication::processEvents();

    auto *sidebar = window.findChild<darkeye::Sidebar *>(QStringLiteral("DesignSidebar"));
    auto *workPage = window.findChild<darkeye::WorkPage *>(QStringLiteral("WorkPage"));
    if (sidebar == nullptr || workPage == nullptr)
        return 4;
    emit sidebar->itemClicked(QStringLiteral("work"));
    auto *workCount = workPage->findChild<QLabel *>(QStringLiteral("WorkCountLabel"));
    auto *workTable = workPage->findChild<QTableWidget *>(QStringLiteral("WorkTable"));
    if (workCount == nullptr || workCount->text() == QStringLiteral("过滤总数:0") ||
        workTable == nullptr || workTable->rowCount() == 0)
    {
        return 28;
    }
    // Python 封面卡片同样异步解码；视觉基线必须等首屏图片稳定后再截取。
    QTest::qWait(2500);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("work-list.png"))))
    {
        return 5;
    }
    if (!workPage->grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("work-list-page.png"))))
    {
        return 9;
    }
    auto *tagPanel = workPage->findChild<QWidget *>(QStringLiteral("WorkTagPanel"));
    auto *tagPanelButton = workPage->findChild<QPushButton *>(
        QStringLiteral("WorkTagPanelButton"));
    auto *tagExpand = workPage->findChild<QPushButton *>(
        QStringLiteral("WorkTagExpandButton"));
    if (tagPanel == nullptr || tagPanelButton == nullptr || tagExpand == nullptr)
        return 29;
    if (!tagPanel->isVisible())
    {
        tagPanelButton->click();
        QApplication::processEvents();
    }
    tagExpand->click();
    QTest::qWait(150);
    if (!window.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("work-tag-selector.png"))))
    {
        return 30;
    }
    tagExpand->click();

    QSqlQuery query(publicConnection.database());
    if (!query.exec(QStringLiteral("SELECT work_id FROM work WHERE is_deleted=0 AND image_url<>'' "
                                   "ORDER BY work_id DESC LIMIT 1")) ||
        !query.next())
    {
        return 6;
    }
    emit workPage->detailRequested(query.value(0).toLongLong());
    QTest::qWait(250);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("work-detail.png"))))
    {
        return 7;
    }
    auto *actressPage = window.findChild<darkeye::PersonPage *>(QStringLiteral("ActressPage"));
    if (actressPage == nullptr)
        return 10;
    emit sidebar->itemClicked(QStringLiteral("actress"));
    QTest::qWait(1500);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("actress-list.png"))))
    {
        return 11;
    }
    QSqlQuery actressQuery(publicConnection.database());
    if (!actressQuery.exec(
            QStringLiteral("SELECT actress_id FROM actress ORDER BY actress_id DESC LIMIT 1")) ||
        !actressQuery.next())
    {
        return 12;
    }
    emit actressPage->detailRequested(darkeye::PersonKind::Actress,
                                      actressQuery.value(0).toLongLong());
    QTest::qWait(500);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("person-detail.png"))))
    {
        return 13;
    }
    emit actressPage->editRequested(darkeye::PersonKind::Actress,
                                    actressQuery.value(0).toLongLong());
    QTest::qWait(500);
    auto *personEditor =
        window.findChild<darkeye::PersonEditorDialog *>(QStringLiteral("PersonEditorDialog"));
    if (personEditor == nullptr ||
        !personEditor->grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("person-editor.png"))))
    {
        return 14;
    }
    personEditor->close();
    emit sidebar->itemClicked(QStringLiteral("database"));
    QTest::qWait(250);
    auto *managementTabs = window.findChild<QTabWidget *>(QStringLiteral("ManagementTabs"));
    if (managementTabs == nullptr ||
        !window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("tag-management.png"))))
    {
        return 15;
    }
    managementTabs->setCurrentIndex(1);
    QTest::qWait(100);
    if (!window.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("tag-type-management.png"))))
    {
        return 16;
    }
    managementTabs->setCurrentIndex(2);
    QTest::qWait(100);
    if (!window.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("maker-prefix-management.png"))))
    {
        return 17;
    }
    managementTabs->setCurrentIndex(3);
    QTest::qWait(100);
    if (!window.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("reference-management.png"))))
    {
        return 18;
    }
    managementTabs->setCurrentIndex(6);
    QTest::qWait(100);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("work-maintenance.png"))))
    {
        return 19;
    }
    managementTabs->setCurrentIndex(7);
    QTest::qWait(100);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("work-soft-delete.png"))))
    {
        return 20;
    }
    managementTabs->setCurrentIndex(8);
    QTest::qWait(100);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("work-recycle-bin.png"))))
    {
        return 21;
    }
    managementTabs->setCurrentIndex(9);
    QTest::qWait(100);
    if (!window.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("add-work-management.png"))))
    {
        return 22;
    }
    auto *workRelationTabs = managementTabs->currentWidget()->findChild<QTabWidget *>(
        QStringLiteral("WorkRelationTabs"));
    if (workRelationTabs == nullptr)
        return 23;
    workRelationTabs->setCurrentIndex(3);
    auto *fanartStrip = managementTabs->currentWidget()->findChild<darkeye::FanartStripWidget *>(
        QStringLiteral("WorkFanartStrip"));
    if (fanartStrip == nullptr)
        return 24;
    fanartStrip->setEntries({{QStringLiteral("https://invalid.example/fanart-1.jpg"),
                              QStringLiteral("DEMO-001-1.jpg"),
                              {}},
                             {QStringLiteral("https://invalid.example/pending.jpg"), {}, {}}});
    fanartStrip->setCanAdd(true);
    QTest::qWait(100);
    if (!window.grab().save(QDir(outputDirectory).filePath(QStringLiteral("add-work-fanart.png"))))
    {
        return 25;
    }
    emit sidebar->itemClicked(QStringLiteral("chart"));
    QTest::qWait(250);
    if (!window.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("personal-statistics.png"))))
    {
        return 26;
    }
    darkeye::DashboardPage dashboard(publicConnection.database(), privateConnection.database());
    dashboard.resize(1000, 700);
    dashboard.show();
    QTest::qWait(100);
    if (!dashboard.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("dashboard-page.png"))))
    {
        return 27;
    }
    darkeye::ColorWheelSimple colorWheel;
    colorWheel.setInitialColor(QStringLiteral("#336699"));
    colorWheel.show();
    QTest::qWait(100);
    if (!colorWheel.grab().save(
            QDir(outputDirectory).filePath(QStringLiteral("component-color-wheel.png"))))
    {
        return 8;
    }
    return 0;
}
