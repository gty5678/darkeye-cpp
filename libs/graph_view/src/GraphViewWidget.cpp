#include "graph_view/GraphViewWidget.h"

#include "graph/GraphManager.h"
#include "graph_view/ForceViewRhiWidget.h"

#include <QShowEvent>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace darkeye::graph_view {

GraphViewWidget::GraphViewWidget(graph::GraphManager &manager, QWidget *parent)
    : QWidget(parent), m_manager(manager), m_session(manager.store()),
      m_view(new ForceViewRhiWidget(this)), m_presenter(*m_view)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    const QDir publicDataDirectory(QFileInfo(m_manager.publicDatabasePath()).absolutePath());
    m_actressImageDirectory = publicDataDirectory.filePath(QStringLiteral("actressimages"));
    m_workCoverDirectory = publicDataDirectory.filePath(QStringLiteral("workcovers"));
    m_nodeImage = new QLabel(this);
    m_nodeImage->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_nodeImage->setAlignment(Qt::AlignCenter);
    m_nodeImage->setStyleSheet(QStringLiteral("background: transparent;"));
    m_nodeImage->hide();
    m_nodeImagePositionTimer = new QTimer(this);
    m_nodeImagePositionTimer->setInterval(16);
    connect(m_nodeImagePositionTimer, &QTimer::timeout, this,
            &GraphViewWidget::updateNodeImagePosition);
    connect(m_view, &ForceViewRhiWidget::nodeLeftClicked, this,
            &GraphViewWidget::nodeLeftClicked);
    connect(m_view, &ForceViewRhiWidget::nodeHoveredWithInfo, this,
            [this](const QString &nodeId, float, float, float radius, float, bool dragging) {
                showNodeImage(nodeId, radius, dragging);
            });
    connect(&m_manager, &graph::GraphManager::graphChanged, this,
            &GraphViewWidget::applyManagerChange);
    connect(&m_manager, &graph::GraphManager::loadFailed, this,
            &GraphViewWidget::loadFailed);
}

void GraphViewWidget::setImageDirectories(QString actressDirectory, QString workCoverDirectory)
{
    m_actressImageDirectory = std::move(actressDirectory);
    m_workCoverDirectory = std::move(workCoverDirectory);
}

void GraphViewWidget::setFavoriteOnly(bool enabled)
{
    QString error;
    m_session.setFilterMode(enabled ? graph::GraphFilterMode::FavoriteWorks
                                    : graph::GraphFilterMode::All);
    if (enabled) {
        m_session.setFavoriteWorkIds(m_manager.favoriteWorkNodeIds(&error));
    }
    if (!error.isEmpty()) {
        emit loadFailed(error);
        return;
    }
    reloadView();
}

void GraphViewWidget::setEgoGraph(const QString &centerId, int radius)
{
    m_showingTestGraph = false;
    m_session.setFilterMode(graph::GraphFilterMode::Ego);
    m_session.setEgoCenter(centerId);
    m_session.setEgoRadius(radius);
    m_egoRadius = radius;
    m_style.centerId = centerId;
    reloadView();
}

void GraphViewWidget::showEgoGraph(int radius)
{
    const graph::GraphViewSnapshot snapshot = m_session.snapshot();
    if (snapshot.nodes.isEmpty()) return;
    const auto actress = std::find_if(snapshot.nodes.cbegin(), snapshot.nodes.cend(),
                                      [](const graph::GraphViewNode &node) {
        return node.node.group == QStringLiteral("actress")
               || node.node.id.startsWith(QLatin1Char('a'));
    });
    setEgoGraph((actress == snapshot.nodes.cend() ? snapshot.nodes.cbegin() : actress)->node.id,
                radius);
}

void GraphViewWidget::showAll()
{
    m_showingTestGraph = false;
    m_session.setFilterMode(graph::GraphFilterMode::All);
    m_style.centerId.clear();
    reloadView();
}

void GraphViewWidget::showEmpty()
{
    m_showingTestGraph = false;
    m_session.setFilterMode(graph::GraphFilterMode::Empty);
    m_style.centerId.clear();
    reloadView();
}

void GraphViewWidget::showTestGraph()
{
    constexpr int nodeCount = 2000;
    QVector<int> edges;
    QStringList ids;
    QStringList labels;
    QVector<float> radii;
    QVector<QColor> colors;
    edges.reserve(nodeCount * 4);
    ids.reserve(nodeCount);
    labels.reserve(nodeCount);
    radii.reserve(nodeCount);
    colors.reserve(nodeCount);
    for (int index = 0; index < nodeCount; ++index) {
        ids.append(QStringLiteral("test%1").arg(index));
        labels.append(QStringLiteral("%1").arg(index));
        radii.append(5.0F);
        colors.append(index % 2 == 0 ? m_style.actressColor : m_style.workColor);
        if (index > 0) {
            edges.append(index - 1);
            edges.append(index);
        }
        if (index >= 25) {
            edges.append(index - 25);
            edges.append(index);
        }
    }
    m_showingTestGraph = true;
    m_style.centerId.clear();
    m_view->setGraph(nodeCount, edges, {}, ids, labels, radii, colors);
    QTimer::singleShot(0, m_view, [view = m_view] { view->fitViewToContent(); });
}

void GraphViewWidget::setEgoRadius(int radius)
{
    m_egoRadius = radius;
    m_session.setEgoRadius(radius);
    if (!m_showingTestGraph) reloadView();
}

void GraphViewWidget::setNodeColor(const QString &group, const QColor &color)
{
    if (group == QStringLiteral("actress")) m_style.actressColor = color;
    else if (group == QStringLiteral("work")) m_style.workColor = color;
    else if (group == QStringLiteral("center")) m_style.centerColor = color;
    else if (group == QStringLiteral("default")) m_style.defaultColor = color;
    else return;
    if (!m_showingTestGraph) reloadView();
}

void GraphViewWidget::refreshGraph()
{
    QString error;
    if (!m_manager.refresh(&error) && !error.isEmpty()) {
        emit loadFailed(error);
    }
}

ForceViewRhiWidget *GraphViewWidget::view() const noexcept
{
    return m_view;
}

void GraphViewWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_pendingVisibleLoad && m_loaded) {
        return;
    }

    // QRhiWidget only owns a QRhi after it has become visible. Mirror the
    // Python widget's first-show retry instead of calling setGraph while this
    // stacked page is hidden.
    QTimer::singleShot(0, this, [this] {
        if (isVisible()) {
            loadVisibleSnapshot();
        }
    });
}

void GraphViewWidget::reloadView()
{
    if (!isVisible() || width() <= 0 || height() <= 0) {
        m_pendingVisibleLoad = true;
        return;
    }
    loadVisibleSnapshot();
}

void GraphViewWidget::applyManagerChange()
{
    hideNodeImage();
    if (m_showingTestGraph) return;
    if (m_session.filterMode() == graph::GraphFilterMode::FavoriteWorks) {
        QString error;
        m_session.setFavoriteWorkIds(m_manager.favoriteWorkNodeIds(&error));
        if (!error.isEmpty()) {
            emit loadFailed(error);
            return;
        }
    }
    if (!m_loaded || !isVisible() || width() <= 0 || height() <= 0) {
        // A deferred full snapshot also incorporates every manager update
        // received while this page is hidden.
        m_pendingVisibleLoad = true;
        return;
    }

    // The initial visible load can legitimately happen before GraphManager's
    // asynchronous database load completes.  In that case setGraph(0, ...)
    // clears the QRhi view's PhysicsState.  apply_diff_runtime deliberately
    // has no state to apply an "add node" delta to, so the first real graph
    // would otherwise be dropped and remain blank until a later refresh.
    // Python handles this path by issuing a complete setGraph on its first
    // dataReady notification.  Do the same here, then retain diffs for every
    // subsequent non-empty graph update.
    if (m_view->getNodeIds().isEmpty()) {
        loadVisibleSnapshot();
        return;
    }
    m_presenter.apply(m_session.refresh(), m_style);
}

QString GraphViewWidget::imageFilePath(const QString &nodeId) const
{
    const QString imageName = m_manager.imagePathForNode(nodeId);
    if (imageName.isEmpty()) return {};
    if (QFileInfo(imageName).isAbsolute()) return imageName;
    const QString directory = nodeId.startsWith(QLatin1Char('a'))
        ? m_actressImageDirectory : m_workCoverDirectory;
    return QDir(directory).filePath(imageName);
}

void GraphViewWidget::showNodeImage(const QString &nodeId, float radius, bool dragging)
{
    if (dragging || nodeId.isEmpty()
        || (nodeId.front() != QLatin1Char('a') && nodeId.front() != QLatin1Char('w'))) {
        hideNodeImage();
        return;
    }
    const QString path = imageFilePath(nodeId);
    QImage image = QImageReader(path).read();
    if (image.isNull()) {
        hideNodeImage();
        return;
    }

    const bool actress = nodeId.startsWith(QLatin1Char('a'));
    const QSize targetSize = actress ? QSize(180, 180) : QSize(140, 200);
    if (!actress) {
        const int cropWidth = qRound(image.height() * 0.7);
        if (cropWidth > 0 && image.width() > cropWidth)
            image = image.copy(image.width() - cropWidth, 0, cropWidth, image.height());
        image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        image = image.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    m_nodeImage->setFixedSize(targetSize);
    m_nodeImage->setPixmap(QPixmap::fromImage(image));
    m_hoveredNodeId = nodeId;
    m_hoveredNodeRadius = radius;
    updateNodeImagePosition();
    m_nodeImage->show();
    m_nodeImage->raise();
    m_nodeImagePositionTimer->start();
}

void GraphViewWidget::updateNodeImagePosition()
{
    if (m_hoveredNodeId.isEmpty() || m_nodeImage == nullptr) return;
    const QPointF position = m_view->getNodePosition(m_hoveredNodeId);
    if (position.isNull()) return;
    const float zoom = m_view->getZoom();
    const qreal screenX = (position.x() - m_view->getPanX()) * zoom + m_view->width() / 2.0;
    const qreal screenY = (position.y() - m_view->getPanY()) * zoom + m_view->height() / 2.0;
    m_nodeImage->move(qRound(screenX - m_nodeImage->width() / 2.0),
                      qRound(screenY - m_hoveredNodeRadius * zoom
                             - m_nodeImage->height() - 20.0));
}

void GraphViewWidget::hideNodeImage()
{
    m_hoveredNodeId.clear();
    if (m_nodeImage != nullptr) m_nodeImage->hide();
    if (m_nodeImagePositionTimer != nullptr) m_nodeImagePositionTimer->stop();
}

void GraphViewWidget::loadVisibleSnapshot()
{
    if (m_showingTestGraph) return;
    if (!isVisible() || width() <= 0 || height() <= 0) {
        m_pendingVisibleLoad = true;
        return;
    }
    m_presenter.load(m_session.reload(), m_style);
    m_loaded = true;
    m_pendingVisibleLoad = false;
    QTimer::singleShot(0, m_view, [view = m_view] {
        view->fitViewToContent();
        view->update();
    });
}

} // namespace darkeye::graph_view
