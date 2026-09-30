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
#include <QQuickWidget>
#include <QTemporaryDir>
#include <QtTest>

class ShelfPageTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersAndLoadsVirtualizedShelf();
    void filtersByPrivateScope();
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

QTEST_MAIN(ShelfPageTest)
#include "ShelfPageTest.moc"
