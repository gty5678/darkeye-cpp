#include "settings/Paths.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/base/LazyWidget.h"
#include "database/SchemaManager.h"
#include "database/SqliteConnection.h"
#include "database/repositories/PersonRepository.h"
#include "database/repositories/ReferenceRepository.h"
#include "database/repositories/WorkRepository.h"
#include "MainWindow.h"
#include "ui/pages/DashboardPage.h"
#include "ui/pages/HomePage.h"
#include "ui/pages/ManagementPage.h"
#include "ui/pages/PersonDetailPage.h"
#include "ui/pages/PersonPage.h"
#include "ui/pages/StatisticsPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/ShelfPage.h"
#include "ui/pages/WorkPage.h"
#include "ui/components/IdCheckList.h"
#include "ui/components/PersonTransferSelector.h"
#include "ui/components/JsonTransferBar.h"
#include "darkeye_ui/components/Sidebar.h"
#include "ui/pages/management/MakerManagementWidget.h"
#include "ui/pages/management/LabelManagementWidget.h"
#include "darkeye_ui/components/ModernScrollMenu.h"
#include "darkeye_ui/components/LinkCard.h"
#include "ui/components/PathManagement.h"
#include "ui/pages/management/TagManagementWidget.h"
#include "darkeye_ui/components/TokenViews.h"
#include "ui/pages/management/WorkBatchStateWidget.h"
#include "ui/pages/management/AddWorkTabPage3.h"
#include "ui/pages/management/WorkMaintenanceWidget.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
template <typename T>
T *findWorkControl(QObject *root, const QString &controlId)
{
    if (root == nullptr) return nullptr;
    for (T *child : root->findChildren<T *>())
        if (child->property("workControlId").toString() == controlId)
            return child;
    return nullptr;
}
}

class MainWindowSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesAllPrimaryNavigationPages();
};

void MainWindowSmokeTest::exposesAllPrimaryNavigationPages()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString applicationDirectory = temporaryDirectory.path();
    const darkeye::settings::Paths paths(applicationDirectory);
    QVERIFY(paths.ensureRuntimeDirectories());

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    QVERIFY(themeService.setTheme(darkeye::ThemeId::Light));
    darkeye::SqliteConnection publicConnection;
    QString errorMessage;
    QVERIFY2(publicConnection.open(paths.publicDatabase(), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 publicConnection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    darkeye::SqliteConnection privateConnection;
    QVERIFY2(privateConnection.open(paths.privateDatabase(), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 privateConnection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));
    darkeye::WorkRepository repository(publicConnection.database());
    darkeye::Work work;
    work.serialNumber = QStringLiteral("ROUTE-001");
    work.chineseTitle = QStringLiteral("详情路由测试");
    const auto workId = repository.insertComplete(work, {}, {}, {}, &errorMessage);
    QVERIFY2(workId.has_value(), qPrintable(errorMessage));
    darkeye::PersonRepository people(publicConnection.database());
    const auto actressId = people.create(darkeye::PersonKind::Actress, QStringLiteral("路由女演员"),
                                         QStringLiteral("ルート女優"), &errorMessage);
    QVERIFY2(actressId.has_value(), qPrintable(errorMessage));

    darkeye::MainWindow window(themeService, publicConnection.database(),
                               privateConnection.database(), paths);
    window.show();
    QCoreApplication::processEvents();
    auto *sidebar = window.findChild<darkeye::Sidebar *>(QStringLiteral("DesignSidebar"));
    const auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("mainPages"));

    QVERIFY(sidebar != nullptr);
    QVERIFY(pages != nullptr);
    // Python's factory router only adds routes to the stack when first visited.
    QCOMPARE(pages->count(), 1);
    QCOMPARE(sidebar->selectedId(), QStringLiteral("work"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("WorkPage"));
    QVERIFY(window.findChild<darkeye::WorkPage *>(QStringLiteral("WorkPage")) !=
            nullptr);
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("add_masturbation_record"))->shortcut(),
             QKeySequence(QStringLiteral("M")));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("add_quick_work"))->shortcut(),
             QKeySequence(QStringLiteral("W")));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("add_makelove_record"))->shortcut(),
             QKeySequence(QStringLiteral("L")));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("add_sexual_rousal_record"))->shortcut(),
             QKeySequence(QStringLiteral("A")));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("search"))->shortcut(),
             QKeySequence(QStringLiteral("Ctrl+F")));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("capture"))->shortcut(),
             QKeySequence(QStringLiteral("C")));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("allcapture"))->shortcut(),
             QKeySequence(QStringLiteral("Shift+C")));
    QVERIFY(window.findChild<QComboBox *>(QStringLiteral("themeSelector")) == nullptr);

    emit sidebar->itemClicked(QStringLiteral("chart"));
    QVERIFY(qobject_cast<darkeye::StatisticsPage *>(pages->currentWidget()) != nullptr);
    auto *statisticsPage = window.findChild<darkeye::StatisticsPage *>(
        QStringLiteral("StatisticsPage"));
    QVERIFY(statisticsPage != nullptr);
    QVERIFY(statisticsPage->isInitialized());

    emit sidebar->itemClicked(QStringLiteral("setting"));
    auto *themeSelector = window.findChild<QComboBox *>(QStringLiteral("themeSelector"));
    QVERIFY(themeSelector != nullptr);
    QCOMPARE(themeSelector->count(), 7);
    QCOMPARE(themeSelector->itemText(0), QStringLiteral("亮色主题"));
    QCOMPARE(themeSelector->itemText(1), QStringLiteral("暗色主题"));
    QCOMPARE(themeSelector->itemText(3), QStringLiteral("黄色"));
    QCOMPARE(themeSelector->itemText(4), QStringLiteral("绿色"));
    auto *settingsMenu =
        window.findChild<darkeye::ModernScrollMenu *>(QStringLiteral("DesignModernScrollMenu"));
    QVERIFY(settingsMenu);
    QCOMPARE(settingsMenu->sectionCount(), 8);
    auto *aboutPage = window.findChild<darkeye::AboutSettingsPage *>();
    QVERIFY(aboutPage);
    aboutPage->initialize();
    QCOMPARE(aboutPage->findChildren<darkeye::TokenLinkCard *>().size(), 8);
    QVERIFY(aboutPage->findChild<QPushButton *>(QStringLiteral("CheckUpdateButton"))->isEnabled());
    QVERIFY(aboutPage->findChild<QPushButton *>(QStringLiteral("FeedbackButton"))->isEnabled());
    QVERIFY(window.findChild<QWidget *>(QStringLiteral("PrimaryColorRow")));
    auto *shortcutPage = window.findChild<darkeye::ShortcutSettingsPage *>();
    QVERIFY(shortcutPage);
    shortcutPage->initialize();
    const auto loadedShortcutRows =
        window.findChildren<QWidget *>(QRegularExpression(QStringLiteral("^ShortcutSettingRow_")));
    QCOMPARE(loadedShortcutRows.size(), 8);
    auto *helpEditor = window.findChild<QKeySequenceEdit *>(
        QStringLiteral("ShortcutEditor_open_help"));
    auto *helpReset =
        window.findChild<QPushButton *>(QStringLiteral("ShortcutReset_open_help"));
    QVERIFY(helpEditor);
    QVERIFY(helpReset);
    QCOMPARE(helpEditor->keySequence(), QKeySequence(QStringLiteral("H")));
    helpEditor->setKeySequence(QKeySequence(QStringLiteral("F1")));
    QVERIFY(QMetaObject::invokeMethod(helpEditor, "editingFinished", Qt::DirectConnection));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("open_help"))->shortcut(),
             QKeySequence(QStringLiteral("F1")));
    helpReset->click();
    QCOMPARE(helpEditor->keySequence(), QKeySequence(QStringLiteral("H")));
    QCOMPARE(window.findChild<QAction *>(QStringLiteral("open_help"))->shortcut(),
             QKeySequence(QStringLiteral("H")));
    auto *player = window.findChild<QLineEdit *>(QStringLiteral("LocalVideoPlayerEdit"));
    auto *videoPage = window.findChild<darkeye::VideoSettingsPage *>();
    QVERIFY(videoPage);
    videoPage->initialize();
    player = window.findChild<QLineEdit *>(QStringLiteral("LocalVideoPlayerEdit"));
    auto *videoPaths =
        window.findChild<darkeye::MultiplePathManagement *>(QStringLiteral("VideoPathManagement"));
    QVERIFY(player);
    QVERIFY(videoPaths);
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("ScanLocalVideosButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("MatchLocalVideosButton")));
    emit sidebar->itemClicked(QStringLiteral("work"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("WorkPage"));
    auto *workPage = window.findChild<darkeye::WorkPage *>(QStringLiteral("WorkPage"));
    auto *shelfPage = window.findChild<darkeye::ShelfPage *>(QStringLiteral("ShelfPage"));
    auto *actressPage = window.findChild<darkeye::PersonPage *>(QStringLiteral("ActressPage"));
    auto *actressDetail =
        window.findChild<darkeye::PersonDetailPage *>(QStringLiteral("ActressDetailPage"));
    QVERIFY(workPage != nullptr);
    // Like Python's factory router, routes that have not been visited must not
    // construct their pages during MainWindow startup.
    QVERIFY(shelfPage == nullptr);
    QVERIFY(actressPage == nullptr);
    QVERIFY(actressDetail == nullptr);
    window.preloadShelfPage();
    shelfPage = window.findChild<darkeye::ShelfPage *>(QStringLiteral("ShelfPage"));
    QVERIFY(shelfPage != nullptr);
    QVERIFY(shelfPage->isInitialized());
    // Preloading must not navigate away from the page the user is viewing.
    QCOMPARE(pages->currentWidget(), workPage);
    emit workPage->detailRequested(*workId);
    QCOMPARE(pages->currentWidget(), shelfPage);
    QCOMPARE(sidebar->selectedId(), QStringLiteral("shelf"));
    emit sidebar->backwardClicked();
    QCOMPARE(pages->currentWidget(), workPage);
    emit sidebar->forwardClicked();
    QCOMPARE(pages->currentWidget(), shelfPage);
    emit sidebar->itemClicked(QStringLiteral("actress"));
    actressPage = window.findChild<darkeye::PersonPage *>(QStringLiteral("ActressPage"));
    QVERIFY(actressPage != nullptr);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("ActressPage"));
    emit sidebar->itemClicked(QStringLiteral("actor"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("ActorPage"));
    emit sidebar->itemClicked(QStringLiteral("database"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("ManagementPage"));
    auto *managementTabs =
        window.findChild<QTabWidget *>(QStringLiteral("ManagementTabs"));
    QVERIFY(managementTabs != nullptr);
    const auto loadManagementTab = [managementTabs](int index)
    {
        managementTabs->setCurrentIndex(index);
        QCoreApplication::processEvents();
        if (auto *lazy = dynamic_cast<darkeye::LazyWidget *>(managementTabs->widget(index)))
            lazy->initialize();
    };
    // Python creates lazy tab shells up front; selecting each tab performs its
    // first real load. Exercise the same lifecycle before inspecting content.
    loadManagementTab(2);
    loadManagementTab(3);
    loadManagementTab(4);
    QVERIFY(window.findChild<darkeye::MakerManagementWidget *>() != nullptr);
    QCOMPARE(window.findChildren<darkeye::LabelManagementWidget *>().size(), 2);
    loadManagementTab(8);
    loadManagementTab(9);
    loadManagementTab(5);
    const auto stateWidgets = window.findChildren<darkeye::WorkBatchStateWidget *>();
    QCOMPARE(stateWidgets.size(), 2);
    auto *maintenanceWidget = window.findChild<darkeye::WorkMaintenanceWidget *>();
    QVERIFY(maintenanceWidget != nullptr);
    auto *addWorkEditor =
        window.findChild<darkeye::ManagementPage *>(QStringLiteral("ManagementPage"))
            ->findChild<darkeye::AddWorkTabPage3 *>();
    QVERIFY(addWorkEditor != nullptr);
    addWorkEditor->beginCreate();
    auto *addSerial = findWorkControl<QLineEdit>(
        addWorkEditor, QStringLiteral("WorkSerialInput"));
    auto *addTitle = findWorkControl<QPlainTextEdit>(
        addWorkEditor, QStringLiteral("WorkChineseTitleInput"));
    auto *addSave = addWorkEditor->findChild<QPushButton *>(QStringLiteral("WorkSaveButton"));
    QVERIFY(addSerial != nullptr);
    QVERIFY(addTitle != nullptr);
    QVERIFY(addSave != nullptr);
    auto *addActresses = addWorkEditor->findChild<darkeye::PersonTransferSelector *>(
        QStringLiteral("WorkActressSelector"));
    QVERIFY(addActresses != nullptr);
    addActresses->setSelectedIds({*actressId});
    addSerial->setText(QStringLiteral("UI-ADD-001"));
    addTitle->setPlainText(QStringLiteral("界面添加作品"));
    addSave->click();
    QTRY_VERIFY_WITH_TIMEOUT(repository.existsSerial(QStringLiteral("UI-ADD-001")), 500);
    const QList<darkeye::WorkStateRecord> activeWorks = repository.listByDeletedState(false);
    QCOMPARE(activeWorks.size(), 2);
    loadManagementTab(1);
    auto *tagWidget =
        window.findChild<darkeye::TagManagementWidget *>();
    QVERIFY(tagWidget != nullptr);
    emit sidebar->itemClicked(QStringLiteral("setting"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("SettingsPage"));
    themeSelector->setCurrentIndex(1);
    QCOMPARE(themeService.current(), darkeye::ThemeId::Dark);
    emit actressPage->detailRequested(darkeye::PersonKind::Actress, *actressId);
    actressDetail = window.findChild<darkeye::PersonDetailPage *>(
        QStringLiteral("ActressDetailPage"));
    QVERIFY(actressDetail != nullptr);
    QCOMPARE(pages->currentWidget(), actressDetail);
    QCOMPARE(actressDetail->currentPersonId(), *actressId);
    QCOMPARE(sidebar->selectedId(), QStringLiteral("actress"));
    emit actressDetail->editRequested(darkeye::PersonKind::Actress, *actressId);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("ModifyActressPage"));
    QCOMPARE(sidebar->selectedId(), QStringLiteral("actress"));
    QCOMPARE(window.findChildren<QDialog *>().size(), 0);
    QVERIFY(window.windowTitle().startsWith(QStringLiteral("暗之眼 V")));
}

QTEST_MAIN(MainWindowSmokeTest)
#include "MainWindowSmokeTest.moc"
#include <QComboBox>
