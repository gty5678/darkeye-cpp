#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QMainWindow>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QPushButton>
#include <QVector>
#include <QStringList>
#include <QDebug>
#include <QLabel>
#include <QTimer>
#include <algorithm>
#include <cstdlib>
#include <cmath>

#include "ForceViewRhiWidget.h"

/**
 * Standalone test: creates a ForceViewRhiWidget with a random graph.
 * Input box + button allow generating graphs with user-specified node count (default 400).
 */

static void generateRandomGraph(int nNodes, int avgDegree, unsigned int seed,
                                QVector<int>& edges,
                                QVector<float>& pos,
                                QStringList& id,
                                QStringList& labels,
                                QVector<float>& radii,
                                QVector<QColor>& nodeColors)
{
    std::srand(static_cast<unsigned int>(seed));
    // Random initial positions in a circle
    float scale = std::sqrt(static_cast<float>(nNodes)) * 25.0f + 150.0f;
    pos.resize(2 * nNodes);
    id.resize(nNodes);
    labels.resize(nNodes);
    radii.resize(nNodes);
    nodeColors.resize(nNodes);

    for (int i = 0; i < nNodes; ++i) {
        float angle = static_cast<float>(i) / nNodes * 6.2831853f;
        float r = scale * (0.3f + 0.7f * static_cast<float>(std::rand()) / RAND_MAX);
        pos[2 * i]     = r * std::cos(angle);
        pos[2 * i + 1] = r * std::sin(angle);
        id[i] = QString("ID%1").arg(i);
        labels[i] = QString("N%1").arg(i);
        radii[i]  = 4.0f + static_cast<float>(std::rand() % 7);  // 4..10
        nodeColors[i] = QColor(std::rand() % 256, std::rand() % 256, std::rand() % 256);
    }

    // Generate random edges (Poisson-distributed connections per node, matching graph.py)
    edges.clear();
    for (int i = 0; i < nNodes; ++i) {
        // -log(1 - rand) * mean, rounded (exponential approx to Poisson)
        float u = static_cast<float>(std::rand()) / (RAND_MAX + 1.0f);
        int numConnections = static_cast<int>(std::round(-std::log(1.0f - u) * avgDegree));
        for (int k = 0; k < numConnections; ++k) {
            int target = std::rand() % nNodes;
            if (target != i) {
                edges.append(i);
                edges.append(target);
            }
        }
    }

    qDebug() << "Generated graph:" << nNodes << "nodes," << edges.size() / 2 << "edges";
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption smokeOption(QStringLiteral("smoke-test"),
                                   QStringLiteral("验证首帧 QRhi 关系图渲染后退出"));
    parser.addOption(smokeOption);
    parser.process(app);

    // Graph data (generated on demand)
    QVector<int>   edges;
    QVector<float> pos;
    QStringList    ids, labels;
    QVector<float> radii;
    QVector<QColor> nodeColors;

    // Main window
    QMainWindow mainWin;
    mainWin.setWindowTitle("ForceView QRhi Test");
    mainWin.resize(1100, 700);

    QWidget* central = new QWidget(&mainWin);
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    ForceViewRhiWidget* view = new ForceViewRhiWidget(central);
    view->setMinimumSize(400, 300);
    mainLayout->addWidget(view, 1);

    // Right panel: switch graph
    QWidget* panel = new QWidget(central);
    QVBoxLayout* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* groupBox = new QGroupBox("切换图", panel);
    QVBoxLayout* groupLayout = new QVBoxLayout(groupBox);

    QSpinBox* nodeCountSpin = new QSpinBox(groupBox);
    nodeCountSpin->setRange(1, 100000);
    nodeCountSpin->setValue(400);
    nodeCountSpin->setSuffix(" 节点");

    QPushButton* applyBtn = new QPushButton("切换", groupBox);

    groupLayout->addWidget(new QLabel("节点数量:", groupBox));
    groupLayout->addWidget(nodeCountSpin);
    groupLayout->addWidget(applyBtn);
    panelLayout->addWidget(groupBox);

    // 性能信息面板：展示 tick / paint 耗时与 FPS
    QGroupBox* perfGroup = new QGroupBox("性能 (ms / FPS)", panel);
    QVBoxLayout* perfLayout = new QVBoxLayout(perfGroup);
    QLabel* tickLabel  = new QLabel("Tick: -- ms", perfGroup);
    QLabel* paintLabel = new QLabel("Paint: -- ms", perfGroup);
    QLabel* fpsLabel   = new QLabel("FPS: --", perfGroup);
    perfLayout->addWidget(tickLabel);
    perfLayout->addWidget(paintLabel);
    perfLayout->addWidget(fpsLabel);
    perfLayout->addWidget(new QLabel("Layout calculation: CPU", perfGroup));
    perfLayout->addStretch();
    panelLayout->addWidget(perfGroup);

    panelLayout->addStretch();
    mainLayout->addWidget(panel, 0);

    mainWin.setCentralWidget(central);

    auto applyGraph = [view, &edges, &pos, &ids, &labels, &radii, &nodeColors](int nNodes) {
        generateRandomGraph(nNodes, 1, static_cast<unsigned int>(nNodes * 42u), edges, pos, ids, labels, radii, nodeColors);
        view->setGraph(nNodes, edges, pos, ids, labels, radii, nodeColors);
    };

    QObject::connect(applyBtn, &QPushButton::clicked, [applyGraph, nodeCountSpin]() {
        applyGraph(nodeCountSpin->value());
    });
    applyGraph(nodeCountSpin->value());  // Default: 400 nodes on startup

    QObject::connect(view, &ForceViewRhiWidget::nodeLeftClicked,
                     [](const QString& nodeId) { qDebug() << "Left-clicked node: id[" << nodeId << "]"; });
    QObject::connect(view, &ForceViewRhiWidget::nodeHovered,
                     [](const QString& nodeId) { if (!nodeId.isEmpty()) qDebug() << "Hovered node: id:" << nodeId; });

    // 将 FPS、paint / tick 耗时显示在右侧性能面板（使用标签作为上下文对象，保证跨线程安全）
    QObject::connect(view, &ForceViewRhiWidget::fpsUpdated,
                     fpsLabel,
                     [fpsLabel](float fps) {
                         fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
                         qDebug() << "FPS:" << fps;
                     });
    QObject::connect(view, &ForceViewRhiWidget::paintTimeUpdated,
                     paintLabel,
                     [paintLabel, &app, smokeTest = parser.isSet(smokeOption)](float ms) {
                         paintLabel->setText(QString("Paint: %1 ms").arg(ms, 0, 'f', 2));
                         qDebug() << "Paint time (ms):" << ms;
                         if (smokeTest) app.exit(0);
                     });
    QObject::connect(view, &ForceViewRhiWidget::tickTimeUpdated,
                     tickLabel,
                     [tickLabel](float ms) {
                         tickLabel->setText(QString("Tick: %1 ms").arg(ms, 0, 'f', 2));
                         qDebug() << "Tick time (ms):" << ms;
                     });

    mainWin.show();
    if (parser.isSet(smokeOption)) {
        QTimer::singleShot(5000, &app, [&app] {
            qCritical("ForceView QRhi smoke test timed out before the first rendered frame");
            app.exit(2);
        });
    }
    return app.exec();
}



