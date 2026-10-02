#include "ui/layouts/myads/WorkspaceWidget.h"

#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QPainter>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>

namespace darkeye::myads {

class WorkspaceWidget::PreviewOverlay final : public QWidget
{
public:
    explicit PreviewOverlay(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        hide();
    }

    void showPreview(const QRect &rectangle, const DockTheme &theme)
    {
        m_rectangle = rectangle;
        m_theme = theme;
        setGeometry(parentWidget()->rect());
        show();
        raise();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        QColor fill = m_theme.primary;
        fill.setAlpha(std::clamp(m_theme.previewAlpha, 0, 255));
        painter.setBrush(fill);
        painter.setPen(QPen(m_theme.primary, m_theme.previewBorderWidth));
        painter.drawRect(m_rectangle.adjusted(1, 1, -1, -1));
    }

private:
    QRect m_rectangle;
    DockTheme m_theme;
};

ContentConfig::ContentConfig(QString id) : contentId(std::move(id)) {}
ContentConfig &ContentConfig::setWidget(QWidget *value) { widget = value; return *this; }
ContentConfig &ContentConfig::setWindowTitle(const QString &value) { title = value; return *this; }
ContentConfig &ContentConfig::setIcon(const QIcon &value) { icon = value; return *this; }
ContentConfig &ContentConfig::setCloseable(bool value) { closeable = value; return *this; }

WorkspaceWidget::WorkspaceWidget(QWidget *parent, const DockTheme &theme)
    : QWidget(parent), m_theme(theme)
{
    setObjectName(QStringLiteral("MyAdsWorkspace"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tree.addPaneToRoot(QStringLiteral("pane_1"));
    rebuild(false);
    m_preview = new PreviewOverlay(this);
    applyTheme(theme);
    qApp->installEventFilter(this);
}

WorkspaceWidget::~WorkspaceWidget()
{
    if (qApp) qApp->removeEventFilter(this);
}

const LayoutTree &WorkspaceWidget::layoutTree() const noexcept { return m_tree; }
PaneWidget *WorkspaceWidget::pane(const QString &id) const { return m_panes.value(id); }
QList<PaneWidget *> WorkspaceWidget::panes() const { return m_panes.values(); }

PaneWidget *WorkspaceWidget::rootPane() const
{
    const QList<QString> ids = m_tree.paneIds();
    return ids.isEmpty() ? nullptr : m_panes.value(ids.first());
}

PaneWidget *WorkspaceWidget::activePane() const
{
    PaneWidget *result = pane(m_activePaneId);
    return result ? result : rootPane();
}

void WorkspaceWidget::setActivePane(PaneWidget *target)
{
    if (!target || m_panes.value(target->paneId()) != target ||
        m_activePaneId == target->paneId()) return;
    m_activePaneId = target->paneId();
    emit activePaneChanged(m_activePaneId);
}

bool WorkspaceWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        while (widget) {
            if (auto *target = qobject_cast<PaneWidget *>(widget)) {
                setActivePane(target);
                break;
            }
            if (widget == this) break;
            widget = widget->parentWidget();
        }
    }
    return QWidget::eventFilter(watched, event);
}

PaneWidget *WorkspaceWidget::findPaneByContentId(const QString &contentId) const
{
    for (PaneWidget *item : m_panes)
        if (item->contentIds().contains(contentId)) return item;
    return nullptr;
}

QString WorkspaceWidget::nextPaneId() const
{
    const QList<QString> ids = m_tree.paneIds();
    for (int i = 1;; ++i) {
        const QString candidate = QStringLiteral("pane_%1").arg(i);
        if (!ids.contains(candidate)) return candidate;
    }
}

QString WorkspaceWidget::nextContentId()
{
    QString candidate;
    do candidate = QStringLiteral("content_%1").arg(++m_contentCounter);
    while (findPaneByContentId(candidate));
    return candidate;
}

ContentConfig WorkspaceWidget::createContentConfig(const QString &id)
{
    return ContentConfig(id.isEmpty() ? nextContentId() : id);
}

bool WorkspaceWidget::fillPane(PaneWidget *target, const ContentConfig &content)
{
    if (!target || !m_panes.values().contains(target)) return false;
    QString id = content.contentId.isEmpty() ? nextContentId() : content.contentId;
    if (findPaneByContentId(id)) id = nextContentId();
    QWidget *widget = content.widget;
    if (!widget) {
        widget = new QLabel(content.title.isEmpty() ? id : content.title);
        qobject_cast<QLabel *>(widget)->setAlignment(Qt::AlignCenter);
    }
    return target->addContent(id, content.title.isEmpty() ? id : content.title,
                              widget, content.icon, content.closeable);
}

PaneWidget *WorkspaceWidget::createPane(const QString &id)
{
    if (PaneWidget *existing = m_reusablePanes.take(id)) {
        m_panes.insert(id, existing);
        return existing;
    }
    auto *result = new PaneWidget(id, this);
    result->applyTheme(m_theme);
    connect(result, &PaneWidget::paneEmpty, this,
            [this](PaneWidget *empty) { removeEmptyPane(empty->paneId()); });
    connect(result, &PaneWidget::dragMoved, this, &WorkspaceWidget::handleDragMove);
    connect(result, &PaneWidget::tabDropped, this, &WorkspaceWidget::handleDrop);
    connect(result, &PaneWidget::dragFinished, this, [this] {
        if (m_preview) m_preview->hide();
    });
    m_panes.insert(id, result);
    return result;
}

QWidget *WorkspaceWidget::buildNode(const std::shared_ptr<LayoutNode> &node)
{
    if (node->kind == LayoutNode::Kind::Pane) return createPane(node->paneId);
    auto *splitter = new QSplitter(node->orientation, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(2);
    // 延后到鼠标释放才提交尺寸，避免深层 splitter 在父级重排期间以零尺寸进行
    // opaque resize，从而短暂把相邻窗格挤成 0。
    splitter->setOpaqueResize(false);
    for (const auto &child : node->children) splitter->addWidget(buildNode(child));
    const QList<int> requestedSizes = node->sizes;
    const auto validSizes = [splitter](const QList<int> &sizes) {
        return sizes.size() == splitter->count() &&
               std::all_of(sizes.cbegin(), sizes.cend(), [](int size) { return size > 0; });
    };
    if (validSizes(requestedSizes)) splitter->setSizes(requestedSizes);
    // 新建 splitter 时父布局还没有最终几何尺寸。此时 setSizes 可能被 Qt 的一次
    // 零尺寸布局覆盖；在事件循环下一轮重新应用保存的比例。
    if (validSizes(requestedSizes)) {
        QTimer::singleShot(0, splitter, [splitter, requestedSizes] {
            splitter->setSizes(requestedSizes);
        });
    }
    connect(splitter, &QSplitter::splitterMoved, this,
            [splitter, node, validSizes](int, int) {
                const QList<int> sizes = splitter->sizes();
                // 忽略父级重建/隐藏期间的零尺寸通知，不能让它污染可持久化布局。
                if (validSizes(sizes)) node->sizes = sizes;
            });
    return splitter;
}

void WorkspaceWidget::rebuild(bool preserveContents)
{
    m_reusablePanes = preserveContents ? m_panes : QHash<QString, PaneWidget *>();
    m_panes.clear();
    QWidget *oldRoot = m_rootWidget;
    if (oldRoot) layout()->removeWidget(oldRoot);
    m_rootWidget = buildNode(m_tree.root());
    layout()->addWidget(m_rootWidget);
    // The first split reuses the old root PaneWidget inside a new QSplitter.
    // It is no longer the layout root, but it is still a live pane and must not
    // be hidden or queued for deletion.
    const bool oldRootIsReusedPane = qobject_cast<PaneWidget *>(oldRoot) &&
        m_panes.values().contains(qobject_cast<PaneWidget *>(oldRoot));
    if (oldRoot && oldRoot != m_rootWidget && !oldRootIsReusedPane) {
        oldRoot->hide();
        oldRoot->deleteLater();
    }
    m_reusablePanes.clear();
    m_rootWidget->show();
    if (!m_panes.contains(m_activePaneId)) {
        const QList<QString> ids = m_tree.paneIds();
        const QString nextActiveId = ids.isEmpty() ? QString() : ids.first();
        if (m_activePaneId != nextActiveId) {
            m_activePaneId = nextActiveId;
            emit activePaneChanged(m_activePaneId);
        }
    }
    if (m_preview) m_preview->raise();
}

void WorkspaceWidget::beginLayoutUpdate()
{
    ++m_layoutUpdateDepth;
}

void WorkspaceWidget::endLayoutUpdate()
{
    if (m_layoutUpdateDepth <= 0) return;
    --m_layoutUpdateDepth;
    if (m_layoutUpdateDepth != 0 || !m_layoutRebuildPending) return;

    m_layoutRebuildPending = false;
    rebuild(true);
    setActivePane(pane(m_activePaneId));
    emit layoutChanged();
}

PaneWidget *WorkspaceWidget::split(PaneWidget *target, Placement placement, int percent)
{
    if (!target || !m_panes.values().contains(target)) return nullptr;
    const QString id = nextPaneId();
    const bool before = placement == Placement::Left || placement == Placement::Top;
    const Qt::Orientation orientation =
        placement == Placement::Left || placement == Placement::Right
            ? Qt::Horizontal : Qt::Vertical;
    m_tree.split(target->paneId(), orientation, before, id, percent);
    if (m_layoutUpdateDepth > 0) {
        // Python's LayoutTree mutates the live tree incrementally.  During a
        // programmatic batch, register the new pane immediately so later
        // splits can target it, but defer rebuilding the QWidget tree until
        // the batch is complete.
        PaneWidget *created = createPane(id);
        m_activePaneId = id;
        m_layoutRebuildPending = true;
        return created;
    }
    // 拆分会重建树，但已有窗格（及其中的内容 widget）必须继续复用；否则调用方
    // 持有的 PaneWidget 指针会在连续拆分中失效，并可能在销毁工作区时重复释放布局。
    rebuild(true);
    setActivePane(pane(id));
    emit layoutChanged();
    return pane(id);
}

QString WorkspaceWidget::splitPane(const QString &id, Qt::Orientation orientation,
                                   bool before, int percent)
{
    PaneWidget *target = pane(id);
    PaneWidget *created = split(target, orientation == Qt::Horizontal
        ? (before ? Placement::Left : Placement::Right)
        : (before ? Placement::Top : Placement::Bottom), percent);
    return created ? created->paneId() : QString();
}

QString WorkspaceWidget::addPanel(const QString &title, QWidget *content,
                                  const QString &paneId)
{
    PaneWidget *target = paneId.isEmpty() ? rootPane() : pane(paneId);
    if (!target) target = rootPane();
    const QString id = nextContentId();
    ContentConfig config(id);
    config.setWindowTitle(title).setWidget(content);
    return fillPane(target, config) ? target->paneId() : QString();
}

void WorkspaceWidget::removeEmptyPane(const QString &id)
{
    if (m_tree.paneIds().size() <= 1) return;
    if (m_tree.removePane(id)) {
        rebuild();
        emit layoutChanged();
    }
}

DropZone WorkspaceWidget::hitTest(const QRect &rectangle, const QPoint &position)
{
    if (rectangle.width() <= 0 || rectangle.height() <= 0) return DropZone::Center;
    const double x = static_cast<double>(position.x() - rectangle.left()) / rectangle.width();
    const double y = static_cast<double>(position.y() - rectangle.top()) / rectangle.height();
    if (x >= 0.25 && x < 0.75 && y >= 0.25 && y < 0.75) return DropZone::Center;
    if (y < 0.25) return DropZone::Top;
    if (y >= 0.75) return DropZone::Bottom;
    if (x < 0.25) return DropZone::Left;
    return DropZone::Right;
}

DropZone WorkspaceWidget::dropZone(PaneWidget *target, const QPoint &global) const
{
    const QPoint local = mapFromGlobal(global);
    const double x = static_cast<double>(local.x()) / std::max(1, width());
    const double y = static_cast<double>(local.y()) / std::max(1, height());
    if (x >= 0.30 && x < 0.70 && y < 0.05) return DropZone::RootTop;
    if (x >= 0.30 && x < 0.70 && y >= 0.95) return DropZone::RootBottom;
    if (y >= 0.30 && y < 0.70 && x < 0.05) return DropZone::RootLeft;
    if (y >= 0.30 && y < 0.70 && x >= 0.95) return DropZone::RootRight;
    return hitTest(target->rect(), target->mapFromGlobal(global));
}

QRect WorkspaceWidget::previewRect(PaneWidget *target, DropZone zone) const
{
    const bool root = zone == DropZone::RootLeft || zone == DropZone::RootRight ||
                      zone == DropZone::RootTop || zone == DropZone::RootBottom;
    QRect area = root ? rect() : QRect(target->mapTo(this, QPoint()), target->size());
    return previewRectForZone(area, zone);
}

QRect WorkspaceWidget::previewRectForZone(const QRect &area, DropZone zone)
{
    const int halfWidth = area.width() / 2;
    const int halfHeight = area.height() / 2;
    switch (zone) {
    case DropZone::Left: case DropZone::RootLeft:
        return QRect(area.left(), area.top(), halfWidth, area.height());
    case DropZone::Right: case DropZone::RootRight:
        return QRect(area.left() + halfWidth, area.top(),
                     area.width() - halfWidth, area.height());
    case DropZone::Top: case DropZone::RootTop:
        return QRect(area.left(), area.top(), area.width(), halfHeight);
    case DropZone::Bottom: case DropZone::RootBottom:
        return QRect(area.left(), area.top() + halfHeight,
                     area.width(), area.height() - halfHeight);
    case DropZone::Center:
        // Python SplitPreviewOverlay highlights the complete target pane for a merge.
        return area;
    }
    return {};
}

void WorkspaceWidget::handleDragMove(PaneWidget *target, const QPoint &global)
{
    const DropZone zone = dropZone(target, global);
    showDropPreview(target, zone);
}

void WorkspaceWidget::showDropPreview(PaneWidget *target, DropZone zone)
{
    if (!target && zone != DropZone::RootLeft && zone != DropZone::RootRight &&
        zone != DropZone::RootTop && zone != DropZone::RootBottom) return;
    PaneWidget *geometryTarget = target ? target : rootPane();
    if (geometryTarget) m_preview->showPreview(previewRect(geometryTarget, zone), m_theme);
}

void WorkspaceWidget::hideDropPreview()
{
    m_preview->hide();
}

void WorkspaceWidget::handleDrop(PaneWidget *target, const QPoint &global,
                                 const QString &sourcePaneId, const QString &contentId)
{
    moveContent(sourcePaneId, contentId, target->paneId(), dropZone(target, global));
    hideDropPreview();
}

bool WorkspaceWidget::moveContent(const QString &sourcePaneId, const QString &contentId,
                                  const QString &targetPaneId, DropZone zone)
{
    PaneWidget *source = pane(sourcePaneId);
    PaneWidget *target = pane(targetPaneId);
    if (!source || !target || (source == target && zone == DropZone::Center)) return false;
    const QJsonObject snapshot = m_tree.toJson();
    PaneContent content = source->takeContent(contentId);
    if (!content.widget) return false;
    QString destinationId = targetPaneId;
    try {
        if (zone != DropZone::Center) {
            destinationId = nextPaneId();
            const bool root = zone == DropZone::RootLeft || zone == DropZone::RootRight ||
                              zone == DropZone::RootTop || zone == DropZone::RootBottom;
            const bool before = zone == DropZone::Left || zone == DropZone::Top ||
                                zone == DropZone::RootLeft || zone == DropZone::RootTop;
            const Qt::Orientation orientation =
                zone == DropZone::Left || zone == DropZone::Right ||
                zone == DropZone::RootLeft || zone == DropZone::RootRight
                    ? Qt::Horizontal : Qt::Vertical;
            if (root) m_tree.splitRoot(orientation, before, destinationId);
            else m_tree.split(targetPaneId, orientation, before, destinationId);
            rebuild();
            source = pane(sourcePaneId);
            target = pane(destinationId);
        }
        if (!target || !target->addContent(content.contentId, content.title, content.widget,
                                           content.icon, content.closeable))
            throw std::runtime_error("drop failed");
        if (source && source->contentCount() == 0 && m_tree.paneIds().size() > 1) {
            (void)m_tree.removePane(sourcePaneId);
            rebuild();
        }
        emit contentMoved(contentId, destinationId);
        setActivePane(pane(destinationId));
        emit layoutChanged();
        return true;
    } catch (...) {
        m_tree = LayoutTree::fromJson(snapshot);
        rebuild();
        if (PaneWidget *restoredSource = pane(sourcePaneId)) restoredSource->restoreContent(content);
        return false;
    }
}

void WorkspaceWidget::resetToSingleEmptyPane()
{
    m_tree = LayoutTree();
    m_tree.addPaneToRoot(QStringLiteral("pane_1"));
    rebuild(false);
    emit layoutChanged();
}

bool WorkspaceWidget::save(const QString &path, QString *error) const
{
    return saveLayout(path, {}, {}, error);
}

bool WorkspaceWidget::saveLayout(const QString &path, const ContentDescriptor &descriptor,
                                 const PaneMetadata &metadata, QString *error) const
{
    QJsonObject documentData{{QStringLiteral("schema_version"), LayoutTree::SchemaVersion},
                             {QStringLiteral("layout"), m_tree.toJson()}};
    QJsonObject contents;
    QJsonObject paneMetadata;
    for (const QString &paneId : m_tree.paneIds()) {
        const PaneWidget *item = pane(paneId);
        QJsonArray items;
        if (descriptor) {
            for (const QString &contentId : item->contentIds()) {
                QJsonObject value = descriptor(item, contentId);
                if (!value.isEmpty()) {
                    if (!value.contains(QStringLiteral("content_id")))
                        value.insert(QStringLiteral("content_id"), contentId);
                    items.append(value);
                }
            }
        }
        if (!items.isEmpty()) contents.insert(paneId, items);
        QJsonObject meta = metadata ? metadata(item) : QJsonObject();
        if (!item->currentContentId().isEmpty())
            meta.insert(QStringLiteral("current_content_id"), item->currentContentId());
        if (item->iconOnly()) meta.insert(QStringLiteral("icon_only"), true);
        if (!meta.isEmpty()) paneMetadata.insert(paneId, meta);
    }
    if (!contents.isEmpty()) documentData.insert(QStringLiteral("pane_contents"), contents);
    if (!paneMetadata.isEmpty()) documentData.insert(QStringLiteral("pane_metadata"), paneMetadata);
    return saveLayoutAtomic(path, documentData, error);
}

bool WorkspaceWidget::load(const QString &path, QString *error)
{
    return loadLayout(path, {}, error);
}

bool WorkspaceWidget::loadLayout(const QString &path, const ContentFactory &factory,
                                 QString *error)
{
    const QJsonObject documentData = loadLayoutFile(path, error);
    if (documentData.isEmpty()) return false;
    const int version = documentData.contains(QStringLiteral("schema_version"))
        ? documentData.value(QStringLiteral("schema_version")).toInt(-1) : LayoutTree::SchemaVersion;
    if (version != LayoutTree::SchemaVersion) {
        if (error) *error = QStringLiteral("不支持的工作区布局版本");
        return false;
    }
    bool workspaceReplaced = false;
    try {
        const QJsonObject layout = documentData.contains(QStringLiteral("layout"))
            ? documentData.value(QStringLiteral("layout")).toObject() : documentData;
        LayoutTree candidate = LayoutTree::fromJson(layout);
        const QList<QString> paneIdList = candidate.paneIds();
        const QSet<QString> paneIds(paneIdList.cbegin(), paneIdList.cend());
        if (documentData.contains(QStringLiteral("pane_contents")) &&
            !documentData.value(QStringLiteral("pane_contents")).isObject())
            throw std::invalid_argument("pane_contents must be an object");
        if (documentData.contains(QStringLiteral("pane_metadata")) &&
            !documentData.value(QStringLiteral("pane_metadata")).isObject())
            throw std::invalid_argument("pane_metadata must be an object");
        const QJsonObject contents = documentData.value(QStringLiteral("pane_contents")).toObject();
        const QJsonObject metadata = documentData.value(QStringLiteral("pane_metadata")).toObject();
        QSet<QString> contentIds;
        for (auto it = contents.begin(); it != contents.end(); ++it) {
            if (!paneIds.contains(it.key()) || !it.value().isArray())
                throw std::invalid_argument("pane_contents references an unknown pane");
            for (const auto &value : it.value().toArray()) {
                if (!value.isObject()) throw std::invalid_argument("invalid content descriptor");
                const QJsonObject descriptor = value.toObject();
                if (descriptor.contains(QStringLiteral("content_id"))) {
                    const QJsonValue idValue = descriptor.value(QStringLiteral("content_id"));
                    if (!idValue.isString() || idValue.toString().trimmed().isEmpty())
                        throw std::invalid_argument("content id must be a non-empty string");
                    const QString id = idValue.toString();
                    if (contentIds.contains(id)) throw std::invalid_argument("duplicate content id");
                    contentIds.insert(id);
                }
            }
        }
        for (auto it = metadata.begin(); it != metadata.end(); ++it)
            if (!paneIds.contains(it.key()) || !it.value().isObject())
                throw std::invalid_argument("pane_metadata references an unknown pane");

        m_tree = std::move(candidate);
        workspaceReplaced = true;
        rebuild(false);
        for (auto it = metadata.begin(); it != metadata.end(); ++it)
            pane(it.key())->setIconOnly(it.value().toObject().value(QStringLiteral("icon_only")).toBool());
        if (factory) {
            for (auto it = contents.begin(); it != contents.end(); ++it) {
                for (const auto &value : it.value().toArray()) {
                    std::optional<ContentConfig> config = factory(value.toObject());
                    if (config && !fillPane(pane(it.key()), *config))
                        throw std::runtime_error("content factory result could not be inserted");
                }
            }
        }
        for (auto it = metadata.begin(); it != metadata.end(); ++it)
            pane(it.key())->setCurrentContentId(
                it.value().toObject().value(QStringLiteral("current_content_id")).toString());
        emit layoutChanged();
        return true;
    } catch (const std::exception &exception) {
        if (workspaceReplaced) resetToSingleEmptyPane();
        if (error) *error = QString::fromUtf8(exception.what());
        return false;
    }
}

void WorkspaceWidget::applyTheme(const DockTheme &theme)
{
    m_theme = theme;
    setStyleSheet(theme.styleSheet());
    for (PaneWidget *item : m_panes) item->applyTheme(theme);
    for (QSplitter *splitter : findChildren<QSplitter *>())
        splitter->setHandleWidth(2);
}

DockTheme WorkspaceWidget::theme() const { return m_theme; }

} // namespace darkeye::myads
