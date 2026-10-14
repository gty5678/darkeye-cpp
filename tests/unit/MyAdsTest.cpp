#include "ui/layouts/myads/PaneWidget.h"
#include "ui/layouts/myads/WorkspaceWidget.h"

#include <QFile>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMimeData>
#include <QPointer>
#include <QTabBar>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>

using namespace darkeye::myads;

class MyAdsTest final : public QObject
{
    Q_OBJECT
private slots:
    void splitAndSerialize();
    void splitRootAndNormalize();
    void rejectsInvalidTreesAndRatios();
    void savesAtomically();
    void paneTransfersCompleteContent();
    void workspacePreservesContentWhileSplitting();
    void workspaceBatchesProgrammaticSplits();
    void workspaceMovesTabsAcrossPanes();
    void workspaceSavesAndRestoresContent();
    void unknownSchemaDoesNotDestroyWorkspace();
    void invalidLayoutDoesNotDestroyWorkspace();
    void factoryFailureResetsPartialWorkspace();
    void hitTestingMatchesPythonZones();
    void previewGeometryMatchesPythonOverlay();
    void activePaneFollowsUserInteraction();
    void docksThroughGraphicsView_data();
    void docksThroughGraphicsView();
    void preservesChildDropsAndRejectsInvalidDockData();
};

void MyAdsTest::splitAndSerialize()
{
    LayoutTree tree;
    tree.addPaneToRoot(QStringLiteral("left"));
    tree.split(QStringLiteral("left"), Qt::Horizontal, false,
               QStringLiteral("right"), 30);
    QCOMPARE(tree.paneIds(), QList<QString>({QStringLiteral("left"), QStringLiteral("right")}));
    QCOMPARE(tree.root()->sizes, QList<int>({700, 300}));
    const QJsonObject json = tree.toJson();
    QCOMPARE(json.value(QStringLiteral("type")).toString(), QStringLiteral("split"));
    QCOMPARE(json.value(QStringLiteral("children")).toArray().size(), 2);
    const LayoutTree restored = LayoutTree::fromJson(json);
    QCOMPARE(restored.paneIds(), tree.paneIds());
    QCOMPARE(restored.root()->sizes, tree.root()->sizes);
}

void MyAdsTest::splitRootAndNormalize()
{
    LayoutTree tree;
    tree.addPaneToRoot(QStringLiteral("center"));
    tree.split(QStringLiteral("center"), Qt::Vertical, false,
               QStringLiteral("bottom"), 25);
    tree.splitRoot(Qt::Horizontal, true, QStringLiteral("left"), 20);
    QCOMPARE(tree.paneIds(), QList<QString>({QStringLiteral("left"),
                                             QStringLiteral("center"),
                                             QStringLiteral("bottom")}));
    QCOMPARE(tree.root()->orientation, Qt::Horizontal);
    QCOMPARE(tree.root()->sizes, QList<int>({200, 800}));
    QVERIFY(tree.removePane(QStringLiteral("bottom")));
    QCOMPARE(tree.paneIds(), QList<QString>({QStringLiteral("left"),
                                             QStringLiteral("center")}));
}

void MyAdsTest::rejectsInvalidTreesAndRatios()
{
    LayoutTree tree;
    tree.addPaneToRoot(QStringLiteral("one"));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
        tree.addPaneToRoot(QStringLiteral("one")));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
        tree.split(QStringLiteral("one"), Qt::Horizontal, false,
                   QStringLiteral("two"), 0));
    QJsonObject invalidPane{{QStringLiteral("type"), QStringLiteral("pane")},
                            {QStringLiteral("pane_id"), QString()}};
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
        (void)LayoutTree::fromJson(invalidPane));
    QJsonObject invalidSplit{
        {QStringLiteral("type"), QStringLiteral("split")},
        {QStringLiteral("orientation"), QStringLiteral("horizontal")},
        {QStringLiteral("children"), QJsonArray{tree.toJson()}},
        {QStringLiteral("size_ratios"), QJsonArray{-1.0}}};
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
        (void)LayoutTree::fromJson(invalidSplit));
}

void MyAdsTest::savesAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("nested/workspace.json"));
    QJsonObject data{{QStringLiteral("schema_version"), LayoutTree::SchemaVersion}};
    QString error;
    QVERIFY2(saveLayoutAtomic(path, data, &error), qPrintable(error));
    QCOMPARE(loadLayoutFile(path, &error).value(QStringLiteral("schema_version")).toInt(), 1);
}

void MyAdsTest::paneTransfersCompleteContent()
{
    PaneWidget source(QStringLiteral("source"));
    PaneWidget destination(QStringLiteral("destination"));
    auto *label = new QLabel(QStringLiteral("payload"));
    QVERIFY(source.addContent(QStringLiteral("movie"), QStringLiteral("影片"),
                              label, QIcon(), false));
    QCOMPARE(source.currentContentId(), QStringLiteral("movie"));
    const PaneContent content = source.takeContent(QStringLiteral("movie"));
    QCOMPARE(content.widget, label);
    QCOMPARE(content.title, QStringLiteral("影片"));
    QVERIFY(!content.closeable);
    QVERIFY(destination.restoreContent(content));
    QCOMPARE(destination.contentWidget(QStringLiteral("movie")), label);
    QVERIFY(!destination.isContentCloseable(QStringLiteral("movie")));
}

void MyAdsTest::workspacePreservesContentWhileSplitting()
{
    WorkspaceWidget workspace;
    PaneWidget *root = workspace.rootPane();
    QPointer<PaneWidget> rootGuard(root);
    ContentConfig first(QStringLiteral("list"));
    first.setWindowTitle(QStringLiteral("列表"))
         .setWidget(new QLabel(QStringLiteral("A"))).setCloseable(false);
    QVERIFY(workspace.fillPane(root, first));
    QWidget *originalWidget = root->contentWidget(QStringLiteral("list"));
    PaneWidget *right = workspace.split(root, Placement::Right, 35);
    QVERIFY(right);
    QCOMPARE(workspace.pane(QStringLiteral("pane_1")), root);
    QCOMPARE(workspace.findPaneByContentId(QStringLiteral("list"))->contentWidget(
                 QStringLiteral("list")), originalWidget);
    QCOMPARE(workspace.layoutTree().root()->sizes, QList<int>({650, 350}));
    workspace.show();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(rootGuard);
    QCOMPARE(rootGuard->contentCount(), 1);
    QVERIFY(rootGuard->isVisible());

    PaneWidget *bottom = workspace.split(rootGuard, Placement::Bottom);
    QVERIFY(bottom);
    ContentConfig second(QStringLiteral("search"));
    second.setWindowTitle(QStringLiteral("搜索结果"))
          .setWidget(new QLabel(QStringLiteral("B")));
    QVERIFY(workspace.fillPane(rootGuard, second));
    QCoreApplication::processEvents();
    QCOMPARE(rootGuard->contentCount(), 2);
    QTabBar *tabBar = rootGuard->findChild<QTabBar *>(QStringLiteral("MyAdsTabBar"));
    QVERIFY(tabBar);
    QVERIFY(tabBar->isVisible());
    QCOMPARE(tabBar->height(), 32);
}

void MyAdsTest::workspaceBatchesProgrammaticSplits()
{
    WorkspaceWidget workspace;
    QSignalSpy layoutChanges(&workspace, &WorkspaceWidget::layoutChanged);
    PaneWidget *root = workspace.rootPane();

    workspace.beginLayoutUpdate();
    PaneWidget *basic = workspace.split(root, Placement::Right, 70);
    PaneWidget *tag = workspace.split(basic, Placement::Right, 25);
    PaneWidget *text = workspace.split(basic, Placement::Bottom, 42);
    PaneWidget *fanart = workspace.split(tag, Placement::Bottom, 20);
    PaneWidget *force = workspace.split(tag, Placement::Right, 50);
    PaneWidget *actress = workspace.split(root, Placement::Bottom, 50);
    PaneWidget *editor = workspace.split(force, Placement::Bottom, 40);

    QVERIFY(basic);
    QVERIFY(tag);
    QVERIFY(text);
    QVERIFY(fanart);
    QVERIFY(force);
    QVERIFY(actress);
    QVERIFY(editor);
    QCOMPARE(layoutChanges.size(), 0);

    workspace.endLayoutUpdate();
    QCOMPARE(layoutChanges.size(), 1);
    QCOMPARE(workspace.panes().size(), 8);
    QCOMPARE(workspace.pane(root->paneId()), root);
    QCOMPARE(workspace.pane(editor->paneId()), editor);
}

void MyAdsTest::workspaceMovesTabsAcrossPanes()
{
    WorkspaceWidget workspace;
    PaneWidget *left = workspace.rootPane();
    ContentConfig first(QStringLiteral("one"));
    first.setWidget(new QLabel(QStringLiteral("one")));
    ContentConfig second(QStringLiteral("two"));
    second.setWidget(new QLabel(QStringLiteral("two")));
    QVERIFY(workspace.fillPane(left, first));
    QVERIFY(workspace.fillPane(left, second));
    PaneWidget *right = workspace.split(left, Placement::Right);
    QCOMPARE(workspace.layoutTree().root()->sizes, QList<int>({500, 500}));
    ContentConfig third(QStringLiteral("three"));
    third.setWidget(new QLabel(QStringLiteral("three")));
    QVERIFY(workspace.fillPane(right, third));
    QVERIFY(workspace.moveContent(QStringLiteral("pane_1"), QStringLiteral("one"),
                                  right->paneId(), DropZone::Center));
    QCOMPARE(workspace.pane(right->paneId())->contentIds(),
             QStringList({QStringLiteral("three"), QStringLiteral("one")}));
    QVERIFY(workspace.moveContent(QStringLiteral("pane_1"), QStringLiteral("two"),
                                  right->paneId(), DropZone::RootBottom));
    QVERIFY(!workspace.pane(QStringLiteral("pane_1")));
    QCOMPARE(workspace.layoutTree().paneIds().size(), 2);
    QVERIFY(workspace.findPaneByContentId(QStringLiteral("two")));
}

void MyAdsTest::workspaceSavesAndRestoresContent()
{
    QTemporaryDir directory;
    WorkspaceWidget source;
    PaneWidget *root = source.rootPane();
    ContentConfig list(QStringLiteral("movie_list"));
    list.setWindowTitle(QStringLiteral("影片列表"))
        .setWidget(new QLabel(QStringLiteral("list"))).setCloseable(false);
    QVERIFY(source.fillPane(root, list));
    PaneWidget *detailPane = source.split(root, Placement::Right, 30);
    detailPane->setIconOnly(true);
    ContentConfig detail(QStringLiteral("movie_detail"));
    detail.setWindowTitle(QStringLiteral("影片详情"))
          .setWidget(new QLabel(QStringLiteral("detail")));
    QVERIFY(source.fillPane(detailPane, detail));
    ContentConfig auxiliary(QStringLiteral("detail_aux"));
    auxiliary.setWindowTitle(QStringLiteral("附加资料"))
             .setWidget(new QLabel(QStringLiteral("auxiliary")));
    QVERIFY(source.fillPane(detailPane, auxiliary));
    QVERIFY(detailPane->setCurrentContentId(QStringLiteral("movie_detail")));
    const QString path = directory.filePath(QStringLiteral("workspace.json"));
    QString error;
    QVERIFY2(source.saveLayout(path,
        [](const PaneWidget *pane, const QString &id) {
            return QJsonObject{{QStringLiteral("content_id"), id},
                               {QStringLiteral("title"), pane->contentTitle(id)},
                               {QStringLiteral("closeable"), pane->isContentCloseable(id)}};
        }, {}, &error), qPrintable(error));

    WorkspaceWidget restored;
    QVERIFY2(restored.loadLayout(path,
        [](const QJsonObject &value) -> std::optional<ContentConfig> {
            const QString id = value.value(QStringLiteral("content_id")).toString();
            ContentConfig config(id);
            config.setWindowTitle(value.value(QStringLiteral("title")).toString())
                  .setCloseable(value.value(QStringLiteral("closeable")).toBool(true))
                  .setWidget(new QLabel(id));
            return config;
        }, &error), qPrintable(error));
    QCOMPARE(restored.layoutTree().paneIds(), source.layoutTree().paneIds());
    QVERIFY(restored.findPaneByContentId(QStringLiteral("movie_list")));
    PaneWidget *restoredDetail = restored.findPaneByContentId(QStringLiteral("movie_detail"));
    QVERIFY(restoredDetail);
    QVERIFY(restoredDetail->iconOnly());
    QCOMPARE(restoredDetail->contentIds(), QStringList({QStringLiteral("movie_detail"),
                                                        QStringLiteral("detail_aux")}));
    QCOMPARE(restoredDetail->currentContentId(), QStringLiteral("movie_detail"));
}

void MyAdsTest::unknownSchemaDoesNotDestroyWorkspace()
{
    QTemporaryDir directory;
    WorkspaceWidget workspace;
    ContentConfig content(QStringLiteral("keep"));
    content.setWidget(new QLabel(QStringLiteral("keep")));
    QVERIFY(workspace.fillPane(workspace.rootPane(), content));
    const QString path = directory.filePath(QStringLiteral("future.json"));
    QString error;
    QVERIFY(saveLayoutAtomic(path,
        QJsonObject{{QStringLiteral("schema_version"), 999},
                    {QStringLiteral("layout"), workspace.layoutTree().toJson()}}, &error));
    QVERIFY(!workspace.loadLayout(path, {}, &error));
    QVERIFY(workspace.findPaneByContentId(QStringLiteral("keep")));
}

void MyAdsTest::invalidLayoutDoesNotDestroyWorkspace()
{
    QTemporaryDir directory;
    WorkspaceWidget workspace;
    ContentConfig content(QStringLiteral("keep"));
    content.setWidget(new QLabel(QStringLiteral("keep")));
    QVERIFY(workspace.fillPane(workspace.rootPane(), content));
    const QString path = directory.filePath(QStringLiteral("invalid.json"));
    QString error;
    QVERIFY(saveLayoutAtomic(path,
        QJsonObject{{QStringLiteral("schema_version"), 1},
                    {QStringLiteral("layout"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("pane")},
                        {QStringLiteral("pane_id"), QStringLiteral("not_a_root")}}}}, &error));
    QVERIFY(!workspace.loadLayout(path, {}, &error));
    QVERIFY(workspace.findPaneByContentId(QStringLiteral("keep")));
}

void MyAdsTest::factoryFailureResetsPartialWorkspace()
{
    QTemporaryDir directory;
    WorkspaceWidget source;
    ContentConfig content(QStringLiteral("saved"));
    content.setWidget(new QLabel(QStringLiteral("saved")));
    QVERIFY(source.fillPane(source.rootPane(), content));
    const QString path = directory.filePath(QStringLiteral("factory.json"));
    QString error;
    QVERIFY(source.saveLayout(path, [](const PaneWidget *, const QString &id) {
        return QJsonObject{{QStringLiteral("content_id"), id}};
    }, {}, &error));

    WorkspaceWidget restored;
    QVERIFY(!restored.loadLayout(path,
        [](const QJsonObject &) -> std::optional<ContentConfig> {
            throw std::runtime_error("factory failed");
        }, &error));
    QCOMPARE(restored.layoutTree().paneIds(), QList<QString>{QStringLiteral("pane_1")});
    QCOMPARE(restored.rootPane()->contentCount(), 0);
}

void MyAdsTest::hitTestingMatchesPythonZones()
{
    const QRect rectangle(0, 0, 100, 100);
    QCOMPARE(WorkspaceWidget::hitTest(rectangle, QPoint(50, 50)), DropZone::Center);
    QCOMPARE(WorkspaceWidget::hitTest(rectangle, QPoint(50, 10)), DropZone::Top);
    QCOMPARE(WorkspaceWidget::hitTest(rectangle, QPoint(50, 90)), DropZone::Bottom);
    QCOMPARE(WorkspaceWidget::hitTest(rectangle, QPoint(10, 50)), DropZone::Left);
    QCOMPARE(WorkspaceWidget::hitTest(rectangle, QPoint(90, 50)), DropZone::Right);
}

void MyAdsTest::previewGeometryMatchesPythonOverlay()
{
    const QRect paneArea(17, 23, 101, 81);
    QCOMPARE(WorkspaceWidget::previewRectForZone(paneArea, DropZone::Center), paneArea);
    QCOMPARE(WorkspaceWidget::previewRectForZone(paneArea, DropZone::Left),
             QRect(17, 23, 50, 81));
    QCOMPARE(WorkspaceWidget::previewRectForZone(paneArea, DropZone::Right),
             QRect(67, 23, 51, 81));
    QCOMPARE(WorkspaceWidget::previewRectForZone(paneArea, DropZone::Top),
             QRect(17, 23, 101, 40));
    QCOMPARE(WorkspaceWidget::previewRectForZone(paneArea, DropZone::Bottom),
             QRect(17, 63, 101, 41));
}

void MyAdsTest::activePaneFollowsUserInteraction()
{
    WorkspaceWidget workspace;
    PaneWidget *left = workspace.rootPane();
    auto *leftContent = new QLabel(QStringLiteral("left"));
    ContentConfig content(QStringLiteral("left_content"));
    content.setWidget(leftContent);
    QVERIFY(workspace.fillPane(left, content));
    PaneWidget *right = workspace.split(left, Placement::Right);
    QCOMPARE(workspace.activePane(), right);

    QTest::mouseClick(leftContent, Qt::LeftButton);
    QCOMPARE(workspace.activePane(), left);
    PaneWidget *bottom = workspace.split(workspace.activePane(), Placement::Bottom);
    QVERIFY(bottom);
    QCOMPARE(workspace.activePane(), bottom);
}

void MyAdsTest::docksThroughGraphicsView_data()
{
    QTest::addColumn<int>("zone");
    QTest::newRow("center") << int(DropZone::Center);
    QTest::newRow("pane-right") << int(DropZone::Right);
    QTest::newRow("root-bottom") << int(DropZone::RootBottom);
}

void MyAdsTest::docksThroughGraphicsView()
{
    QFETCH(int, zone);
    WorkspaceWidget workspace;
    workspace.resize(800, 600);
    PaneWidget *source = workspace.rootPane();
    const QString sourceId = source->paneId();
    QVERIFY(workspace.fillPane(source, ContentConfig("moving").setWidget(new QLabel("moving"))));
    QVERIFY(workspace.fillPane(source, ContentConfig("stay").setWidget(new QLabel("stay"))));
    PaneWidget *target = workspace.split(source, Placement::Right);
    const QString targetId = target->paneId();
    auto *view = new QGraphicsView;
    auto *scene = new QGraphicsScene(view);
    scene->addText("tag selector");
    view->setScene(scene);
    QVERIFY(workspace.fillPane(target, ContentConfig("tags").setWidget(view)));
    workspace.show();
    QCoreApplication::processEvents();

    QWidget *preview = nullptr;
    for (QWidget *child : workspace.findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        if (child->testAttribute(Qt::WA_TransparentForMouseEvents)) preview = child;
    QVERIFY(preview);
    QWidget *receiver = view->viewport();
    QPoint global = target->mapToGlobal(target->rect().center());
    if (DropZone(zone) == DropZone::Right)
        global = target->mapToGlobal(QPoint(target->width() * 9 / 10, target->height() / 2));
    if (DropZone(zone) == DropZone::RootBottom)
        global = workspace.mapToGlobal(QPoint(workspace.width() * 6 / 10, workspace.height() - 12));
    const QPoint local = receiver->mapFromGlobal(global);
    QVERIFY(receiver->rect().contains(local));
    QMimeData mime;
    mime.setData(MyAdsTabMimeType, QJsonDocument(QJsonObject{
        {"pane_id", sourceId}, {"content_id", "moving"}}).toJson());
    QDragEnterEvent enter(local, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(receiver, &enter);
    QVERIFY(enter.isAccepted());
    QDragMoveEvent move(local, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(receiver, &move);
    QVERIFY(move.isAccepted());
    QVERIFY(preview->isVisible());
    QDragLeaveEvent leave;
    QApplication::sendEvent(receiver, &leave);
    QVERIFY(!preview->isVisible());
    QDragEnterEvent reenter(local, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(receiver, &reenter);
    QVERIFY(reenter.isAccepted());
    QDropEvent drop(local, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(receiver, &drop);
    QVERIFY(drop.isAccepted());
    QCOMPARE(drop.dropAction(), Qt::MoveAction);
    QVERIFY(!preview->isVisible());
    PaneWidget *destination = workspace.findPaneByContentId("moving");
    QVERIFY(destination);
    QCOMPARE(destination->paneId() == targetId, DropZone(zone) == DropZone::Center);
    QCOMPARE(workspace.panes().size(), DropZone(zone) == DropZone::Center ? 2 : 3);
    QCOMPARE(workspace.pane(sourceId)->contentIds(), QStringList{"stay"});
    QCOMPARE(workspace.findPaneByContentId("tags")->contentWidget("tags"), view);
}

void MyAdsTest::preservesChildDropsAndRejectsInvalidDockData()
{
    class DropReceiver final : public QWidget {
    public:
        DropReceiver() { setAcceptDrops(true); }
        int received = 0;
    protected:
        void dragEnterEvent(QDragEnterEvent *event) override {
            ++received;
            event->acceptProposedAction();
        }
    };
    WorkspaceWidget workspace;
    WorkspaceWidget otherWorkspace;
    auto *child = new DropReceiver;
    QVERIFY(workspace.fillPane(workspace.rootPane(), ContentConfig("keep").setWidget(child)));
    workspace.resize(400, 300);
    workspace.show();
    QCoreApplication::processEvents();
    QMimeData mime;
    mime.setText("ordinary child drag");
    QDragEnterEvent ordinary(child->rect().center(), Qt::CopyAction, &mime,
                             Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(child, &ordinary);
    QVERIFY(ordinary.isAccepted());
    QCOMPARE(child->received, 1);
    QDragLeaveEvent leave;
    QApplication::sendEvent(child, &leave);

    mime.setData(MyAdsTabMimeType, "invalid json");
    QDragEnterEvent invalid(child->rect().center(), Qt::MoveAction, &mime,
                            Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(child, &invalid);
    QVERIFY(!invalid.isAccepted());
    QCOMPARE(child->received, 1);
    QVERIFY(workspace.findPaneByContentId("keep"));
    QCOMPARE(otherWorkspace.rootPane()->contentCount(), 0);
}

QTEST_MAIN(MyAdsTest)
#include "MyAdsTest.moc"

