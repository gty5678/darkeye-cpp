#include "ui/components/ActressWorkTimeline.h"

#include "darkeye_ui/components/DesignLabel.h"
#include "ui/components/WorkCard.h"

#include <QApplication>
#include <QDate>
#include <QHelpEvent>
#include <QFrame>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <functional>

namespace darkeye {
namespace {
constexpr int rulerHeight = 28;
constexpr int markerSize = 20;
constexpr int laneStride = 16;
constexpr int trackPad = 14;
constexpr int unknownWidth = 120;
constexpr double minPpd = 0.001;
constexpr double maxPpd = 300.0;
const QDate defaultStart(2016, 1, 1);
const QDate defaultEnd(2026, 12, 31);

QColor tagColor(int tagId, const QPalette &palette)
{
    switch (tagId) {
    case 1: return QColor(QStringLiteral("#80B0F8"));
    case 2: return QColor(QStringLiteral("#F88441"));
    case 3: return QColor(QStringLiteral("#FDEB48"));
    default: return palette.color(QPalette::Highlight);
    }
}

class TimelineHoverPopup final : public QWidget
{
public:
    TimelineHoverPopup()
        : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setMouseTracking(true);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    std::function<void()> entered;
    std::function<void()> left;

protected:
    void enterEvent(QEnterEvent *event) override
    {
        if (entered) entered();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        if (left) left();
        QWidget::leaveEvent(event);
    }
};
}

class TimelineCanvas final : public QWidget
{
public:
    explicit TimelineCanvas(QScrollArea *scroll, QString coverDirectory,
                            QWidget *parent = nullptr)
        : QWidget(parent), m_scroll(scroll), m_coverDirectory(std::move(coverDirectory))
    {
        setObjectName(QStringLiteral("ActressWorkTimelineCanvas"));
        setMouseTracking(true);
        setCursor(Qt::ArrowCursor);
        m_popup = new TimelineHoverPopup;
        m_popup->entered = [this] { m_hideTimer->stop(); };
        m_popup->left = [this] { schedulePopupHide(); };
        m_hideTimer = new QTimer(this);
        m_hideTimer->setSingleShot(true);
        m_hideTimer->setInterval(280);
        connect(m_hideTimer, &QTimer::timeout, this, [this] { hidePopup(); });
        connect(m_scroll->horizontalScrollBar(), &QScrollBar::valueChanged,
                this, [this] { hidePopup(); });
        connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged,
                this, [this] { hidePopup(); });
    }

    ~TimelineCanvas() override { delete m_popup; }

    void setWorks(QList<PersonWorkSummary> works)
    {
        m_works = std::move(works);
        rebuild();
    }
    const QList<PersonWorkSummary> &works() const { return m_works; }
    int markerCount() const { return m_markers.size(); }
    double ppd() const { return m_ppd; }
    QDate dateMinimum() const { return m_dateMin; }
    QPoint markerCenter(qint64 workId) const
    {
        const auto marker = std::find_if(m_markers.cbegin(), m_markers.cend(),
            [workId](const Marker &item) { return item.work.id == workId; });
        return marker == m_markers.cend() ? QPoint{} : marker->rect.center();
    }
    bool hoverPreviewVisible() const { return m_popup->isVisible(); }
    void setPpd(double value)
    {
        m_ppd = qBound(minPpd, value, maxPpd);
        rebuild();
    }
    std::function<void(qint64)> activate;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Base));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(palette().color(QPalette::Mid), 1));
        painter.drawLine(0, rulerHeight - 1, width(), rulerHeight - 1);
        painter.setPen(QPen(palette().color(QPalette::Mid), 2));
        painter.drawLine(0, m_axisY, width(), m_axisY);

        if (m_dateMin.isValid() && m_numDays > 0) {
            const int rawStep = qMax(1, qCeil(76.0 / qMax(m_ppd, 1e-9)));
            const QList<int> nice{1, 2, 3, 5, 7, 10, 14, 21, 30, 60, 90, 120,
                                  180, 365, 730, 1095, 1825};
            int step = rawStep;
            for (int candidate : nice) if (candidate >= rawStep) { step = candidate; break; }
            painter.setFont(QFont(painter.font().family(), 8));
            painter.setPen(palette().color(QPalette::Text));
            const int visibleStart = qMax(0, qFloor((m_scroll->horizontalScrollBar()->value()
                                                    - trackPad) / m_ppd));
            const int visibleEnd = qMin(m_numDays, visibleStart
                + qCeil((m_scroll->viewport()->width() + 400) / m_ppd));
            int first = ((visibleStart + step - 1) / step) * step;
            for (int day = first; day <= visibleEnd; day += step) {
                const int x = qRound(trackPad + day * m_ppd);
                painter.drawLine(x, m_axisY - 5, x, m_axisY + 5);
                const QString label = m_ppd >= 5.0
                    ? m_dateMin.addDays(day).toString(QStringLiteral("yyyy-MM-dd"))
                    : m_dateMin.addDays(day).toString(QStringLiteral("yyyy-MM"));
                const int textWidth = painter.fontMetrics().horizontalAdvance(label);
                painter.drawText(x - textWidth / 2, rulerHeight - 6, label);
            }
        }
        if (m_unknownX >= 0) {
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(m_unknownX - 18, rulerHeight - 6, QStringLiteral("日期未知"));
        }
        for (const Marker &marker : std::as_const(m_markers)) {
            painter.setBrush(tagColor(marker.work.highlightTagId, palette()));
            painter.setPen(QPen(palette().color(QPalette::Mid), 1));
            const QPointF center = marker.rect.center();
            QPainterPath diamond;
            diamond.moveTo(center.x(), marker.rect.top() + 2);
            diamond.lineTo(marker.rect.right() - 2, center.y());
            diamond.lineTo(center.x(), marker.rect.bottom() - 2);
            diamond.lineTo(marker.rect.left() + 2, center.y());
            diamond.closeSubpath();
            painter.drawPath(diamond);
        }
    }

    void wheelEvent(QWheelEvent *event) override
    {
        hidePopup();
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            m_scroll->horizontalScrollBar()->setValue(
                m_scroll->horizontalScrollBar()->value() - event->angleDelta().y());
            event->accept();
            return;
        }
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_scroll->verticalScrollBar()->setValue(
                m_scroll->verticalScrollBar()->value() - event->angleDelta().y());
            event->accept();
            return;
        }
        const int oldScroll = m_scroll->horizontalScrollBar()->value();
        const double anchorDay = (oldScroll + event->position().x() - trackPad) / m_ppd;
        setPpd(m_ppd * (event->angleDelta().y() > 0 ? 1.2 : 1.0 / 1.2));
        m_scroll->horizontalScrollBar()->setValue(
            qRound(trackPad + anchorDay * m_ppd - event->position().x()));
        event->accept();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::MiddleButton) {
            m_middlePanning = true;
            m_lastGlobalPosition = event->globalPosition();
            setCursor(Qt::ClosedHandCursor);
            grabMouse();
            hidePopup();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_middlePanning) {
            const QPointF delta = event->globalPosition() - m_lastGlobalPosition;
            m_lastGlobalPosition = event->globalPosition();
            m_scroll->horizontalScrollBar()->setValue(
                m_scroll->horizontalScrollBar()->value() - qRound(delta.x()));
            m_scroll->verticalScrollBar()->setValue(
                m_scroll->verticalScrollBar()->value() - qRound(delta.y()));
            event->accept();
            return;
        }
        const Marker *marker = markerAt(event->position().toPoint());
        if (marker != nullptr) {
            m_hideTimer->stop();
            showPopup(*marker);
        } else {
            schedulePopupHide();
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::MiddleButton && m_middlePanning) {
            m_middlePanning = false;
            releaseMouse();
            setCursor(Qt::ArrowCursor);
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            for (const Marker &marker : std::as_const(m_markers)) {
                if (marker.rect.contains(event->position().toPoint())) {
                    if (activate) activate(marker.work.id);
                    event->accept();
                    return;
                }
            }
        }
        QWidget::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        if (!m_middlePanning) schedulePopupHide();
        QWidget::leaveEvent(event);
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ToolTip) {
            const auto *help = static_cast<QHelpEvent *>(event);
            for (const Marker &marker : std::as_const(m_markers)) {
                if (marker.rect.contains(help->pos())) {
                    QToolTip::showText(help->globalPos(),
                        QStringLiteral("%1\n%2\n%3")
                            .arg(marker.work.serialNumber, marker.work.title,
                                 marker.work.releaseDate.trimmed().isEmpty()
                                     ? QStringLiteral("日期未知") : marker.work.releaseDate), this);
                    return true;
                }
            }
            QToolTip::hideText();
        }
        return QWidget::event(event);
    }

private:
    struct Marker { QRect rect; PersonWorkSummary work; int lane = 0; };

    const Marker *markerAt(const QPoint &position) const
    {
        const auto marker = std::find_if(m_markers.cbegin(), m_markers.cend(),
            [&position](const Marker &item) { return item.rect.contains(position); });
        return marker == m_markers.cend() ? nullptr : &*marker;
    }

    void showPopup(const Marker &marker)
    {
        if (m_popupWorkId == marker.work.id && m_popup->isVisible()) return;
        hidePopup();
        WorkSummary summary;
        summary.id = marker.work.id;
        summary.serialNumber = marker.work.serialNumber;
        summary.chineseTitle = marker.work.title;
        summary.releaseDate = marker.work.releaseDate;
        summary.imageUrl = marker.work.imageUrl;
        summary.highlightTagId = marker.work.highlightTagId;
        summary.standard = marker.work.standard;
        auto *card = new WorkCard(summary, m_coverDirectory, false, m_popup);
        connect(card, &WorkCard::activated, this, [this](qint64 workId) {
            hidePopup();
            if (activate) activate(workId);
        });
        connect(card, &WorkCard::editRequested, this, [this](qint64 workId) {
            hidePopup();
            if (activate) activate(workId);
        });
        m_popup->layout()->addWidget(card);
        m_popupWorkId = marker.work.id;
        m_popup->adjustSize();
        QPoint position = mapToGlobal(QPoint(marker.rect.center().x() - m_popup->width() / 2,
                                             marker.rect.bottom() + 8));
        if (QScreen *screen = QApplication::screenAt(mapToGlobal(marker.rect.center()))) {
            const QRect available = screen->availableGeometry();
            position.setX(qBound(available.left(), position.x(),
                                 available.right() - m_popup->width() + 1));
            if (position.y() + m_popup->height() > available.bottom()) {
                position.setY(mapToGlobal(QPoint(0, marker.rect.top() - m_popup->height() - 8)).y());
            }
        }
        m_popup->move(position);
        m_popup->show();
        m_popup->raise();
    }

    void schedulePopupHide()
    {
        if (m_popup->isVisible()) m_hideTimer->start();
    }

    void hidePopup()
    {
        m_hideTimer->stop();
        m_popup->hide();
        m_popupWorkId = 0;
        if (QLayoutItem *item = m_popup->layout()->takeAt(0)) {
            delete item->widget();
            delete item;
        }
    }

    void rebuild()
    {
        hidePopup();
        m_markers.clear();
        QList<QDate> dates;
        QMap<QDate, int> lanes;
        int unknownLane = 0;
        for (const PersonWorkSummary &work : std::as_const(m_works)) {
            const QDate date = QDate::fromString(work.releaseDate.left(10), Qt::ISODate);
            if (date.isValid()) dates.append(date);
        }
        if (!m_works.isEmpty()) {
            QDate coreMin = defaultStart;
            QDate coreMax = defaultEnd;
            if (!dates.isEmpty()) {
                coreMin = qMin(coreMin, *std::min_element(dates.cbegin(), dates.cend()));
                coreMax = qMax(coreMax, *std::max_element(dates.cbegin(), dates.cend()));
            }
            m_dateMin = coreMin.addDays(-120).addYears(-90);
            const QDate end = coreMax.addDays(120).addYears(90);
            m_numDays = m_dateMin.daysTo(end) + 1;
        } else {
            m_dateMin = {};
            m_numDays = 0;
        }
        int maxLane = 1;
        const bool hasUnknown = std::any_of(m_works.cbegin(), m_works.cend(), [](const auto &work) {
            return !QDate::fromString(work.releaseDate.left(10), Qt::ISODate).isValid();
        });
        m_unknownX = hasUnknown ? qRound(trackPad + m_numDays * m_ppd + 16) : -1;
        for (const PersonWorkSummary &work : std::as_const(m_works)) {
            const QDate date = QDate::fromString(work.releaseDate.left(10), Qt::ISODate);
            int lane = 0;
            int x = m_unknownX;
            if (date.isValid()) {
                lane = lanes[date]++;
                x = qRound(trackPad + m_dateMin.daysTo(date) * m_ppd);
            } else {
                lane = unknownLane++;
            }
            maxLane = qMax(maxLane, lane + 1);
            m_markers.append({QRect(x - markerSize / 2, 0, markerSize, markerSize), work, lane});
        }
        m_axisY = rulerHeight + 2 + (maxLane - 1) * laneStride + markerSize / 2;
        for (Marker &marker : m_markers) {
            marker.rect.moveTop(m_axisY - marker.lane * laneStride - markerSize / 2);
        }
        const int width = hasUnknown ? m_unknownX + unknownWidth
                                     : qRound(trackPad + m_numDays * m_ppd + 30);
        setFixedSize(qMax(200, width), qMax(84, m_axisY + markerSize + 14));
        update();
    }

    QScrollArea *m_scroll = nullptr;
    QString m_coverDirectory;
    TimelineHoverPopup *m_popup = nullptr;
    QTimer *m_hideTimer = nullptr;
    QList<PersonWorkSummary> m_works;
    QList<Marker> m_markers;
    QDate m_dateMin;
    int m_numDays = 0;
    int m_axisY = 52;
    int m_unknownX = -1;
    double m_ppd = 0.25;
    qint64 m_popupWorkId = 0;
    bool m_middlePanning = false;
    QPointF m_lastGlobalPosition;
};

ActressWorkTimeline::ActressWorkTimeline(QWidget *parent, QString coverDirectory) : QWidget(parent)
{
    setObjectName(QStringLiteral("ActressWorkTimeline"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(false);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_canvas = new TimelineCanvas(m_scroll, std::move(coverDirectory), m_scroll);
    m_canvas->activate = [this](qint64 id) { emit workRequested(id); };
    m_scroll->setWidget(m_canvas);
    m_empty = new DesignLabel(QStringLiteral("暂无关联作品"), this);
    static_cast<DesignLabel *>(m_empty)->setTone(QStringLiteral("muted"));
    layout->addWidget(m_empty, 0, Qt::AlignCenter);
    layout->addWidget(m_scroll, 1);
    setMinimumHeight(130);
    setWorks({});
}

void ActressWorkTimeline::setWorks(const QList<PersonWorkSummary> &works)
{
    m_canvas->setWorks(works);
    m_empty->setVisible(works.isEmpty());
    m_scroll->setVisible(!works.isEmpty());
    m_initialFitPending = !works.isEmpty();
    if (isVisible()) fitDefaultWindow();
}

QList<PersonWorkSummary> ActressWorkTimeline::works() const { return m_canvas->works(); }
int ActressWorkTimeline::markerCount() const noexcept { return m_canvas->markerCount(); }
double ActressWorkTimeline::pixelsPerDay() const noexcept { return m_canvas->ppd(); }
QPoint ActressWorkTimeline::markerCenter(qint64 workId) const
{
    return m_canvas->markerCenter(workId);
}
bool ActressWorkTimeline::hoverPreviewVisible() const noexcept
{
    return m_canvas->hoverPreviewVisible();
}

void ActressWorkTimeline::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    fitDefaultWindow();
}

void ActressWorkTimeline::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_initialFitPending) fitDefaultWindow();
}

void ActressWorkTimeline::fitDefaultWindow()
{
    if (!m_initialFitPending || m_scroll->viewport()->width() < 100) return;
    const int span = defaultStart.daysTo(defaultEnd) + 1;
    m_canvas->setPpd(qBound(minPpd,
        (m_scroll->viewport()->width() - 64.0) / static_cast<double>(span), maxPpd));
    const int day = m_canvas->dateMinimum().daysTo(defaultStart);
    m_scroll->horizontalScrollBar()->setValue(
        qMax(0, qRound(trackPad + day * m_canvas->ppd() - 32)));
    m_initialFitPending = false;
}

} // namespace darkeye
