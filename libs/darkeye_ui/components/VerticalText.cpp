#include "darkeye_ui/components/VerticalText.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/layouts/VerticalTextLayout.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTimer>

namespace darkeye {
namespace {

ThemeTokens tokensFor(ThemeService *themes)
{
    return ThemeService::tokens(themes == nullptr ? ThemeId::Light
                                                   : themes->current(),
                                themes == nullptr ? QString()
                                                   : themes->customPrimary());
}

void drawVerticalText(QPainter &painter, const QRect &area, const QString &source)
{
    const QFontMetrics metrics(painter.font());
    const qreal lineSpacing = metrics.height() * 0.05;
    const qreal columnSpacing = metrics.height() * 0.1;
    const VerticalTextLayout layout(metrics, lineSpacing, columnSpacing);
    painter.save();
    painter.translate(area.topLeft());
    for (const VerticalTextBlock &block :
         layout.calculateLayout(source, area.width(), area.height())) {
        if (block.english) {
            painter.save();
            painter.translate(block.rect.center());
            painter.rotate(block.rotation);
            const qreal textWidth = metrics.horizontalAdvance(block.text);
            const qreal baselineOffset = (metrics.ascent() - metrics.descent()) / 2.0;
            painter.drawText(QPointF(-textWidth / 2.0, baselineOffset), block.text);
            painter.restore();
        } else {
            painter.drawText(block.rect, Qt::AlignCenter, block.text);
        }
    }
    painter.restore();
}

} // namespace

VerticalTextLabel::VerticalTextLabel(const QString &text, const QString &tone,
                                     ThemeService *themes, QWidget *parent)
    : QWidget(parent), m_text(VerticalTextLayout::replaceEllipsis(text)),
      m_tone(tone), m_themes(themes)
{
    setObjectName(QStringLiteral("DesignVerticalTextLabel"));
    setProperty("tone", tone);
    QFont value = font();
    value.setPointSize(value.pointSize() + 2);
    setFont(value);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (m_themes != nullptr) connect(m_themes, &ThemeService::themeChanged,
                                    this, [this] { update(); });
}
QString VerticalTextLabel::text() const { return m_text; }
void VerticalTextLabel::setText(const QString &text)
{
    m_text = VerticalTextLayout::replaceEllipsis(text);
    updateGeometry();
    update();
}
void VerticalTextLabel::setTextColor(const QColor &color) { m_overrideColor = color; update(); }
void VerticalTextLabel::setTone(const QString &tone)
{
    m_tone = tone; setProperty("tone", tone); update();
}
QSize VerticalTextLabel::minimumSizeHint() const
{
    const QFontMetrics metrics(font());
    const VerticalTextLayout layout(metrics, metrics.height() * 0.05,
                                    metrics.height() * 0.1);
    return layout.calculateSize(m_text, height());
}
QSize VerticalTextLabel::sizeHint() const { return minimumSizeHint(); }
void VerticalTextLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font());
    painter.setPen(effectiveColor());
    drawVerticalText(painter, rect(), m_text);
}
QColor VerticalTextLabel::effectiveColor() const
{
    if (m_overrideColor.isValid()) return m_overrideColor;
    const ThemeTokens tokens = tokensFor(m_themes);
    return QColor(m_tone == QStringLiteral("inverse") ? tokens.textInverse : tokens.text);
}

TokenVLabel::TokenVLabel(const QString &text, ThemeService *themes, QWidget *parent)
    : QLabel(text, parent), m_themes(themes)
{
    setObjectName(QStringLiteral("DesignTokenVLabel"));
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    const ThemeTokens tokens = tokensFor(themes);
    m_background = QColor(tokens.pageBackground);
    m_textColor = QColor(tokens.text);
    m_border = QColor(tokens.border);
    m_hoverColor = QColor(tokens.primary);
    m_flashTimer = new QTimer(this);
    connect(m_flashTimer, &QTimer::timeout, this, [this] {
        if (QDateTime::currentMSecsSinceEpoch() >= m_flashEnd) {
            m_flashTimer->stop(); m_inverted = false; update(); return;
        }
        m_inverted = !m_inverted; update();
    });
    if (m_themes != nullptr) connect(m_themes, &ThemeService::themeChanged,
                                    this, [this] {
        if (!m_explicitColors) {
            const ThemeTokens values = tokensFor(m_themes);
            m_background = QColor(values.pageBackground);
            m_textColor = QColor(values.text);
            m_border = QColor(values.border);
            m_hoverColor = QColor(values.primary);
        }
        update();
    });
    updateSize();
}
void TokenVLabel::setTextDynamic(const QString &text) { setText(text); updateSize(); update(); }
void TokenVLabel::setColors(const QColor &background, const QColor &text,
                            const QColor &hover)
{
    m_explicitColors = true;
    m_background = background; m_textColor = text;
    if (hover.isValid()) m_hoverColor = hover;
    update();
}
void TokenVLabel::flashInvert(int duration, int interval)
{
    m_flashEnd = QDateTime::currentMSecsSinceEpoch() + qMax(0, duration);
    m_flashTimer->start(qMax(16, interval));
}
QSize TokenVLabel::sizeHint() const { return size(); }
void TokenVLabel::updateSize()
{
    const QFontMetrics metrics(font());
    const int width = qRound(metrics.horizontalAdvance(QStringLiteral("中")) * 1.7);
    setFixedSize(width, qMax(width * 2, metrics.height() * text().size() + width));
}
void TokenVLabel::paintEvent(QPaintEvent *)
{
    const qreal cut = width() * 0.2;
    QPainterPath outer;
    outer.moveTo(cut, 0); outer.lineTo(width() - cut, 0);
    outer.lineTo(width(), cut); outer.lineTo(width(), height() - cut);
    outer.lineTo(width() - cut, height()); outer.lineTo(cut, height());
    outer.lineTo(0, height() - cut); outer.lineTo(0, cut); outer.closeSubpath();
    QPainterPath hole;
    const qreal radius = width() * 0.1;
    hole.addEllipse(QPointF(width() / 2.0, cut + radius), radius, radius);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor background = m_inverted ? m_textColor : m_background;
    QColor textColor = m_inverted ? m_background : (m_hovered ? m_hoverColor : m_textColor);
    painter.setPen(m_border); painter.setBrush(background); painter.drawPath(outer);
    painter.setBrush(Qt::NoBrush); painter.drawPath(hole);
    painter.setPen(textColor);
    drawVerticalText(painter, rect().adjusted(2, qRound(cut + radius * 2), -2, -2), text());
}
void TokenVLabel::enterEvent(QEnterEvent *event) { m_hovered = true; update(); QLabel::enterEvent(event); }
void TokenVLabel::leaveEvent(QEvent *event) { m_hovered = false; update(); QLabel::leaveEvent(event); }

TokenVerticalTabBar::TokenVerticalTabBar(ThemeService *themes, QWidget *parent)
    : QTabBar(parent), m_themes(themes)
{
    setObjectName(QStringLiteral("VerticalTabBar"));
    setShape(QTabBar::RoundedWest);
    if (m_themes != nullptr) connect(m_themes, &ThemeService::themeChanged,
                                    this, [this] { update(); });
}
QSize TokenVerticalTabBar::tabSizeHint(int index) const
{
    const QFontMetrics metrics(font());
    return {qMax(40, qRound(metrics.height() * 1.5)),
            qMax(40, metrics.height()
                         * static_cast<int>(tabText(index).size()) + 8)};
}
void TokenVerticalTabBar::paintEvent(QPaintEvent *)
{
    const ThemeTokens tokens = tokensFor(m_themes);
    QStylePainter painter(this);
    for (int index = 0; index < count(); ++index) {
        QStyleOptionTab option;
        initStyleOption(&option, index);
        const QString text = option.text;
        option.text.clear();
        painter.drawControl(QStyle::CE_TabBarTabShape, option);
        painter.setPen(QColor(index == currentIndex() ? tokens.textInverse : tokens.text));
        painter.setFont(font());
        drawVerticalText(painter, tabRect(index).adjusted(2, 4, -2, -4), text);
    }
}

} // namespace darkeye


