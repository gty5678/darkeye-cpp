#include "darkeye_ui/components/ColorPicker.h"
#include "darkeye_ui/components/ColorWheel.h"

#include <QGuiApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

namespace darkeye {

ColorPicker::ColorPicker(const QColor &color, bool showText, Shape shape,
                         QWidget *parent)
    : QLabel(parent), m_color(color.isValid() ? color : QColor("#cccccc")),
      m_showText(showText), m_shape(shape)
{
    setObjectName(QStringLiteral("DesignColorPicker"));
    setAlignment(Qt::AlignCenter);
    setCursor(Qt::PointingHandCursor);
    updateDisplay();
}

QString ColorPicker::color() const { return m_color.name(); }

void ColorPicker::setColor(const QString &color)
{
    const QColor candidate(color);
    if (!candidate.isValid()) return;
    m_color = candidate;
    updateDisplay();
    emit colorChanged(m_color.name());
}

void ColorPicker::setShowText(bool show) { m_showText = show; updateDisplay(); }
void ColorPicker::setShape(Shape shape) { m_shape = shape; updateDisplay(); }

void ColorPicker::paintEvent(QPaintEvent *event)
{
    if (m_shape == Shape::Rectangle) {
        QLabel::paintEvent(event);
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_color);
    painter.drawEllipse(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
}

void ColorPicker::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_colorWheel != nullptr && m_colorWheel->isVisible()) {
            m_colorWheel->hide();
            event->accept();
            return;
        }
        if (m_colorWheel == nullptr) {
            m_colorWheel = new ColorWheelSimple(this);
            m_colorWheel->setWindowFlags(Qt::FramelessWindowHint |
                                         Qt::WindowStaysOnTopHint | Qt::Popup);
            connect(m_colorWheel, &ColorWheelSimple::colorChanged, this,
                    [this](const QColor &color) { setColor(color.name()); });
            m_colorWheel->installEventFilter(this);
        }
        m_colorWheel->setInitialColor(m_color.name(QColor::HexRgb));
        m_colorWheel->adjustSize();
        const QPoint topLeft = mapToGlobal(QPoint(0, 0));
        const QRect available = screen() != nullptr
            ? screen()->availableGeometry() : QRect(topLeft, QSize(1920, 1080));
        const int x = qBound(available.left(),
            topLeft.x() + (width() - m_colorWheel->width()) / 2,
            available.right() - m_colorWheel->width() + 1);
        const int gap = 10;
        const int y = topLeft.y() - available.top() >= m_colorWheel->height() + gap
            ? topLeft.y() - m_colorWheel->height() - gap
            : topLeft.y() + height() + gap;
        m_colorWheel->move(x, y);
        m_colorWheel->show();
        m_colorWheel->raise();
        m_colorWheel->setFocus();
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

bool ColorPicker::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_colorWheel && event->type() == QEvent::Hide) {
        emit colorConfirmed(m_color.name());
    }
    return QLabel::eventFilter(watched, event);
}

void ColorPicker::updateDisplay()
{
    if (m_shape == Shape::Circle) {
        setFixedSize(32, 32);
        setText({});
        setStyleSheet({});
        // The circle is painted directly.  Keep the corners transparent so the
        // stylesheet border/background for DesignColorPicker cannot show as a
        // square grey halo around it.
        setAttribute(Qt::WA_TranslucentBackground, true);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setMinimumSize(0, 0);
        setMaximumSize(32, 32);
        setText(m_showText ? m_color.name() : QString());
        const QString foreground = m_color.lightness() > 128 ? QStringLiteral("#000000")
                                                             : QStringLiteral("#ffffff");
        setStyleSheet(QStringLiteral("background:%1;border-radius:12px;color:%2;font-size:24px;")
                          .arg(m_color.name(), foreground));
    }
    update();
}

} // namespace darkeye
