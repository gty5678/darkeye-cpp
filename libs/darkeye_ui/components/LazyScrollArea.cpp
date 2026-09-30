#include "darkeye_ui/components/LazyScrollArea.h"

#include "darkeye_ui/layouts/WaterfallLayout.h"

#include <QScrollBar>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <utility>

namespace darkeye {

LazyScrollArea::LazyScrollArea(int columnWidth, QWidget *parent)
    : QScrollArea(parent), m_contentWidget(new QWidget(this)),
      m_waterfallWidget(m_contentWidget),
      m_layout(new WaterfallLayout(m_waterfallWidget, columnWidth, 0, 10))
{
    setObjectName(QStringLiteral("DesignLazyScrollArea"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    m_layout->setContentsMargins(0, 5, 0, 0);
    setWidget(m_contentWidget);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &LazyScrollArea::handleScroll);
}

void LazyScrollArea::setLoader(Loader loader)
{
    m_loader = std::move(loader);
    reset();
}

void LazyScrollArea::reset()
{
    while (QLayoutItem *item = m_layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            // Match the Python implementation: remove cards from the content
            // widget before scheduling deletion.  deleteLater() alone leaves
            // the old card visible until the next event-loop turn, where it
            // can briefly overlap the newly laid-out page during a refresh.
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
    m_currentPage = 0;
    m_reachedEnd = false;
    m_loading = false;
    m_scrollCheckRetries = 0;
    ++m_generation;
    verticalScrollBar()->setValue(0);
    m_layout->invalidate();
    if (m_loader) {
        loadNextPage();
    }
}

bool LazyScrollArea::loadNextPage()
{
    if (!m_loader || m_loading || m_reachedEnd) {
        return false;
    }

    m_loading = true;
    const QList<QWidget *> widgets = m_loader(m_currentPage, m_pageSize);
    for (QWidget *widget : widgets) {
        if (widget != nullptr) {
            m_layout->addWidget(widget);
        }
    }
    if (widgets.size() < m_pageSize) {
        m_reachedEnd = true;
    }
    if (!widgets.isEmpty()) {
        ++m_currentPage;
    }
    m_loading = false;
    m_layout->invalidate();
    updateContentGeometry();
    scheduleScrollableCheck();
    return !widgets.isEmpty();
}

void LazyScrollArea::setHeaderWidget(QWidget *widget)
{
    if (m_headerWidget == widget) return;
    if (widget != nullptr) ensureContentLayout();
    if (m_headerWidget != nullptr) {
        m_contentLayout->removeWidget(m_headerWidget);
        m_headerWidget->setParent(nullptr);
    }
    m_headerWidget = widget;
    if (widget != nullptr) {
        widget->setParent(m_contentWidget);
        m_contentLayout->insertWidget(0, widget, 0, Qt::AlignHCenter | Qt::AlignTop);
    }
    m_contentWidget->updateGeometry();
}

QWidget *LazyScrollArea::headerWidget() const { return m_headerWidget; }

int LazyScrollArea::pageSize() const
{
    return m_pageSize;
}

void LazyScrollArea::setPageSize(int pageSize)
{
    if (pageSize > 0) {
        m_pageSize = pageSize;
    }
}

int LazyScrollArea::currentPage() const
{
    return m_currentPage;
}

bool LazyScrollArea::reachedEnd() const
{
    return m_reachedEnd;
}

bool LazyScrollArea::loading() const
{
    return m_loading;
}

int LazyScrollArea::itemCount() const
{
    return m_layout->count();
}

int LazyScrollArea::columnWidth() const
{
    return m_layout->columnWidth();
}

void LazyScrollArea::setColumnWidth(int columnWidth)
{
    if (columnWidth > 0) {
        m_layout->setColumnWidth(columnWidth);
        updateContentGeometry();
    }
}

void LazyScrollArea::setPrefetchDistance(int distance)
{
    m_prefetchDistance = qMax(0, distance);
}

void LazyScrollArea::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    updateContentGeometry();
}

void LazyScrollArea::handleScroll(int value)
{
    const QScrollBar *scrollBar = verticalScrollBar();
    if (value >= scrollBar->maximum() - m_prefetchDistance) {
        loadNextPage();
    }
}

void LazyScrollArea::scheduleScrollableCheck()
{
    if (m_reachedEnd || !m_loader) return;
    const quint64 generation = m_generation;
    // Allow QScrollArea and the waterfall layout to apply the newly appended
    // cards before inspecting the scrollbar range.  Checking in the same
    // event-loop turn observes a stale zero range and can preload every page
    // instead of waiting for an actual scroll.
    QTimer::singleShot(50, this, [this, generation] {
        checkScrollableAndLoad(generation);
    });
}

void LazyScrollArea::checkScrollableAndLoad(quint64 generation)
{
    if (generation != m_generation || m_reachedEnd || m_loading || !m_loader
        || verticalScrollBar()->maximum() > 0 || m_scrollCheckRetries >= 10) {
        return;
    }
    ++m_scrollCheckRetries;
    loadNextPage();
}

void LazyScrollArea::ensureContentLayout()
{
    if (m_contentLayout != nullptr) return;

    QWidget *waterfall = takeWidget();
    Q_ASSERT(waterfall == m_waterfallWidget);
    m_contentWidget = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    waterfall->setParent(m_contentWidget);
    m_contentLayout->addWidget(waterfall, 0, Qt::AlignTop);
    m_contentLayout->addStretch();
    setWidget(m_contentWidget);
}

void LazyScrollArea::updateContentGeometry()
{
    // QScrollArea does not reliably turn a custom layout's height-for-width
    // result into its child's height.  Without an explicit minimum height the
    // cards can paint below the widget bounds while the scrollbar only covers
    // part of the waterfall.
    const int contentWidth = qMax(1, viewport()->width());
    m_waterfallWidget->setMinimumHeight(m_layout->heightForWidth(contentWidth));
    m_waterfallWidget->updateGeometry();
    m_contentWidget->updateGeometry();
}

} // namespace darkeye
