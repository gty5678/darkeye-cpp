#include "ui/pages/WorkPage.h"
#include "app/AppPaths.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/LazyScrollArea.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/SchemaManager.h"
#include "database/SqliteConnection.h"
#include "database/repositories/WorkRepository.h"
#include "ui/components/FanartStripWidget.h"
#include "ui/components/ImageDropWidget.h"
#include "ui/components/WikiTextEdit.h"
#include "ui/components/WorkCard.h"
#include "ui/components/WorkEditorWidget.h"
#include "ui/components/WorkTagSelector.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class WorkPageTest final : public QObject
{
    Q_OBJECT

  private slots:
    void savesDetailsAndRefreshesSelectedRow();
    void loadsOneDatabasePageAtATime();
    void filtersByPrivateScope();
    void persistsViewPreferences();
    void groupsAndSelectsTags();
    void importsCoverAndRollsBackOnDatabaseFailure();
    void matchesPythonWorkListSqlContract();
    void loadsConfiguredRealDatabaseWhenAvailable();
};

void WorkPageTest::loadsConfiguredRealDatabaseWhenAvailable()
{
    const QString dataDirectory = qEnvironmentVariable("DARKEYE_REAL_DATA_DIR").trimmed();
    if (dataDirectory.isEmpty())
        QSKIP("Set DARKEYE_REAL_DATA_DIR to run the real-data WorkPage integration "
              "check");

    qputenv("DARKEYE_DATA_DIR", dataDirectory.toUtf8());
    darkeye::AppPaths paths(QCoreApplication::applicationDirPath());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(paths.publicDatabase(), true, &errorMessage), qPrintable(errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::Settings settings(
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("settings.ini")));
    darkeye::WorkPage page(connection.database(), themes, settings, paths.workCoverDirectory());
    page.initialize();

    auto *count = page.findChild<QLabel *>(QStringLiteral("WorkCountLabel"));
    auto *table = page.findChild<QTableWidget *>(QStringLiteral("WorkTable"));
    auto *cardsWidget = page.findChild<QWidget *>(QStringLiteral("DesignLazyScrollArea"));
    QVERIFY(count != nullptr);
    QVERIFY2(count->text() != QStringLiteral("过滤总数:0"), qPrintable(count->text()));
    QVERIFY(table != nullptr);
    QVERIFY2(table->rowCount() > 0, "The real database returned no WorkPage rows");
    QVERIFY(cardsWidget != nullptr);
    auto *cards = static_cast<darkeye::LazyScrollArea *>(cardsWidget);
    QCOMPARE(cards->itemCount(), table->rowCount());
}

void WorkPageTest::matchesPythonWorkListSqlContract()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage));
    QSqlQuery seed(connection.database());
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO maker(cn_name) VALUES('片商甲')")));
    const qint64 makerId = seed.lastInsertId().toLongLong();
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO maker(cn_name) VALUES('片商乙')")));
    const qint64 otherMakerId = seed.lastInsertId().toLongLong();
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO tag(tag_id,tag_name) VALUES(1,'蓝色')")));
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO tag(tag_id,tag_name) VALUES(2,'橙色')")));

    darkeye::WorkRepository repository(connection.database());
    darkeye::Work highlighted;
    highlighted.serialNumber = QStringLiteral("SQL-001");
    highlighted.chineseTitle = QStringLiteral("中文标题");
    highlighted.japaneseTitle = QStringLiteral("日文标题");
    highlighted.releaseDate = QStringLiteral("2025-01-02");
    highlighted.makerId = makerId;
    QVERIFY(repository.insertComplete(highlighted, {}, {}, {1, 2}, &errorMessage).has_value());
    darkeye::Work noDate;
    noDate.serialNumber = QStringLiteral("SQL-002");
    noDate.chineseTitle = QStringLiteral("无日期");
    noDate.makerId = makerId;
    QVERIFY(repository.insertComplete(noDate, {}, {}, {}, &errorMessage).has_value());
    darkeye::Work other;
    other.serialNumber = QStringLiteral("SQL-003");
    other.chineseTitle = QStringLiteral("其他片商");
    other.releaseDate = QStringLiteral("2024-01-02");
    other.makerId = otherMakerId;
    QVERIFY(repository.insertComplete(other, {}, {}, {}, &errorMessage).has_value());

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::Settings settings(
        QDir(settingsDirectory.path()).filePath(QStringLiteral("settings.ini")));
    darkeye::WorkPage page(connection.database(), themes, settings);
    page.initialize();
    auto *maker = page.findChild<QComboBox *>(QStringLiteral("WorkMakerFilter"));
    auto *sort = page.findChild<QComboBox *>(QStringLiteral("WorkSortSelector"));
    auto *count = page.findChild<QLabel *>(QStringLiteral("WorkCountLabel"));
    auto *table = page.findChild<QTableWidget *>(QStringLiteral("WorkTable"));
    QVERIFY(maker != nullptr);
    QVERIFY(sort != nullptr);
    QVERIFY(count != nullptr);
    QVERIFY(table != nullptr);
    QCOMPARE(count->text(), QStringLiteral("过滤总数:4"));
    QCOMPARE(table->rowCount(), 4);

    const int makerIndex = maker->findData(makerId);
    QVERIFY(makerIndex > 0);
    maker->setCurrentIndex(makerIndex);
    QTRY_COMPARE(count->text(), QStringLiteral("过滤总数:3"));
    QTRY_COMPARE(table->rowCount(), 3);

    sort->setCurrentText(QStringLiteral("发布时间逆序"));
    QTRY_COMPARE(count->text(), QStringLiteral("过滤总数:2"));
    QTRY_COMPARE(table->rowCount(), 2);
    for (int row = 0; row < table->rowCount(); ++row)
        QCOMPARE(table->item(row, 0)->text(), QStringLiteral("SQL-001"));
}

void WorkPageTest::savesDetailsAndRefreshesSelectedRow()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));

    darkeye::WorkRepository repository(connection.database());
    darkeye::Work work;
    work.serialNumber = QStringLiteral("PAGE-001");
    work.chineseTitle = QStringLiteral("原始标题");
    const auto workId = repository.insertComplete(work, {}, {}, {}, &errorMessage);
    QVERIFY2(workId.has_value(), qPrintable(errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::Settings settings(
        QDir(settingsDirectory.path()).filePath(QStringLiteral("settings.ini")));
    darkeye::WorkPage page(connection.database(), themeService, settings);
    page.initialize();
    auto *sortSelector = page.findChild<QComboBox *>(QStringLiteral("WorkSortSelector"));
    auto *tagList = page.findChild<QScrollArea *>(QStringLiteral("WorkTagList"));
    QVERIFY(sortSelector != nullptr);
    QVERIFY(tagList != nullptr);
    QCOMPARE(sortSelector->count(), 13);
    QCOMPARE(sortSelector->currentText(), QStringLiteral("添加逆序"));
    auto *table = page.findChild<QTableWidget *>(QStringLiteral("WorkTable"));
    auto *saveButton = page.findChild<QPushButton *>(QStringLiteral("WorkSaveButton"));
    QVERIFY(table != nullptr);
    QVERIFY(saveButton != nullptr);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("原始标题"));
    table->selectRow(0);

    QLineEdit *titleInput = nullptr;
    for (auto *input : page.findChildren<QLineEdit *>())
    {
        if (input->text() == QStringLiteral("原始标题"))
        {
            titleInput = input;
            break;
        }
    }
    QVERIFY(titleInput != nullptr);
    titleInput->setText(QStringLiteral("页面保存后的标题"));
    auto *videoInput = page.findChild<QLineEdit *>(QStringLiteral("WorkVideoUrlInput"));
    auto *notesInput = page.findChild<darkeye::WikiTextEdit *>(QStringLiteral("WorkNotesInput"));
    QVERIFY(videoInput != nullptr);
    QVERIFY(notesInput != nullptr);
    QVERIFY(notesInput->completerList().contains(QStringLiteral("PAGE-001")));
    videoInput->setText(QStringLiteral("D:/Videos/PAGE-001.mp4"));
    notesInput->setPlainText(QStringLiteral("关联 [[PAGE-001]]"));
    QTest::mouseClick(saveButton, Qt::LeftButton);

    QTRY_COMPARE(table->item(0, 1)->text(), QStringLiteral("页面保存后的标题"));
    const auto saved = repository.findById(*workId, &errorMessage);
    QVERIFY2(saved.has_value(), qPrintable(errorMessage));
    QCOMPARE(saved->chineseTitle, QStringLiteral("页面保存后的标题"));
    QCOMPARE(saved->videoUrl, QStringLiteral("D:/Videos/PAGE-001.mp4"));
    QCOMPARE(saved->notes, QStringLiteral("关联 [[PAGE-001]]"));
}

void WorkPageTest::loadsOneDatabasePageAtATime()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    darkeye::WorkRepository repository(connection.database());
    for (int index = 1; index <= 75; ++index)
    {
        darkeye::Work work;
        work.serialNumber = QStringLiteral("PAGE-%1").arg(index, 3, 10, QLatin1Char('0'));
        QVERIFY(repository.insertComplete(work, {}, {}, {}, &errorMessage).has_value());
    }

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::Settings settings(
        QDir(settingsDirectory.path()).filePath(QStringLiteral("settings.ini")));
    darkeye::WorkPage page(connection.database(), themeService, settings);
    QVERIFY(!page.isInitialized());
    page.initialize();
    QVERIFY(page.isInitialized());
    auto *table = page.findChild<QTableWidget *>(QStringLiteral("WorkTable"));
    auto *scrollArea = page.findChild<QScrollArea *>(QStringLiteral("DesignLazyScrollArea"));
    auto *lazyArea = dynamic_cast<darkeye::LazyScrollArea *>(scrollArea);
    QVERIFY(table != nullptr);
    QVERIFY(lazyArea != nullptr);
    QCOMPARE(table->rowCount(), 70);
    QCOMPARE(lazyArea->itemCount(), 70);
    QCOMPARE(page.findChildren<darkeye::WorkCard *>().size(), 70);

    QVERIFY(lazyArea->loadNextPage());
    QCOMPARE(table->rowCount(), 75);
    QCOMPARE(lazyArea->itemCount(), 75);
    QCOMPARE(page.findChildren<darkeye::WorkCard *>().size(), 75);
    QVERIFY(lazyArea->reachedEnd());
}

void WorkPageTest::filtersByPrivateScope()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection publicConnection;
    darkeye::SqliteConnection privateConnection;
    QString errorMessage;
    QVERIFY2(publicConnection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(privateConnection.open(QDir(temporaryDirectory.path()).filePath("private.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(publicConnection, darkeye::DatabaseKind::Public,
                                                            &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(privateConnection, darkeye::DatabaseKind::Private,
                                                            &errorMessage));

    darkeye::WorkRepository works(publicConnection.database());
    darkeye::Work first;
    first.serialNumber = QStringLiteral("SCOPE-001");
    const auto firstId = works.insertComplete(first, {}, {}, {}, &errorMessage);
    darkeye::Work second;
    second.serialNumber = QStringLiteral("SCOPE-002");
    const auto secondId = works.insertComplete(second, {}, {}, {}, &errorMessage);
    QVERIFY(firstId.has_value());
    QVERIFY(secondId.has_value());
    darkeye::PrivateRepository privateData(privateConnection.database());
    QVERIFY(privateData.addFavoriteWork(*firstId, first.serialNumber, &errorMessage));
    QVERIFY(privateData.addFavoriteWork(*secondId, second.serialNumber, &errorMessage));
    QVERIFY(privateData.addMasturbationRecord(*firstId, first.serialNumber, QStringLiteral("2026-09-10"), QString(), 4,
                                              QString(), &errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::Settings settings(
        QDir(settingsDirectory.path()).filePath(QStringLiteral("settings.ini")));
    darkeye::WorkPage page(publicConnection.database(), themeService, settings, {},
                           privateConnection.database());
    page.initialize();
    auto *scope = page.findChild<QComboBox *>(QStringLiteral("WorkScopeSelector"));
    auto *table = page.findChild<QTableWidget *>(QStringLiteral("WorkTable"));
    QVERIFY(scope != nullptr);
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 2);
    scope->setCurrentText(QStringLiteral("收藏未观看"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("SCOPE-002"));
    scope->setCurrentText(QStringLiteral("已撸过"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("SCOPE-001"));
}

void WorkPageTest::persistsViewPreferences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage));
    const QString settingsFile = QDir(temporaryDirectory.path()).filePath("settings.ini");
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    darkeye::Settings settings(settingsFile);

    {
        darkeye::WorkPage page(connection.database(), themeService, settings);
        page.initialize();
        auto *tagPanel = page.findChild<QWidget *>(QStringLiteral("WorkTagPanel"));
        auto *tagButton = page.findChild<darkeye::IconButton *>(QStringLiteral("WorkTagPanelButton"));
        auto *viewButton = page.findChild<darkeye::IconButton *>(QStringLiteral("WorkCoverViewButton"));
        auto *lazyArea = dynamic_cast<darkeye::LazyScrollArea *>(
            page.findChild<QScrollArea *>(QStringLiteral("DesignLazyScrollArea")));
        QVERIFY(tagPanel != nullptr);
        QVERIFY(tagButton != nullptr);
        QVERIFY(viewButton != nullptr);
        QVERIFY(lazyArea != nullptr);
        QVERIFY(!tagPanel->isHidden());
        QTest::mouseClick(tagButton, Qt::LeftButton);
        QTest::mouseClick(viewButton, Qt::LeftButton);
        QVERIFY(!tagPanel->isVisible());
        QCOMPARE(lazyArea->columnWidth(), 250);
    }

    darkeye::WorkPage restored(connection.database(), themeService, settings);
    restored.initialize();
    auto *restoredPanel = restored.findChild<QWidget *>(QStringLiteral("WorkTagPanel"));
    auto *restoredArea = dynamic_cast<darkeye::LazyScrollArea *>(
        restored.findChild<QScrollArea *>(QStringLiteral("DesignLazyScrollArea")));
    QVERIFY(restoredPanel != nullptr);
    QVERIFY(restoredArea != nullptr);
    QVERIFY(!restoredPanel->isVisible());
    QCOMPARE(restoredArea->columnWidth(), 250);
}

void WorkPageTest::groupsAndSelectsTags()
{
    const QList<darkeye::TagOption> tags{
        {1, QStringLiteral("标签甲"), QStringLiteral("类型一"), QStringLiteral("#336699"), QStringLiteral("说明甲")},
        {2, QStringLiteral("标签乙"), QStringLiteral("类型二"), QStringLiteral("#663399"), QStringLiteral("说明乙")}};
    darkeye::WorkTagSelector selector(tags);
    auto *tabs = selector.findChild<QTabWidget *>();
    const auto availableLists = selector.findChildren<QScrollArea *>(QStringLiteral("WorkAvailableTagList"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(availableLists.size(), 2);
    auto *search = selector.findChild<QLineEdit *>(QStringLiteral("WorkTagSearch"));
    auto *searchResult = selector.findChild<QLabel *>(QStringLiteral("WorkTagSearchResult"));
    auto *searchNext = selector.findChild<QPushButton *>(QStringLiteral("WorkTagSearchNext"));
    auto *expand = selector.findChild<QPushButton *>(QStringLiteral("WorkTagExpandButton"));
    QVERIFY(search != nullptr);
    QVERIFY(searchResult != nullptr);
    QVERIFY(searchNext != nullptr);
    QVERIFY(expand != nullptr);
    search->setText(QStringLiteral("标签"));
    QCOMPARE(searchResult->text(), QStringLiteral("结果 1/2"));
    searchNext->click();
    QCOMPARE(searchResult->text(), QStringLiteral("结果 2/2"));
    expand->click();
    QTest::qWait(350);
    QCOMPARE(selector.width(), 363);
    QSignalSpy selectionChanged(&selector, &darkeye::WorkTagSelector::selectionChanged);
    QWidget *firstTag = selector.findChild<QWidget *>(QStringLiteral("WorkTag_1"));
    QVERIFY(firstTag != nullptr);
    QTest::mouseClick(firstTag, Qt::LeftButton);
    QCOMPARE(selector.selectedIds(), QList<qint64>{1});
    QCOMPARE(selectionChanged.count(), 1);
    QVERIFY(firstTag->isHidden());
    QVERIFY(selector.findChild<QWidget *>(QStringLiteral("WorkSelectedTagFlow"))
                ->findChild<QWidget *>(QStringLiteral("WorkTag_1")) != nullptr);
    selector.clearSelection();
    QVERIFY(selector.selectedIds().isEmpty());
    QCOMPARE(selectionChanged.count(), 2);
    QVERIFY(!firstTag->isHidden());

    selector.setLoader(
        [] { return QList<darkeye::TagOption>{{3, QStringLiteral("新标签"), QStringLiteral("新类型"), {}, {}}}; });
    selector.reloadTags();
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabText(0), QStringLiteral("新类型"));
}

void WorkPageTest::importsCoverAndRollsBackOnDatabaseFailure()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString coverDirectory = QDir(temporaryDirectory.path()).filePath("covers");
    const QString fanartDirectory = QDir(temporaryDirectory.path()).filePath("fanart");
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage));
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    darkeye::WorkRepository repository(connection.database());

    const QString sourcePath = QDir(temporaryDirectory.path()).filePath("source.png");
    QImage source(40, 30, QImage::Format_RGB32);
    source.fill(Qt::blue);
    QVERIFY(source.save(sourcePath));

    darkeye::WorkEditorWidget editor(connection.database(), themeService, coverDirectory, fanartDirectory);
    editor.beginCreate();
    auto *serial = editor.findChild<QLineEdit *>(QStringLiteral("WorkSerialInput"));
    auto *title = editor.findChild<QLineEdit *>(QStringLiteral("WorkChineseTitleInput"));
    auto *imageDrop = editor.findChild<darkeye::ImageDropWidget *>(QStringLiteral("WorkCoverDropWidget"));
    auto *fanart = editor.findChild<darkeye::FanartStripWidget *>(QStringLiteral("WorkFanartStrip"));
    auto *save = editor.findChild<QPushButton *>(QStringLiteral("WorkSaveButton"));
    QVERIFY(serial != nullptr);
    QVERIFY(title != nullptr);
    QVERIFY(imageDrop != nullptr);
    QVERIFY(fanart != nullptr);
    QVERIFY(save != nullptr);
    QSignalSpy savedSpy(&editor, &darkeye::WorkEditorWidget::workSaved);
    serial->setText(QStringLiteral("cover-001"));
    title->setText(QStringLiteral("封面导入"));
    imageDrop->setImagePath(sourcePath);
    imageDrop->setDirty(true);
    QVERIFY(fanart->addLocalImage(sourcePath, &errorMessage));
    fanart->setUrlList({QStringLiteral("https://example.invalid/scene.jpg")});
    QVERIFY(fanart->addLocalImage(sourcePath, &errorMessage));
    QTest::mouseClick(save, Qt::LeftButton);

    QCOMPARE(savedSpy.count(), 1);
    const qint64 savedId = savedSpy.first().at(0).toLongLong();
    const auto saved = repository.findById(savedId, &errorMessage);
    QVERIFY2(saved.has_value(), qPrintable(errorMessage));
    QCOMPARE(saved->imageUrl, QStringLiteral("COVER-001.jpg"));
    const QString savedCover = QDir(coverDirectory).filePath(saved->imageUrl);
    QVERIFY(QFileInfo::exists(savedCover));
    QImage imported(savedCover);
    QVERIFY(!imported.isNull());
    QList<darkeye::FanartEntry> savedFanart;
    QVERIFY(darkeye::FanartStripWidget::parseJson(saved->fanartJson, &savedFanart, &errorMessage));
    QCOMPARE(savedFanart.size(), 2);
    QCOMPARE(savedFanart.at(0).url, QStringLiteral("https://example.invalid/scene.jpg"));
    QVERIFY(savedFanart.at(1).file.startsWith(QStringLiteral("cover001_fa_")));
    const QString importedFanart = QDir(fanartDirectory).filePath(savedFanart.at(1).file);
    QVERIFY(QFileInfo::exists(importedFanart));

    darkeye::Work duplicate;
    duplicate.serialNumber = QStringLiteral("DUPLICATE");
    QVERIFY(repository.insertComplete(duplicate, {}, {}, {}, &errorMessage).has_value());
    QVERIFY(QDir().mkpath(coverDirectory));
    const QString existingCover = QDir(coverDirectory).filePath("DUPLICATE.jpg");
    QImage original(40, 30, QImage::Format_RGB32);
    original.fill(Qt::green);
    QVERIFY(original.save(existingCover, "JPEG", 100));

    editor.beginCreate();
    serial->setText(QStringLiteral("DUPLICATE"));
    imageDrop->setImagePath(sourcePath);
    imageDrop->setDirty(true);
    QVERIFY(fanart->addLocalImage(sourcePath, &errorMessage));
    const int fanartCountBeforeFailure = QDir(fanartDirectory).entryList({QStringLiteral("*.jpg")}, QDir::Files).size();
    QTest::mouseClick(save, Qt::LeftButton);

    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(QDir(fanartDirectory).entryList({QStringLiteral("*.jpg")}, QDir::Files).size(), fanartCountBeforeFailure);
    QImage restored(existingCover);
    QVERIFY(!restored.isNull());
    const QColor restoredColor = restored.pixelColor(restored.width() / 2, restored.height() / 2);
    QVERIFY(restoredColor.green() > restoredColor.blue());

    QVERIFY(editor.loadWork(savedId));
    QVERIFY(fanart->removeEntry(1));
    QTest::mouseClick(save, Qt::LeftButton);
    QCOMPARE(savedSpy.count(), 2);
    QVERIFY(!QFileInfo::exists(importedFanart));
    const auto withoutLocalFanart = repository.findById(savedId, &errorMessage);
    QVERIFY(withoutLocalFanart.has_value());
    QList<darkeye::FanartEntry> remainingFanart;
    QVERIFY(darkeye::FanartStripWidget::parseJson(withoutLocalFanart->fanartJson, &remainingFanart, &errorMessage));
    QCOMPARE(remainingFanart.size(), 1);
    QCOMPARE(remainingFanart.first().url, QStringLiteral("https://example.invalid/scene.jpg"));
}

QTEST_MAIN(WorkPageTest)
#include "WorkPageTest.moc"
