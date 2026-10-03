#include "darkeye_ui/theme/ThemeService.h"
#include "database/SchemaManager.h"
#include "database/SqliteConnection.h"
#include "database/repositories/PrivateRepository.h"
#include "database/repositories/WorkRepository.h"
#include "ui/components/DvdShelfView.h"
#include "ui/pages/ShelfPage.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTemporaryDir>
#include <QtTest>

class ShelfPageTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersAndLoadsVirtualizedShelf();
    void filtersByPrivateScope();
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
