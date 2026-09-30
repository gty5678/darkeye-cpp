#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/TokenControls.h"
#include "graph/GraphManager.h"
#include "graph_view/ForceViewRhiWidget.h"
#include "graph_view/GraphViewWidget.h"
#include "ui/pages/ForceDirectPage.h"

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QtTest>

class GraphPageRenderingTest final : public QObject
{
    Q_OBJECT

private slots:
    void rendersWhenRendererExistsBeforeWindowShow();
    void loadsFirstGraphSnapshotAfterAnInitiallyEmptyView();
};

void GraphPageRenderingTest::rendersWhenRendererExistsBeforeWindowShow()
{
    const QString publicConnectionName = QStringLiteral("graph_page_public");
    const QString privateConnectionName = QStringLiteral("graph_page_private");
    {
        QSqlDatabase publicDatabase =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), publicConnectionName);
        publicDatabase.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(publicDatabase.open());
        QSqlQuery query(publicDatabase);
        const QStringList statements = {
            QStringLiteral("CREATE TABLE actress (actress_id INTEGER PRIMARY KEY)"),
            QStringLiteral("CREATE TABLE actress_name (actress_id INTEGER, cn TEXT, jp TEXT)"),
            QStringLiteral("CREATE TABLE work (work_id INTEGER PRIMARY KEY, serial_number TEXT, "
                           "notes TEXT, series_id INTEGER, is_deleted INTEGER)"),
            QStringLiteral("CREATE TABLE work_actress_relation (work_id INTEGER, actress_id INTEGER)"),
            QStringLiteral("INSERT INTO actress VALUES (1)"),
            QStringLiteral("INSERT INTO actress_name VALUES (1, '演员一', '')"),
            QStringLiteral("INSERT INTO work VALUES (10, 'PAGE-001', '', 1, 0)"),
            QStringLiteral("INSERT INTO work_actress_relation VALUES (10, 1)"),
        };
        for (const QString &statement : statements) {
            QVERIFY2(query.exec(statement), qPrintable(query.lastError().text()));
        }

        QSqlDatabase privateDatabase =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), privateConnectionName);
        privateDatabase.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(privateDatabase.open());
        QSqlQuery privateQuery(privateDatabase);
        QVERIFY(privateQuery.exec(QStringLiteral("CREATE TABLE favorite_work (work_id INTEGER)")));

        auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
        QVERIFY(application != nullptr);
        darkeye::ThemeService themeService(*application);
        QVERIFY(themeService.setTheme(darkeye::ThemeId::Light));
        darkeye::graph::GraphManager manager(publicDatabase, privateDatabase);

        QMainWindow window;
        auto *stack = new QStackedWidget(&window);
        stack->addWidget(new QLabel(QStringLiteral("占位页"), stack));
        auto *page = new darkeye::ForceDirectPage(themeService, manager, stack);
        stack->addWidget(page);
        stack->setCurrentIndex(0);
        auto *view = page->findChild<ForceViewRhiWidget *>();
        QVERIFY(view != nullptr);
        QSignalSpy paintSpy(view, &ForceViewRhiWidget::paintTimeUpdated);
        window.setCentralWidget(stack);
        window.resize(900, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        // Match MainWindow: the page data is initialized on first navigation,
        // while the QRhi renderer itself existed before the window was shown.
        page->initialize();
        stack->setCurrentWidget(page);
        // A QSQLITE :memory: database is local to one connection, whereas
        // GraphManager's scheduled loader uses a worker connection.  Complete
        // this fixture's data initialization on its owning connection instead
        // of expecting the worker to see a different empty database.
        QString initializeError;
        QVERIFY2(manager.initialize(&initializeError), qPrintable(initializeError));
        QTRY_COMPARE_WITH_TIMEOUT(view->getNodeIds().size(), 2, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!paintSpy.isEmpty(), 5000);

        auto *graphWidget = page->findChild<darkeye::graph_view::GraphViewWidget *>();
        QVERIFY(graphWidget != nullptr);
        QTRY_COMPARE(graphWidget->geometry(), page->contentsRect());
    }
    QSqlDatabase::removeDatabase(publicConnectionName);
    QSqlDatabase::removeDatabase(privateConnectionName);
}

void GraphPageRenderingTest::loadsFirstGraphSnapshotAfterAnInitiallyEmptyView()
{
    darkeye::graph::GraphManager manager({}, {});
    QMainWindow window;
    auto *graphWidget = new darkeye::graph_view::GraphViewWidget(manager, &window);
    window.setCentralWidget(graphWidget);
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Let the first-show callback load the empty store.  This reproduces the
    // production race where the page becomes visible before its asynchronous
    // GraphManager database load finishes.
    QTest::qWait(50);
    QVERIFY(graphWidget->view()->getNodeIds().isEmpty());

    QVERIFY(manager.upsertRuntimeNode(
        {QStringLiteral("a1"), QStringLiteral("演员一"), QStringLiteral("actress")}));

    // A complete first snapshot is required here: the QRhi widget has no
    // PhysicsState after setGraph(0, ...), so an add-node diff alone cannot
    // create the initial graph.
    QTRY_COMPARE_WITH_TIMEOUT(graphWidget->view()->getNodeIds().size(), 1, 2000);
}

QTEST_MAIN(GraphPageRenderingTest)
#include "GraphPageRenderingTest.moc"
