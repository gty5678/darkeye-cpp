#include "ui/layouts/myads/PaneWidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace darkeye::myads {
namespace {

QIcon closeIcon(const QColor &color)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(6, 6), QPointF(18, 18));
    painter.drawLine(QPointF(18, 6), QPointF(6, 18));
    return QIcon(pixmap);
}

class DraggableTabBar final : public QTabBar
{
public:
    explicit DraggableTabBar(PaneWidget *pane) : QTabBar(pane), m_pane(pane)
    {
        setObjectName(QStringLiteral("MyAdsTabBar"));
        setMovable(true);
        setTabsClosable(true);
        setExpanding(false);
        setElideMode(Qt::ElideRight);
        setFixedHeight(32);
    }

    QSize tabSizeHint(int index) const override
    {
        const QString id = tabData(index).toString();
        if (m_pane->iconOnly() && !m_pane->isContentCloseable(id)) return QSize(30, 32);
        return QTabBar::tabSizeHint(index);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressPosition = event->position().toPoint();
        m_pressedIndex = tabAt(m_pressPosition);
        m_pressedContentId = m_pressedIndex < 0 ? QString() : tabData(m_pressedIndex).toString();
        QTabBar::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_pressedIndex = -1;
        m_pressedContentId.clear();
        QTabBar::mouseReleaseEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_pressedIndex < 0 || !(event->buttons() & Qt::LeftButton) ||
            (event->position().toPoint() - m_pressPosition).manhattanLength() < 8) {
            QTabBar::mouseMoveEvent(event);
            return;
        }
        if (rect().contains(event->position().toPoint())) {
            QTabBar::mouseMoveEvent(event);
            return;
        }
        const QString contentId = m_pressedContentId;
        if (contentId.isEmpty()) return;
        int currentIndex = -1;
        for (int i = 0; i < count(); ++i)
            if (tabData(i).toString() == contentId) { currentIndex = i; break; }
        if (currentIndex < 0) return;

        const int originalIndex = m_pressedIndex;
        // QDrag consumes the real release. Finish QTabBar's internal reorder
        // first, otherwise its moving-tab overlay and offsets remain active
        // when a dock drag is cancelled or rejected.
        const QPoint releasePosition = tabRect(currentIndex).center();
        QMouseEvent release(QEvent::MouseButtonRelease, releasePosition,
                            mapToGlobal(releasePosition), Qt::LeftButton,
                            Qt::NoButton, event->modifiers());
        mouseReleaseEvent(&release);

        QJsonObject payload{{QStringLiteral("pane_id"), m_pane->paneId()},
                            {QStringLiteral("content_id"), contentId}};
        auto *mime = new QMimeData;
        mime->setData(MyAdsTabMimeType, QJsonDocument(payload).toJson(QJsonDocument::Compact));
        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(grab(tabRect(currentIndex)));
        drag->setHotSpot(event->position().toPoint() - tabRect(currentIndex).topLeft());
        const QPointer<DraggableTabBar> guard(this);
        const Qt::DropAction action = drag->exec(Qt::MoveAction);
        if (!guard) return;
        if (action == Qt::IgnoreAction) {
            for (int i = 0; i < count(); ++i) {
                if (tabData(i).toString() == contentId) {
                    moveTab(i, qBound(0, originalIndex, count() - 1));
                    break;
                }
            }
        }
        emit m_pane->dragFinished();
    }

private:
    PaneWidget *m_pane;
    QPoint m_pressPosition;
    int m_pressedIndex = -1;
    QString m_pressedContentId;
};

bool decodeTab(const QMimeData *mime, QString *paneId, QString *contentId)
{
    if (!mime || !mime->hasFormat(MyAdsTabMimeType)) return false;
    const QJsonDocument document = QJsonDocument::fromJson(mime->data(MyAdsTabMimeType));
    if (!document.isObject()) return false;
    *paneId = document.object().value(QStringLiteral("pane_id")).toString();
    *contentId = document.object().value(QStringLiteral("content_id")).toString();
    return !paneId->isEmpty() && !contentId->isEmpty();
}

} // namespace

PaneWidget::PaneWidget(const QString &paneId, QWidget *parent)
    : QWidget(parent), m_paneId(paneId)
{
    setProperty("myadsPane", true);
    setAcceptDrops(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_tabBar = new DraggableTabBar(this);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabBar, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        const int index = m_tabBar->tabAt(position);
        if (index < 0) return;
        QMenu *menu = createTabContextMenu(m_tabBar->tabData(index).toString());
        if (!menu) return;
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(m_tabBar->mapToGlobal(position));
    });
    m_stack = new QStackedWidget(this);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack, 1);
    connect(m_tabBar, &QTabBar::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        const QString id = m_tabBar->tabData(index).toString();
        if (m_closeable.value(id, true)) removeContent(id);
    });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *page = m_stack->widget(from);
        if (!page) return;
        m_stack->removeWidget(page);
        m_stack->insertWidget(to, page);
        m_stack->setCurrentIndex(m_tabBar->currentIndex());
    });
}

QString PaneWidget::paneId() const { return m_paneId; }

int PaneWidget::tabInsertionIndex(const QPoint &globalPosition) const
{
    const QPoint local = m_tabBar->mapFromGlobal(globalPosition);
    if (!m_tabBar->rect().contains(local)) return -1;
    // Scroll arrows are controls, not insertion targets. Close buttons remain
    // part of their tab's drop area.
    auto *button = qobject_cast<QToolButton *>(m_tabBar->childAt(local));
    if (button && button->objectName() != QStringLiteral("MyAdsCloseButton")) return -1;
    const bool rtl = m_tabBar->layoutDirection() == Qt::RightToLeft;
    for (int i = 0; i < m_tabBar->count(); ++i) {
        const int center = m_tabBar->tabRect(i).center().x();
        if (rtl ? local.x() >= center : local.x() <= center) return i;
    }
    return m_tabBar->count();
}

QRect PaneWidget::tabInsertionRect(int index) const
{
    if (index < 0 || index > m_tabBar->count()) return {};
    const bool rtl = m_tabBar->layoutDirection() == Qt::RightToLeft;
    int x = rtl ? m_tabBar->width() - 1 : 0;
    if (index < m_tabBar->count()) {
        const QRect tab = m_tabBar->tabRect(index);
        x = rtl ? tab.right() + 1 : tab.left();
    } else if (index > 0) {
        const QRect tab = m_tabBar->tabRect(index - 1);
        x = rtl ? tab.left() : tab.right() + 1;
    }
    x = qBound(1, x, qMax(1, m_tabBar->width() - 2));
    return QRect(m_tabBar->mapTo(this, QPoint(x - 1, 2)),
                 QSize(3, qMax(1, m_tabBar->height() - 4)));
}

QMenu *PaneWidget::createTabContextMenu(const QString &contentId)
{
    const int index = indexOf(contentId);
    if (index < 0) return nullptr;
    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("MyAdsTabContextMenu"));
    const QStringList ids = contentIds();
    const auto addCloseAction = [this, menu](const QString &text, const QString &name,
                                            const QStringList &candidates) {
        QStringList closeableIds;
        for (const QString &id : candidates)
            if (isContentCloseable(id)) closeableIds.append(id);
        QAction *action = menu->addAction(text);
        action->setObjectName(name);
        action->setEnabled(!closeableIds.isEmpty());
        connect(action, &QAction::triggered, this, [this, closeableIds] {
            for (const QString &id : closeableIds)
                if (contentWidget(id) && isContentCloseable(id) && !removeContent(id)) break;
        });
    };
    QStringList others = ids;
    others.removeAll(contentId);
    addCloseAction(tr("关闭其他"), QStringLiteral("closeOtherTabs"), others);
    addCloseAction(tr("关闭右侧"), QStringLiteral("closeTabsToRight"), ids.mid(index + 1));
    emit tabContextMenuCreated(menu, contentId);
    return menu;
}

void PaneWidget::applyTheme(const DockTheme &theme)
{
    m_theme = theme;
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (auto *button = qobject_cast<QToolButton *>(
                m_tabBar->tabButton(i, QTabBar::RightSide)))
            button->setIcon(closeIcon(theme.closeIcon));
    }
    update();
}

bool PaneWidget::iconOnly() const noexcept { return m_iconOnly; }

void PaneWidget::setIconOnly(bool iconOnly)
{
    if (m_iconOnly == iconOnly) return;
    m_iconOnly = iconOnly;
    for (int i = 0; i < m_tabBar->count(); ++i) {
        const QString id = m_tabBar->tabData(i).toString();
        m_tabBar->setTabText(i, iconOnly ? QString() : m_titles.value(id));
    }
    m_tabBar->updateGeometry();
}

int PaneWidget::indexOf(const QString &contentId) const
{
    for (int i = 0; i < m_tabBar->count(); ++i)
        if (m_tabBar->tabData(i).toString() == contentId) return i;
    return -1;
}

void PaneWidget::updateCloseButton(int index)
{
    const QString id = m_tabBar->tabData(index).toString();
    QWidget *oldButton = m_tabBar->tabButton(index, QTabBar::RightSide);
    auto *button = qobject_cast<QToolButton *>(oldButton);
    if (!button || button->objectName() != QStringLiteral("MyAdsCloseButton")) {
        button = new QToolButton(m_tabBar);
        button->setObjectName(QStringLiteral("MyAdsCloseButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setIconSize(QSize(16, 16));
        connect(button, &QToolButton::clicked, this, [this, button] {
            for (int i = 0; i < m_tabBar->count(); ++i) {
                if (m_tabBar->tabButton(i, QTabBar::RightSide) == button) {
                    const QString contentId = m_tabBar->tabData(i).toString();
                    if (m_closeable.value(contentId, true)) removeContent(contentId);
                    return;
                }
            }
        });
        m_tabBar->setTabButton(index, QTabBar::RightSide, button);
        if (oldButton) oldButton->deleteLater();
    }
    const bool closeable = m_closeable.value(id, true);
    button->setIcon(closeIcon(m_theme.closeIcon));
    button->setFixedSize(closeable ? (m_tabBar->tabIcon(index).isNull() ? 22 : 24) : 0, closeable ? 20 : 0);
    button->setVisible(closeable);
    button->setEnabled(closeable);
}

bool PaneWidget::addContent(const QString &contentId, const QString &title, QWidget *widget,
                            const QIcon &icon, bool closeable, int index, ClosePolicy closePolicy)
{
    if (contentId.trimmed().isEmpty() || !widget || indexOf(contentId) >= 0) return false;
    const int target = index < 0 || index > m_tabBar->count() ? m_tabBar->count() : index;
    m_stack->insertWidget(target, widget);
    const int inserted = icon.isNull()
        ? m_tabBar->insertTab(target, m_iconOnly ? QString() : title)
        : m_tabBar->insertTab(target, icon, m_iconOnly ? QString() : title);
    m_tabBar->setTabData(inserted, contentId);
    m_tabBar->setTabToolTip(inserted, title);
    m_titles.insert(contentId, title);
    m_closeable.insert(contentId, closeable);
    m_closePolicies.insert(contentId, closePolicy);
    updateCloseButton(inserted);
    m_tabBar->setCurrentIndex(inserted);
    m_stack->setCurrentIndex(inserted);
    return true;
}

PaneContent PaneWidget::takeContent(const QString &contentId)
{
    PaneContent content;
    const int index = indexOf(contentId);
    if (index < 0) return content;
    content.contentId = contentId;
    content.title = m_titles.value(contentId);
    content.widget = m_stack->widget(index);
    content.icon = m_tabBar->tabIcon(index);
    content.closeable = m_closeable.value(contentId, true);
    content.closePolicy = contentClosePolicy(contentId);
    content.index = index;
    content.currentContentId = currentContentId();
    const QSignalBlocker blocker(m_tabBar);
    m_tabBar->removeTab(index);
    m_stack->removeWidget(content.widget);
    m_titles.remove(contentId);
    m_closeable.remove(contentId);
    m_closePolicies.remove(contentId);
    m_stack->setCurrentIndex(m_tabBar->currentIndex());
    return content;
}

bool PaneWidget::restoreContent(const PaneContent &content)
{
    if (!addContent(content.contentId, content.title, content.widget, content.icon,
                    content.closeable, content.index, content.closePolicy)) return false;
    if (!content.currentContentId.isEmpty()) setCurrentContentId(content.currentContentId);
    return true;
}

bool PaneWidget::removeContent(const QString &contentId)
{
    QWidget *page = contentWidget(contentId);
    if (!page || !isContentCloseable(contentId) || m_pendingCloseRequests.contains(contentId))
        return false;

    // Dispatch the event without QWidget::close(): the docking layer owns tab
    // removal, visibility and page lifetime (including WA_DeleteOnClose pages).
    // A modal save prompt can process events, so guard against re-entry and
    // revalidate ownership before touching the tab again.
    const QPointer<PaneWidget> paneGuard(this);
    const QPointer<QWidget> pageGuard(page);
    m_pendingCloseRequests.insert(contentId);
    QCloseEvent request;
    QApplication::sendEvent(page, &request);
    if (!paneGuard) return false;
    m_pendingCloseRequests.remove(contentId);
    if (!request.isAccepted() || !pageGuard || contentWidget(contentId) != pageGuard.data() ||
        !isContentCloseable(contentId)) return false;

    PaneContent content = takeContent(contentId);
    if (!content.widget) return false;
    content.widget->hide();
    if (content.closePolicy == ClosePolicy::DeleteLater) {
        // Release heavy pages after the current UI event has completed.
        content.widget->deleteLater();
    } else {
        // Retained pages belong to the caller and survive an empty pane being
        // removed. The caller must reuse or destroy the detached widget.
        content.widget->setParent(nullptr);
    }
    if (m_tabBar->count() == 0) emit paneEmpty(this);
    return true;
}

QStringList PaneWidget::contentIds() const
{
    QStringList result;
    for (int i = 0; i < m_tabBar->count(); ++i)
        result.append(m_tabBar->tabData(i).toString());
    return result;
}

int PaneWidget::contentCount() const { return m_tabBar->count(); }
QString PaneWidget::currentContentId() const
{
    return m_tabBar->currentIndex() < 0 ? QString()
        : m_tabBar->tabData(m_tabBar->currentIndex()).toString();
}

bool PaneWidget::setCurrentContentId(const QString &contentId)
{
    const int index = indexOf(contentId);
    if (index < 0) return false;
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
    return true;
}

QString PaneWidget::contentTitle(const QString &id) const { return m_titles.value(id); }
QWidget *PaneWidget::contentWidget(const QString &id) const
{
    const int index = indexOf(id);
    return index < 0 ? nullptr : m_stack->widget(index);
}
QIcon PaneWidget::contentIcon(const QString &id) const
{
    const int index = indexOf(id);
    return index < 0 ? QIcon() : m_tabBar->tabIcon(index);
}
bool PaneWidget::isContentCloseable(const QString &id) const
{
    return m_closeable.value(id, true);
}

ClosePolicy PaneWidget::contentClosePolicy(const QString &id) const
{
    return m_closePolicies.value(id, ClosePolicy::DeleteLater);
}

void PaneWidget::dragEnterEvent(QDragEnterEvent *event)
{
    QString paneId, contentId;
    if (decodeTab(event->mimeData(), &paneId, &contentId)) event->acceptProposedAction();
}

void PaneWidget::dragMoveEvent(QDragMoveEvent *event)
{
    QString paneId, contentId;
    if (!decodeTab(event->mimeData(), &paneId, &contentId)) return;
    emit dragMoved(this, mapToGlobal(event->position().toPoint()));
    event->acceptProposedAction();
}

void PaneWidget::dropEvent(QDropEvent *event)
{
    QString paneId, contentId;
    if (!decodeTab(event->mimeData(), &paneId, &contentId)) return;
    emit tabDropped(this, mapToGlobal(event->position().toPoint()), paneId, contentId);
    event->acceptProposedAction();
    emit dragFinished();
}

void PaneWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    emit dragFinished();
    QWidget::dragLeaveEvent(event);
}

} // namespace darkeye::myads

