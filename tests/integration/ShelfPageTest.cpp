#include "darkeye_ui/theme/ThemeService.h"
#include "database/SchemaManager.h"
#include "database/SqliteConnection.h"
#include "database/repositories/PrivateRepository.h"
#include "database/repositories/WorkRepository.h"
#include "ui/components/DvdShelfView.h"
#include "ui/pages/ShelfPage.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class ShelfPageTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersAndLoadsVirtualizedShelf();
    void filtersByPrivateScope();
    void heartTogglesPersistedFavorite_data();
    void heartTogglesPersistedFavorite();
    void deletingExpandedWorkResetsScene_data();
    void deletingExpandedWorkResetsScene();
    void routeJumpReplacesAnAlreadyExpandedWork();
    void routeJumpClearsFiltersAndKeepsWorkExpanded_data();
    void routeJumpClearsFiltersAndKeepsWorkExpanded();
};

void ShelfPageTest::filtersAndLoadsVirtualizedShelf()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString error;
    QVERIFY(connection.open(temporaryDirectory.filePath(QStringLiteral("public.db")), false, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &error));
    darkeye::WorkRepository repository(connection.database());
    darkeye::Work first;
    first.serialNumber = QStringLiteral("SHELF-001");
    first.chineseTitle = QStringLiteral("书架目标");
    first.notes = QStringLiteral("保留");
    QVERIFY(repository.insertComplete(first, {}, {}, {}, &error).has_value());
    darkeye::Work second;
    second.serialNumber = QStringLiteral("SHELF-002");
    second.chineseTitle = QStringLiteral("另一作品");
    QVERIFY(repository.insertComplete(second, {}, {}, {}, &error).has_value());

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::ShelfPage page(connection.database(), {}, themes);
    page.initialize();
    auto *count = page.findChild<QLabel *>(QStringLiteral("ShelfCountLabel"));
    auto *view = page.findChild<darkeye::DvdShelfView *>(QStringLiteral("DvdShelfView"));
    auto *serial = page.findChild<QLineEdit *>(QStringLiteral("ShelfSerialFilter"));
    QVERIFY(count != nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(serial != nullptr);
    auto *quickView = view->findChild<QQuickWidget *>();
    QVERIFY(quickView != nullptr);
    QCOMPARE(quickView->status(), QQuickWidget::Ready);
    QCOMPARE(count->text(), QStringLiteral("过滤总数:2"));

    serial->setText(QStringLiteral("002"));
    QTRY_COMPARE(count->text(), QStringLiteral("过滤总数:1"));
}

void ShelfPageTest::filtersByPrivateScope()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection publicConnection;
    darkeye::SqliteConnection privateConnection;
    QString error;
    QVERIFY(publicConnection.open(temporaryDirectory.filePath(QStringLiteral("public.db")), false, &error));
    QVERIFY(privateConnection.open(temporaryDirectory.filePath(QStringLiteral("private.db")), false, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(publicConnection, darkeye::DatabaseKind::Public, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(privateConnection, darkeye::DatabaseKind::Private, &error));
    darkeye::WorkRepository works(publicConnection.database());
    darkeye::Work work;
    work.serialNumber = QStringLiteral("FAVORITE-001");
    const auto id = works.insertComplete(work, {}, {}, {}, &error);
    QVERIFY(id.has_value());
    darkeye::PrivateRepository privateRepository(privateConnection.database());
    QVERIFY(privateRepository.addFavoriteWork(*id, work.serialNumber, &error));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::ShelfPage page(publicConnection.database(), privateConnection.database(), themes);
    page.initialize();
    auto *scope = page.findChild<QComboBox *>(QStringLiteral("ShelfScopeSelector"));
    auto *count = page.findChild<QLabel *>(QStringLiteral("ShelfCountLabel"));
    QVERIFY(scope != nullptr);
    QVERIFY(count != nullptr);
    scope->setCurrentText(QStringLiteral("收藏库范围"));
    QTRY_COMPARE(count->text(), QStringLiteral("过滤总数:1"));
    scope->setCurrentText(QStringLiteral("已撸过"));
    QTRY_COMPARE(count->text(), QStringLiteral("没有查询到数据"));
}

void ShelfPageTest::heartTogglesPersistedFavorite_data()
{
    QTest::addColumn<int>("workCount");
    QTest::addColumn<bool>("initialFavorite");
    QTest::newRow("add-favorite") << 1 << false;
    QTest::newRow("remove-favorite") << 1 << true;
    QTest::newRow("virtualized-work") << 80 << false;
}

void ShelfPageTest::heartTogglesPersistedFavorite()
{
    QFETCH(int, workCount);
    QFETCH(bool, initialFavorite);
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection publicConnection, privateConnection;
    QString error;
    QVERIFY(publicConnection.open(temporaryDirectory.filePath(QStringLiteral("public.db")), false, &error));
    QVERIFY(privateConnection.open(temporaryDirectory.filePath(QStringLiteral("private.db")), false, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(publicConnection, darkeye::DatabaseKind::Public, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(privateConnection, darkeye::DatabaseKind::Private, &error));
    darkeye::WorkRepository works(publicConnection.database());
    darkeye::PrivateRepository favorites(privateConnection.database());
    qint64 workId = 0;
    QString serial;
    for (int index = 0; index < workCount; ++index)
    {
        darkeye::Work work;
        work.serialNumber = QStringLiteral("HEART-%1").arg(index, 3, 10, QLatin1Char('0'));
        const auto id = works.insertComplete(work, {}, {}, {}, &error);
        QVERIFY(id.has_value());
        workId = *id;
        serial = work.serialNumber;
    }
    if (initialFavorite) QVERIFY(favorites.addFavoriteWork(workId, serial, &error));
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::ShelfPage page(publicConnection.database(), privateConnection.database(), themes);
    page.initialize();
    auto *view = page.findChild<darkeye::DvdShelfView *>();
    QVERIFY(view != nullptr);
    darkeye::WorkSearch search;
    search.limit = 0;
    search.order = darkeye::WorkSortOrder::SerialAscending;
    view->setWorks(works.search(search, &error));
    QVERIFY(view->openWork(workId));
    auto *quick = view->findChild<QQuickWidget *>();
    QVERIFY(quick != nullptr);
    QVERIFY(quick->rootObject() != nullptr);
    auto *root = quick->rootObject();
    QTRY_VERIFY(root->property("fullyExpandedDelegateIndex").toInt() >= 0);
    const int virtualIndex = root->property("selectedWorkVirtualIndex").toInt();
    QCOMPARE(virtualIndex, workCount - 1);
    QCOMPARE(view->expandedWorkFavorited(), initialFavorite);
    QSignalSpy stateChanged(view, &darkeye::DvdShelfView::expandedWorkFavoritedChanged);
    QSignalSpy favoriteChanged(&page, &darkeye::ShelfPage::favoriteChanged);
    view->on_heart_clicked(virtualIndex);
    QCOMPARE(favorites.isFavoriteWork(workId, &error), !initialFavorite);
    QCOMPARE(view->expandedWorkFavorited(), !initialFavorite);
    QCOMPARE(stateChanged.count(), 1);
    QCOMPARE(favoriteChanged.count(), 1);
    QCOMPARE(favoriteChanged.first().at(0).toLongLong(), workId);
    QCOMPARE(favoriteChanged.first().at(1).toBool(), !initialFavorite);
    QCOMPARE(root->property("expandedDelegateIndex").toInt() >= 0, true);
    view->on_heart_clicked(virtualIndex);
    QCOMPARE(favorites.isFavoriteWork(workId, &error), initialFavorite);
    QCOMPARE(view->expandedWorkFavorited(), initialFavorite);
    QCOMPARE(favoriteChanged.count(), 2);
    // Match Python: consult the database even when another page changed it.
    if (initialFavorite) QVERIFY(favorites.removeFavoriteWork(workId, &error));
    else QVERIFY(favorites.addFavoriteWork(workId, serial, &error));
    view->on_heart_clicked(virtualIndex);
    QCOMPARE(favorites.isFavoriteWork(workId, &error), initialFavorite);
    QCOMPARE(view->expandedWorkFavorited(), initialFavorite);
    QCOMPARE(favoriteChanged.count(), 3);
    view->refresh_expanded_favorite_state(virtualIndex);
    QCOMPARE(view->expandedWorkFavorited(), initialFavorite);
    view->on_heart_clicked(-1);
    view->on_heart_clicked(workCount);
    QCOMPARE(favoriteChanged.count(), 3);
}

void ShelfPageTest::deletingExpandedWorkResetsScene_data()
{
    QTest::addColumn<int>("workCount");
    QTest::newRow("first-work") << 3;
    QTest::newRow("scrolled-work") << 100;
    QTest::newRow("last-work") << 1;
}

void ShelfPageTest::deletingExpandedWorkResetsScene()
{
    QFETCH(int, workCount);
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString error;
    QVERIFY(connection.open(temporaryDirectory.filePath(QStringLiteral("public.db")), false, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &error));
    darkeye::WorkRepository repository(connection.database());
    qint64 deletedId = 0;
    qint64 remainingId = 0;
    for (int index = 0; index < workCount; ++index)
    {
        darkeye::Work work;
        work.serialNumber = QStringLiteral("DELETE-%1").arg(index, 3, 10, QLatin1Char('0'));
        const auto id = repository.insertComplete(work, {}, {}, {}, &error);
        QVERIFY(id.has_value());
        if (index == 0) remainingId = *id;
        if (index == workCount - 1) deletedId = *id;
    }

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::ShelfPage page(connection.database(), {}, themes);
    page.initialize();
    auto *view = page.findChild<darkeye::DvdShelfView *>();
    auto *count = page.findChild<QLabel *>(QStringLiteral("ShelfCountLabel"));
    QVERIFY(view != nullptr);
    QVERIFY(count != nullptr);
    auto *quickView = view->findChild<QQuickWidget *>();
    QVERIFY(quickView != nullptr);
    auto *root = quickView->rootObject();
    QVERIFY(root != nullptr);
    QVERIFY(page.showWork(deletedId));
    auto *sort = page.findChild<QComboBox *>(QStringLiteral("ShelfSortSelector"));
    QVERIFY(sort != nullptr);
    sort->setCurrentText(QStringLiteral("番号顺序"));
    page.refresh();
    QVERIFY(view->openWork(deletedId));
    QTRY_VERIFY(root->property("fullyExpandedDelegateIndex").toInt() >= 0);
    if (workCount == 100) QVERIFY(view->cameraX() > 1.0);
    QSignalSpy cameraChanged(view, &darkeye::DvdShelfView::cameraXChanged);
    QSignalSpy deleted(&page, &darkeye::ShelfPage::workDeleted);
    // Confirm the actual delete dialog against a temporary database only.
    QTimer::singleShot(0, &page, [] {
        for (QWidget *widget : QApplication::topLevelWidgets())
            if (auto *dialog = qobject_cast<QMessageBox *>(widget))
                dialog->button(QMessageBox::Yes)->click();
    });
    view->on_delete_clicked(root->property("selectedWorkVirtualIndex").toInt());
    QCOMPARE(deleted.count(), 1);
    QCOMPARE(deleted.first().first().toLongLong(), deletedId);
    QCOMPARE(view->cameraX(), 0.0);
    QVERIFY(!cameraChanged.isEmpty());
    QCOMPARE(root->property("selectedDelegateIndex").toInt(), -1);
    QCOMPARE(root->property("expandedDelegateIndex").toInt(), -1);
    QCOMPARE(root->property("fullyExpandedDelegateIndex").toInt(), -1);
    QCOMPARE(root->property("_frozenSelectedDelegateIndex").toInt(), -1);
    QCOMPARE(root->property("_frozenSelectedVirtualIndex").toInt(), -1);
    QCOMPARE(view->currentWorkId(), qint64(0));
    QVERIFY(view->expandedWorkCode().isEmpty());
    QCOMPARE(count->text(), workCount == 1 ? QStringLiteral("没有查询到数据")
                                         : QStringLiteral("过滤总数:%1").arg(workCount - 1));
    page.refresh();
    QTest::qWait(700);
    QCOMPARE(root->property("expandedDelegateIndex").toInt(), -1);
    QCOMPARE(quickView->rootContext()->contextProperty("dvdVisibleStart").toInt(), 0);
    QCOMPARE(quickView->rootContext()->contextProperty("dvdCount").toInt() > 0, workCount > 1);
    QVERIFY(!view->openWork(deletedId));
    if (workCount > 1)
    {
        QVERIFY(view->openWork(remainingId));
        QTRY_COMPARE(view->currentWorkId(), remainingId);
    }
}

void ShelfPageTest::routeJumpReplacesAnAlreadyExpandedWork()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString error;
    QVERIFY(connection.open(temporaryDirectory.filePath(QStringLiteral("public.db")), false, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &error));
    darkeye::WorkRepository repository(connection.database());
    qint64 firstId = -1;
    qint64 secondId = -1;
    QString firstSerial;
    QString secondSerial;
    // These adjacent entries both occupy local delegate index 30 after
    // virtualization.  That is the case which previously retained the old
    // delegate's frozen cover when a route jump arrived.
    for (int index = 0; index < 100; ++index)
    {
        darkeye::Work work;
        work.serialNumber = QStringLiteral("ROUTE-%1").arg(index, 3, 10, QLatin1Char('0'));
        const auto id = repository.insertComplete(work, {}, {}, {}, &error);
        QVERIFY(id.has_value());
        if (index == 40) { firstId = *id; firstSerial = work.serialNumber; }
        if (index == 41) { secondId = *id; secondSerial = work.serialNumber; }
    }

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::ShelfPage page(connection.database(), {}, themes);
    page.initialize();
    auto *view = page.findChild<darkeye::DvdShelfView *>(QStringLiteral("DvdShelfView"));
    QVERIFY(view != nullptr);

    QVERIFY(page.showWork(firstId));
    QTRY_COMPARE(view->expandedWorkCode(), firstSerial);
    QTest::qWait(600);
    QVERIFY(page.showWork(secondId));
    QTRY_COMPARE(view->expandedWorkCode(), secondSerial);
    // Allow the first delayed expand callback to run; it must not restore the
    // previous work after a new route request has arrived.
    QTest::qWait(650);
    QCOMPARE(view->expandedWorkCode(), secondSerial);

    auto updated = repository.findById(secondId, &error);
    QVERIFY(updated.has_value());
    updated->chineseTitle = QStringLiteral("采集后的新标题");
    QVERIFY(repository.updateDetails(*updated, &error));
    page.refreshAfterCapture();
    QTRY_COMPARE(view->expandedWorkTitle(), updated->chineseTitle);
    QTRY_COMPARE(view->currentWorkId(), secondId);
    QTRY_COMPARE(view->expandedWorkCode(), secondSerial);
}

void ShelfPageTest::routeJumpClearsFiltersAndKeepsWorkExpanded_data()
{
    QTest::addColumn<bool>("waitForFilter");
    QTest::newRow("pending-filter") << false;
    QTest::newRow("applied-filter") << true;
}

void ShelfPageTest::routeJumpClearsFiltersAndKeepsWorkExpanded()
{
    QFETCH(bool, waitForFilter);
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    darkeye::SqliteConnection privateConnection;
    QString error;
    QVERIFY(connection.open(temporaryDirectory.filePath(QStringLiteral("public.db")), false, &error));
    QVERIFY(privateConnection.open(temporaryDirectory.filePath(QStringLiteral("private.db")), false, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &error));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(privateConnection, darkeye::DatabaseKind::Private, &error));
    darkeye::WorkRepository repository(connection.database());
    darkeye::Work work;
    work.serialNumber = QStringLiteral("ROUTE-FILTER-001");
    const auto id = repository.insertComplete(work, {}, {}, {}, &error);
    QVERIFY(id.has_value());

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::ShelfPage page(connection.database(), privateConnection.database(), themes);
    page.initialize();
    auto *serial = page.findChild<QLineEdit *>(QStringLiteral("ShelfSerialFilter"));
    auto *scope = page.findChild<QComboBox *>(QStringLiteral("ShelfScopeSelector"));
    auto *sort = page.findChild<QComboBox *>(QStringLiteral("ShelfSortSelector"));
    auto *count = page.findChild<QLabel *>(QStringLiteral("ShelfCountLabel"));
    auto *view = page.findChild<darkeye::DvdShelfView *>(QStringLiteral("DvdShelfView"));
    QVERIFY(serial != nullptr);
    QVERIFY(scope != nullptr);
    QVERIFY(sort != nullptr);
    QVERIFY(count != nullptr);
    QVERIFY(view != nullptr);
    scope->setCurrentIndex(1);
    sort->setCurrentIndex(3);
    serial->setText(QStringLiteral("no-match"));
    if (waitForFilter) QTest::qWait(100);

    QVERIFY(page.showWork(*id));
    QVERIFY(serial->text().isEmpty());
    QCOMPARE(scope->currentIndex(), 0);
    QCOMPARE(sort->currentIndex(), 0);
    QCOMPARE(count->text(), QStringLiteral("过滤总数:1"));
    QTRY_COMPARE(view->expandedWorkCode(), work.serialNumber);
    // Both the 50 ms filter debounce and the 550 ms route expansion must finish
    // without a stale filter reload discarding the destination selection.
    QTest::qWait(700);
    QCOMPARE(view->expandedWorkCode(), work.serialNumber);
    auto *quickView = view->findChild<QQuickWidget *>();
    QVERIFY(quickView != nullptr);
    QVERIFY(quickView->rootObject() != nullptr);
    QCOMPARE(quickView->rootObject()->property("expandedDelegateIndex").toInt(), 0);
}

QTEST_MAIN(ShelfPageTest)
#include "ShelfPageTest.moc"
