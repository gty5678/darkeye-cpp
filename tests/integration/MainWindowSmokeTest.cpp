#include "settings/Paths.h"
#include "darkeye_ui/theme/ThemeService.h"
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
#include "ui/pages/WorkDetailPage.h"
#include "ui/pages/WorkPage.h"
#include "ui/components/IdCheckList.h"
#include "ui/components/JsonTransferBar.h"
#include "darkeye_ui/components/Sidebar.h"
#include "ui/pages/management/MakerPrefixManagementWidget.h"
#include "darkeye_ui/components/ModernScrollMenu.h"
#include "darkeye_ui/components/LinkCard.h"
#include "ui/components/PathManagement.h"
#include "ui/pages/management/ReferenceManagementWidget.h"
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
    auto *sidebar = window.findChild<darkeye::Sidebar *>(QStringLiteral("DesignSidebar"));
    const auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("mainPages"));

    QVERIFY(sidebar != nullptr);
    QVERIFY(pages != nullptr);
    QCOMPARE(pages->count(), 17);
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
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("StatisticsPage"));
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
    auto *aboutPage = window.findChild<darkeye::AboutSettingsPage *>(
        QStringLiteral("AboutSettingsPage"));
    QVERIFY(aboutPage);
    aboutPage->initialize();
    QCOMPARE(aboutPage->findChildren<darkeye::TokenLinkCard *>().size(), 8);
    QVERIFY(!aboutPage->findChild<QPushButton *>(QStringLiteral("CheckUpdateButton"))->isEnabled());
    QVERIFY(aboutPage->findChild<QPushButton *>(QStringLiteral("FeedbackButton"))->isEnabled());
    QVERIFY(window.findChild<QWidget *>(QStringLiteral("PrimaryColorRow")));
    QVERIFY(window.findChild<QWidget *>(QStringLiteral("greenModeSwitch")));
    auto *shortcutPage = window.findChild<darkeye::ShortcutSettingsPage *>(
        QStringLiteral("ShortcutSettingsPage"));
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
    auto *videoPage = window.findChild<darkeye::VideoSettingsPage *>(
        QStringLiteral("VideoSettingsPage"));
    QVERIFY(videoPage);
    videoPage->initialize();
    player = window.findChild<QLineEdit *>(QStringLiteral("LocalVideoPlayerEdit"));
    auto *videoPaths =
        window.findChild<darkeye::MultiplePathManagement *>(QStringLiteral("VideoPathManagement"));
    QVERIFY(player);
    QVERIFY(videoPaths);
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("ScanLocalVideosButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("MatchLocalVideosButton")));
    player->setText(QStringLiteral(" C:/Player/player.exe "));
    QVERIFY(QMetaObject::invokeMethod(player, "editingFinished", Qt::DirectConnection));
    videoPaths->loadPaths(
        {QStringLiteral("D:/Videos"), QStringLiteral("."), QStringLiteral(" ")});
    videoPaths->table()->item(0, 0)->setText(QStringLiteral("D:/Videos-HD"));
    QSettings videoSettings(paths.settingsFile(), QSettings::IniFormat);
    QCOMPARE(videoSettings.value(QStringLiteral("Video/LocalPlayerExe")).toString(),
             QStringLiteral("C:/Player/player.exe"));
    QCOMPARE(videoSettings.value(QStringLiteral("Paths/Videos")).toString(),
             QStringLiteral("D:/Videos-HD"));
    emit sidebar->itemClicked(QStringLiteral("work"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("WorkPage"));
    auto *workPage = window.findChild<darkeye::WorkPage *>(QStringLiteral("WorkPage"));
    auto *detailPage =
        window.findChild<darkeye::WorkDetailPage *>(QStringLiteral("WorkDetailPage"));
    auto *actressPage = window.findChild<darkeye::PersonPage *>(QStringLiteral("ActressPage"));
    auto *actressDetail =
        window.findChild<darkeye::PersonDetailPage *>(QStringLiteral("ActressDetailPage"));
    QVERIFY(workPage != nullptr);
    QVERIFY(detailPage != nullptr);
    QVERIFY(actressPage != nullptr);
    QVERIFY(actressDetail != nullptr);
    emit workPage->detailRequested(*workId);
    QCOMPARE(pages->currentWidget(), detailPage);
    QCOMPARE(detailPage->currentWorkId(), *workId);
    QCOMPARE(sidebar->selectedId(), QStringLiteral("work"));
    emit sidebar->backwardClicked();
    QCOMPARE(pages->currentWidget(), workPage);
    emit sidebar->forwardClicked();
    QCOMPARE(pages->currentWidget(), detailPage);
    emit sidebar->itemClicked(QStringLiteral("actress"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("ActressPage"));
    emit sidebar->itemClicked(QStringLiteral("actor"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("ActorPage"));
    emit sidebar->itemClicked(QStringLiteral("database"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("ManagementPage"));
    const auto referenceWidgets = window.findChildren<darkeye::ReferenceManagementWidget *>();
    QCOMPARE(referenceWidgets.size(), 3);
    darkeye::ReferenceManagementWidget *makerWidget = nullptr;
    for (darkeye::ReferenceManagementWidget *widget : referenceWidgets)
    {
        if (widget->kind() == darkeye::ReferenceKind::Maker)
            makerWidget = widget;
    }
    QVERIFY(makerWidget != nullptr);
    auto *referenceName =
        makerWidget->findChild<QLineEdit *>(QStringLiteral("ReferenceChineseName"));
    auto *referenceSave =
        makerWidget->findChild<QPushButton *>(QStringLiteral("ReferenceSaveButton"));
    auto *referenceTable =
        makerWidget->findChild<QTableWidget *>(QStringLiteral("ReferenceTable_0"));
    QVERIFY(referenceName != nullptr);
    QVERIFY(referenceSave != nullptr);
    QVERIFY(referenceTable != nullptr);
    referenceName->setText(QStringLiteral("界面新增片商"));
    referenceSave->click();
    QTRY_COMPARE_WITH_TIMEOUT(referenceTable->rowCount(), 1, 500);
    darkeye::ReferenceRepository references(publicConnection.database());
    QVERIFY(references.findByName(darkeye::ReferenceKind::Maker, QStringLiteral("界面新增片商"))
                .has_value());
    QCOMPARE(window.findChildren<darkeye::JsonTransferBar *>().size(), 3);
    auto *softDeleteWidget =
        window.findChild<darkeye::WorkBatchStateWidget *>(QStringLiteral("WorkSoftDeleteWidget"));
    auto *recycleBinWidget =
        window.findChild<darkeye::WorkBatchStateWidget *>(QStringLiteral("WorkRecycleBinWidget"));
    QVERIFY(softDeleteWidget != nullptr);
    QVERIFY(recycleBinWidget != nullptr);
    auto *maintenanceWidget =
        window.findChild<darkeye::WorkMaintenanceWidget *>(QStringLiteral("WorkMaintenanceWidget"));
    QVERIFY(maintenanceWidget != nullptr);
    auto *assignMakerButton =
        maintenanceWidget->findChild<QPushButton *>(QStringLiteral("AssignMakerFromPrefixButton"));
    auto *normalizeCoversButton = maintenanceWidget->findChild<QPushButton *>(
        QStringLiteral("NormalizeCoverFileNamesButton"));
    auto *translationButton = maintenanceWidget->findChild<QPushButton *>(
        QStringLiteral("TranslateMissingWorkFieldsButton"));
    QVERIFY(assignMakerButton != nullptr);
    QVERIFY(normalizeCoversButton != nullptr);
    QVERIFY(translationButton != nullptr);
    QVERIFY(assignMakerButton->isEnabled());
    QVERIFY(normalizeCoversButton->isEnabled());
    QVERIFY(!translationButton->isEnabled());
    auto *softDeleteTable =
        softDeleteWidget->findChild<QTableWidget *>(QStringLiteral("WorkStateTable"));
    auto *recycleBinTable =
        recycleBinWidget->findChild<QTableWidget *>(QStringLiteral("WorkStateTable"));
    QVERIFY(softDeleteTable != nullptr);
    QVERIFY(recycleBinTable != nullptr);
    QCOMPARE(softDeleteTable->rowCount(), 1);
    QCOMPARE(recycleBinTable->rowCount(), 0);
    auto *prefixWidget = window.findChild<darkeye::MakerPrefixManagementWidget *>(
        QStringLiteral("MakerPrefixManagementWidget"));
    QVERIFY(prefixWidget != nullptr);
    auto *prefixInput = prefixWidget->findChild<QLineEdit *>(QStringLiteral("MakerPrefixInput"));
    auto *prefixMaker =
        prefixWidget->findChild<QComboBox *>(QStringLiteral("MakerPrefixMakerSelector"));
    auto *prefixSave =
        prefixWidget->findChild<QPushButton *>(QStringLiteral("MakerPrefixSaveButton"));
    auto *prefixTable = prefixWidget->findChild<QTableWidget *>(QStringLiteral("MakerPrefixTable"));
    QVERIFY(prefixInput != nullptr);
    QVERIFY(prefixMaker != nullptr);
    QVERIFY(prefixSave != nullptr);
    QVERIFY(prefixTable != nullptr);
    const int prefixMakerIndex =
        prefixMaker->findText(QStringLiteral("界面新增片商"), Qt::MatchContains);
    QVERIFY(prefixMakerIndex > 0);
    prefixMaker->setCurrentIndex(prefixMakerIndex);
    prefixInput->setText(QStringLiteral("UIX"));
    prefixSave->click();
    QTRY_COMPARE_WITH_TIMEOUT(prefixTable->rowCount(), 1, 500);
    QCOMPARE(references.listMakerPrefixes().first().prefix, QStringLiteral("UIX"));
    auto *addWorkEditor =
        window.findChild<darkeye::ManagementPage *>(QStringLiteral("ManagementPage"))
            ->findChild<darkeye::AddWorkTabPage3 *>(QStringLiteral("AddWorkTabPage3"));
    QVERIFY(addWorkEditor != nullptr);
    addWorkEditor->beginCreate();
    auto *addSerial = findWorkControl<QLineEdit>(
        addWorkEditor, QStringLiteral("WorkSerialInput"));
    auto *addTitle = findWorkControl<QPlainTextEdit>(
        addWorkEditor, QStringLiteral("WorkChineseTitleInput"));
    auto *addMaker = addWorkEditor->findChild<QComboBox *>(QStringLiteral("WorkMakerSelector"));
    auto *addSave = addWorkEditor->findChild<QPushButton *>(QStringLiteral("WorkSaveButton"));
    QVERIFY(addSerial != nullptr);
    QVERIFY(addTitle != nullptr);
    QVERIFY(addMaker != nullptr);
    QVERIFY(addSave != nullptr);
    auto *addActresses =
        addWorkEditor->findChild<darkeye::IdCheckList *>(QStringLiteral("WorkActressSelector"));
    QVERIFY(addActresses != nullptr);
    addActresses->setSelectedIds({*actressId});
    addSerial->setText(QStringLiteral("UI-ADD-001"));
    addTitle->setPlainText(QStringLiteral("界面添加作品"));
    const int addMakerIndex = addMaker->findText(QStringLiteral("界面新增片商"));
    QVERIFY(addMakerIndex > 0);
    addMaker->setCurrentIndex(addMakerIndex);
    addSave->click();
    QTRY_VERIFY_WITH_TIMEOUT(repository.existsSerial(QStringLiteral("UI-ADD-001")), 500);
    QSqlQuery addedWork(publicConnection.database());
    addedWork.prepare(
        QStringLiteral("SELECT w.maker_id, COUNT(r.work_actress_relation_id) FROM work w "
                       "LEFT JOIN work_actress_relation r ON r.work_id=w.work_id "
                       "WHERE w.serial_number=? GROUP BY w.work_id"));
    addedWork.addBindValue(QStringLiteral("UI-ADD-001"));
    QVERIFY(addedWork.exec());
    QVERIFY(addedWork.next());
    QCOMPARE(addedWork.value(0).toLongLong(), addMaker->currentData().toLongLong());
    QCOMPARE(addedWork.value(1).toInt(), 1);
    const QList<darkeye::WorkStateRecord> activeWorks = repository.listByDeletedState(false);
    QCOMPARE(activeWorks.size(), 2);
    auto *makerFilter = findWorkControl<QComboBox>(
        workPage, QStringLiteral("WorkMakerFilter"));
    QVERIFY(makerFilter != nullptr);
    QVERIFY(makerFilter->findText(QStringLiteral("界面新增片商")) >= 0);
    auto *tagTypeWidget = window.findChild<darkeye::TagTypeManagementWidget *>(
        QStringLiteral("TagTypeManagementWidget"));
    auto *tagWidget =
        window.findChild<darkeye::TagManagementWidget *>(QStringLiteral("TagManagementWidget"));
    QVERIFY(tagTypeWidget != nullptr);
    QVERIFY(tagWidget != nullptr);
    auto *typeName = tagTypeWidget->findChild<QLineEdit *>(QStringLiteral("TagTypeNameInput"));
    auto *typeSave = tagTypeWidget->findChild<QPushButton *>(QStringLiteral("TagTypeSaveButton"));
    auto *typeTable = tagTypeWidget->findChild<QTableWidget *>(QStringLiteral("TagTypeTable"));
    QVERIFY(typeName != nullptr);
    QVERIFY(typeSave != nullptr);
    QVERIFY(typeTable != nullptr);
    typeName->setText(QStringLiteral("界面标签类型"));
    typeSave->click();
    QTRY_COMPARE_WITH_TIMEOUT(typeTable->rowCount(), 1, 500);
    auto *tagName = tagWidget->findChild<QLineEdit *>(QStringLiteral("TagNameInput"));
    auto *tagAliases = tagWidget->findChild<QLineEdit *>(QStringLiteral("TagAliasesInput"));
    auto *tagType = tagWidget->findChild<QComboBox *>(QStringLiteral("TagTypeSelector"));
    auto *tagSave = tagWidget->findChild<QPushButton *>(QStringLiteral("TagSaveButton"));
    auto *tagTable = tagWidget->findChild<QTableWidget *>(QStringLiteral("TagManagementTable"));
    QVERIFY(tagName != nullptr);
    QVERIFY(tagAliases != nullptr);
    QVERIFY(tagType != nullptr);
    QVERIFY(tagSave != nullptr);
    QVERIFY(tagTable != nullptr);
    const int typeIndex = tagType->findText(QStringLiteral("界面标签类型"));
    QVERIFY(typeIndex > 0);
    tagType->setCurrentIndex(typeIndex);
    tagName->setText(QStringLiteral("界面新增标签"));
    tagAliases->setText(QStringLiteral("界面别名一,界面别名二"));
    tagSave->click();
    QTRY_COMPARE_WITH_TIMEOUT(tagTable->rowCount(), 1, 500);
    QCOMPARE(repository.tagOptions().size(), 1);
    QCOMPARE(repository.tagOptions().first().name, QStringLiteral("界面新增标签"));
    emit sidebar->itemClicked(QStringLiteral("setting"));
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("SettingsPage"));
    themeSelector->setCurrentIndex(1);
    QCOMPARE(themeService.current(), darkeye::ThemeId::Dark);
    QSettings settings(paths.settingsFile(), QSettings::IniFormat);
    QCOMPARE(settings.value(QStringLiteral("App/Theme")).toString(), QStringLiteral("DARK"));
    emit actressPage->detailRequested(darkeye::PersonKind::Actress, *actressId);
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
