#include "ui/components/WorkTagSelector.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/InteractionEffects.h"
#include "darkeye_ui/components/VerticalText.h"
#include "darkeye_ui/theme/ThemeService.h"
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <algorithm>
#include <functional>

namespace darkeye
{
namespace
{
constexpr int cardWidth = 27, gap = 5, margin = 5, leftWidth = 108, viewWidth = 84, panelWidth = 255;
bool cjk(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 0x4e00 && u <= 0x9fff) || (u >= 0x3040 && u <= 0x30ff);
}
QFont cardFont(QChar c)
{
    QFont f(cjk(c) ? QStringLiteral("KaiTi") : QStringLiteral("Courier New"));
    f.setPointSize(cjk(c) ? 14 : 16);
    f.setBold(true);
    return f;
}
ThemeTokens themeTokens(ThemeService *s)
{
    return ThemeService::tokens(s ? s->current() : ThemeId::Light, s ? s->customPrimary() : QString());
}
QColor readable(const QColor &c)
{
    return c.lightness() < 135 ? QColor(Qt::white) : QColor(Qt::black);
}
class VerticalTabs final : public QTabWidget
{
  public:
    using QTabWidget::setTabBar;
};
} // namespace

class TagCard final : public QWidget
{
  public:
    TagCard(qint64 id, const QString &t, const QString &color, ThemeService *themes, std::function<void()> click,
            bool title = false, QWidget *p = nullptr)
        : QWidget(p), m_id(id), m_text(t), m_color(color), m_themes(themes), m_click(std::move(click)), m_title(title)
    {
        setObjectName(title ? QStringLiteral("WorkTagTitle") : QStringLiteral("WorkTag_%1").arg(id));
        setAttribute(Qt::WA_Hover);
        setCursor(title ? Qt::ArrowCursor : Qt::PointingHandCursor);
        setFixedSize(sizeHint());
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this] {
            if (QDateTime::currentMSecsSinceEpoch() >= m_end)
            {
                m_timer->stop();
                m_invert = false;
            }
            else
                m_invert = !m_invert;
            update();
        });
        if (themes)
            connect(themes, &ThemeService::themeChanged, this, [this] { update(); });
    }
    qint64 id() const
    {
        return m_id;
    }
    QSize sizeHint() const override
    {
        if (m_text.isEmpty())
            return {cardWidth, cardWidth * 2};
        int h = 0;
        for (QChar c : m_text)
        {
            if (c == '\n')
                continue;
            QFontMetrics fm(cardFont(c));
            h += cjk(c) ? fm.height() : fm.ascent() + qRound(fm.descent() * .3);
        }
        QChar c = m_text.front();
        QFontMetrics fm(cardFont(c));
        qreal adjust = fm.horizontalAdvance(c) * (cjk(c) ? .1 : .4);
        return {cardWidth, qMax(cardWidth * 2, qRound(h + cardWidth * .8 - fm.descent() * .3 - adjust))};
    }
    void flash()
    {
        m_end = QDateTime::currentMSecsSinceEpoch() + 3000;
        m_invert = true;
        m_timer->start(150);
        update();
    }

  protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::Enter)
            m_hover = true;
        else if (e->type() == QEvent::Leave)
            m_hover = false;
        if (e->type() == QEvent::Enter || e->type() == QEvent::Leave)
            update();
        return QWidget::event(e);
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (!m_title && e->button() == Qt::LeftButton)
        {
            e->accept();
            if (m_click)
                m_click();
            return;
        }
        QWidget::mousePressEvent(e);
    }
    void paintEvent(QPaintEvent *) override
    {
        ThemeTokens t = themeTokens(m_themes);
        QColor bg = m_title ? QColor(t.text) : m_color;
        if (!bg.isValid())
            bg = QColor(t.pageBackground);
        QColor fg = m_title ? QColor(t.textInverse) : readable(bg),
               border = m_title ? QColor(t.border) : QColor(Qt::transparent);
        if (m_invert)
            std::swap(bg, fg);
        else if (m_hover && !m_title)
        {
            bg = bg.lighter(110);
            fg = readable(bg);
            border = Qt::red;
        }
        QRectF r = rect();
        qreal cut = r.width() * .2, rad = r.width() * .1;
        QPainterPath out;
        out.moveTo(cut, 0);
        out.lineTo(r.width() - cut, 0);
        out.lineTo(r.width(), cut);
        out.lineTo(r.width(), r.height() - cut);
        out.lineTo(r.width() - cut, r.height());
        out.lineTo(cut, r.height());
        out.lineTo(0, r.height() - cut);
        out.lineTo(0, cut);
        out.closeSubpath();
        QPainterPath hole;
        hole.addEllipse(QPointF(r.width() / 2, cut + rad), rad, rad);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillPath(out.subtracted(hole), bg);
        p.setPen(border);
        p.drawPath(out);
        p.drawPath(hole);
        qreal adjust = 0;
        if (!m_text.isEmpty())
        {
            QChar c = m_text.front();
            QFontMetrics fm(cardFont(c));
            adjust = fm.horizontalAdvance(c) * (cjk(c) ? .1 : .4);
        }
        int maxw = QFontMetrics(cardFont(QChar(0x4e2d))).horizontalAdvance(QChar(0x4e2d));
        qreal y = cut * 2 + rad * 2 - adjust;
        for (QChar c : m_text)
        {
            if (c == '\n')
                continue;
            QFont f = cardFont(c);
            QFontMetrics fm(f);
            int w = fm.horizontalAdvance(c), h = cjk(c) ? fm.height() : fm.ascent() + qRound(fm.descent() * .3);
            p.setFont(f);
            p.setPen(fg);
            p.drawText(QPointF((r.width() - maxw) / 2 + (maxw - w) / 2, y + fm.ascent()), QString(c));
            y += h;
        }
    }

  private:
    qint64 m_id;
    QString m_text;
    QColor m_color;
    ThemeService *m_themes;
    std::function<void()> m_click;
    QTimer *m_timer = nullptr;
    qint64 m_end = 0;
    bool m_title = false, m_hover = false, m_invert = false;
};

class TagFlowWidget final : public QWidget
{
  public:
    explicit TagFlowWidget(QWidget *p = nullptr) : QWidget(p)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    }
    void addCard(TagCard *c)
    {
        c->setParent(this);
        m_cards.append(c);
        c->show();
        relayout();
    }
    void removeCard(TagCard *c)
    {
        m_cards.removeOne(c);
        c->deleteLater();
        relayout();
    }
    void moveToEnd(TagCard *c)
    {
        if (m_cards.removeOne(c))
            m_cards.append(c);
        relayout();
    }
    void relayout()
    {
        int aw = qMax(1, width() - margin * 2), cols = qMax(1, aw / (cardWidth + gap)),
            total = cols * cardWidth + (cols - 1) * gap, ox = margin + (aw - total) / 2;
        QVector<int> hs(cols, margin);
        bool any = false;
        for (TagCard *c : std::as_const(m_cards))
        {
        if (c->isHidden())
            continue;
            any = true;
            int col = 0;
            for (int i = 1; i < cols; ++i)
                if (hs[i] < hs[col])
                    col = i;
            c->move(ox + col * (cardWidth + gap), hs[col]);
            hs[col] += c->height() + gap;
        }
        setMinimumHeight((any ? *std::max_element(hs.cbegin(), hs.cend()) : margin) + margin);
        updateGeometry();
    }

  protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        relayout();
    }

  private:
    QList<TagCard *> m_cards;
};

WorkTagSelector::WorkTagSelector(const QList<TagOption> &tags, ThemeService *themes, QWidget *parent)
    : QWidget(parent), m_tags(tags), m_themes(themes)
{
    setObjectName(QStringLiteral("WorkTagSelector"));
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *left = new QWidget(this);
    left->setFixedWidth(leftWidth);
    auto *ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    m_selectedView = new QScrollArea(left);
    m_selectedView->setObjectName(QStringLiteral("WorkTagList"));
    m_selectedView->setFixedWidth(viewWidth);
    m_selectedView->setWidgetResizable(true);
    m_selectedView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_selectedView->setFrameShape(QFrame::NoFrame);
    m_selectedView->setToolTip(QStringLiteral("点击已选标签可移除"));
    m_selectedFlow = new TagFlowWidget;
    m_selectedFlow->setObjectName(QStringLiteral("WorkSelectedTagFlow"));
    m_selectedView->setWidget(m_selectedFlow);
    applyViewStyle(m_selectedView);
    m_selectedFlow->addCard(new TagCard(-1, QStringLiteral("作品标签"), {}, themes, {}, true));
    auto *tools = new QWidget(left);
    tools->setFixedWidth(24);
    auto *tl = new QVBoxLayout(tools);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);
    auto *clear = new ShakeButton(QStringLiteral("brush_cleaning"), themes, tools);
    auto *reload = new RotateButton(QStringLiteral("refresh_cw"), themes, tools);
    m_expandButton = new IconButton(QStringLiteral("arrow_right"), themes, tools);
    clear->setObjectName(QStringLiteral("WorkTagClearButton"));
    reload->setObjectName(QStringLiteral("WorkTagReloadButton"));
    m_expandButton->setObjectName(QStringLiteral("WorkTagExpandButton"));
    clear->setToolTip(QStringLiteral("清空已选标签"));
    reload->setToolTip(QStringLiteral("重新加载标签"));
    m_expandButton->setToolTip(QStringLiteral("展开标签库"));
    clear->setButtonPixelSize(24);
    clear->setIconPixelSize(24);
    reload->setButtonPixelSize(24);
    reload->setIconPixelSize(24);
    m_expandButton->setButtonPixelSize(24);
    m_expandButton->setIconPixelSize(24);
    tl->addWidget(clear);
    tl->addWidget(reload);
    tl->addWidget(m_expandButton);
    tl->addStretch();
    ll->addWidget(m_selectedView);
    ll->addWidget(tools);
    root->addWidget(left);
    m_panel = new QWidget(this);
    m_panel->setObjectName(QStringLiteral("WorkTagAvailablePanel"));
    m_panel->setFixedWidth(0);
    auto *pl = new QVBoxLayout(m_panel);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);
    auto *bar = new QWidget(m_panel);
    auto *bl = new QHBoxLayout(bar);
    m_search = new DesignLineEdit(bar);
    m_search->setObjectName(QStringLiteral("WorkTagSearch"));
    m_search->setPlaceholderText(QStringLiteral("搜索"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(50);
    m_search->installEventFilter(this);
    m_searchResult = new DesignLabel(QStringLiteral("无搜索结果"), bar);
    m_searchResult->setObjectName(QStringLiteral("WorkTagSearchResult"));
    m_searchResult->setFixedWidth(70);
    m_searchPrevious = new IconButton(QStringLiteral("arrow_up"), themes, bar);
    m_searchNext = new IconButton(QStringLiteral("arrow_down"), themes, bar);
    m_searchPrevious->setObjectName(QStringLiteral("WorkTagSearchPrevious"));
    m_searchNext->setObjectName(QStringLiteral("WorkTagSearchNext"));
    m_searchPrevious->setToolTip(QStringLiteral("向前搜索(Shift+Enter)"));
    m_searchNext->setToolTip(QStringLiteral("向后搜索(Enter)"));
    m_searchPrevious->setEnabled(false);
    m_searchNext->setEnabled(false);
    bl->addWidget(m_search);
    bl->addWidget(m_searchResult);
    bl->addWidget(m_searchPrevious);
    bl->addWidget(m_searchNext);
    bl->addStretch();
    auto *vt = new VerticalTabs;
    vt->setParent(m_panel);
    m_tabs = vt;
    m_tabs->setObjectName(QStringLiteral("WorkTagTypeTabs"));
    m_tabs->setTabPosition(QTabWidget::West);
    vt->setTabBar(new TokenVerticalTabBar(themes, vt));
    pl->addWidget(bar);
    pl->addWidget(m_tabs, 1);
    root->addWidget(m_panel);
    setFixedWidth(leftWidth);
    m_panelAnimation = new QVariantAnimation(this);
    m_panelAnimation->setDuration(300);
    m_panelAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_panelAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_panel->setFixedWidth(v.toInt());
        setFixedWidth(leftWidth + v.toInt());
    });
    connect(clear, &QPushButton::clicked, this, &WorkTagSelector::clearSelection);
    connect(reload, &QPushButton::clicked, this, &WorkTagSelector::reloadTags);
    connect(m_expandButton, &QPushButton::clicked, this, [this] { setPanelExpanded(!m_panelExpanded); });
    connect(m_search, &QLineEdit::textChanged, this, &WorkTagSelector::rebuildSearch);
    connect(m_searchPrevious, &QPushButton::clicked, this, [this] { navigateSearch(-1); });
    connect(m_searchNext, &QPushButton::clicked, this, [this] { navigateSearch(1); });
    if (themes)
        connect(themes, &ThemeService::themeChanged, this, [this] {
            applyViewStyle(m_selectedView);
            for (auto *v : std::as_const(m_views))
                applyViewStyle(v);
            applyTabStyle();
        });
    setTags(tags);
}

bool WorkTagSelector::eventFilter(QObject *w, QEvent *e)
{
    if (w == m_search && e->type() == QEvent::KeyPress)
    {
        auto *k = static_cast<QKeyEvent *>(e);
        if (k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter)
        {
            navigateSearch(k->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1);
            return true;
        }
    }
    return QWidget::eventFilter(w, e);
}
void WorkTagSelector::setLoader(Loader l)
{
    m_loader = std::move(l);
}
void WorkTagSelector::reloadTags()
{
    if (m_loader)
        setTags(m_loader());
    m_search->clear();
}
QList<qint64> WorkTagSelector::selectedIds() const
{
    QList<qint64> ids = m_selectedIds.values();
    std::sort(ids.begin(), ids.end());
    return ids;
}
void WorkTagSelector::clearSelection()
{
    for (qint64 id : QList<qint64>(m_selectedOrder))
        setTagSelected(id, false, false);
}
void WorkTagSelector::setPanelExpanded(bool on)
{
    m_panelExpanded = on;
    m_expandButton->setIconName(on ? QStringLiteral("arrow_left") : QStringLiteral("arrow_right"));
    m_expandButton->setToolTip(on ? QStringLiteral("收起标签库") : QStringLiteral("展开标签库"));
    m_panelAnimation->stop();
    m_panelAnimation->setStartValue(m_panel->width());
    m_panelAnimation->setEndValue(on ? panelWidth : 0);
    m_panelAnimation->start();
    if (on)
        QTimer::singleShot(350, this, [this] {
            if (auto *f = m_flows.value(m_tabs->tabText(m_tabs->currentIndex())))
                f->relayout();
        });
}
void WorkTagSelector::setTagSelected(qint64 id, bool selected, bool switchTab)
{
    if (selected == m_selectedIds.contains(id))
        return;
    const TagOption *t = tagById(id);
    if (!t)
        return;
    if (selected && !t->mutexGroup.isEmpty())
        for (qint64 sid : std::as_const(m_selectedIds))
        {
            const TagOption *s = tagById(sid);
            if (s && s->mutexGroup == t->mutexGroup)
            {
                QMessageBox::warning(this, QStringLiteral("标签冲突"),
                                     QStringLiteral("标签 <b>'%1'</b> 与已选标签 <b>'%2'</b> "
                                                    "互斥！<br><br>请先移除冲突标签再添加。")
                                         .arg(t->name, s->name));
                return;
            }
        }
    if (selected)
    {
        m_selectedIds.insert(id);
        m_selectedOrder.append(id);
        auto *c = new TagCard(id, t->name, t->color, m_themes,
                              [this, id] { QTimer::singleShot(0, this, [this, id] { setTagSelected(id, false); }); });
        m_selectedItems.insert(id, c);
        m_selectedFlow->addCard(c);
    }
    else
    {
        m_selectedIds.remove(id);
        m_selectedOrder.removeOne(id);
        if (auto *c = m_selectedItems.take(id))
            m_selectedFlow->removeCard(c);
        if (auto *c = m_items.value(id))
        {
            c->show();
            if (auto *f = m_flows.value(t->typeName))
            {
                f->moveToEnd(c);
                if (switchTab)
                    for (int i = 0; i < m_tabs->count(); ++i)
                        if (m_tabs->tabText(i) == t->typeName)
                        {
                            m_tabs->setCurrentIndex(i);
                            break;
                        }
            }
        }
    }
    updateAvailableVisibility();
    emit selectionChanged();
}
void WorkTagSelector::rebuildSearch(const QString &s)
{
    m_searchMatches.clear();
    m_searchIndex = -1;
    QString n = s.trimmed();
    if (!n.isEmpty())
        for (const auto &t : std::as_const(m_tags))
            if (QStringLiteral("%1 %2 %3").arg(t.name, t.typeName, t.detail).contains(n, Qt::CaseInsensitive))
                m_searchMatches.append(t.id);
    bool ok = !m_searchMatches.isEmpty();
    m_searchPrevious->setEnabled(ok);
    m_searchNext->setEnabled(ok);
    m_searchResult->setText(ok            ? QStringLiteral("找到 %1 个结果").arg(m_searchMatches.size())
                            : n.isEmpty() ? QStringLiteral("无搜索结果")
                                          : QStringLiteral("无匹配结果"));
    if (ok)
        navigateSearch(1);
}
void WorkTagSelector::navigateSearch(int d)
{
    if (m_searchMatches.isEmpty())
        return;
    m_searchIndex = (m_searchIndex + d + m_searchMatches.size()) % m_searchMatches.size();
    showSearchResult();
}
void WorkTagSelector::showSearchResult()
{
    if (m_searchIndex < 0)
        return;
    qint64 id = m_searchMatches[m_searchIndex];
    m_searchResult->setText(QStringLiteral("结果 %1/%2").arg(m_searchIndex + 1).arg(m_searchMatches.size()));
    if (auto *c = m_selectedItems.value(id))
    {
        m_selectedView->ensureWidgetVisible(c);
        c->flash();
        return;
    }
    const TagOption *t = tagById(id);
    auto *c = m_items.value(id);
    if (!t || !c)
        return;
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->tabText(i) == t->typeName)
        {
            m_tabs->setCurrentIndex(i);
            break;
        }
    if (auto *v = m_views.value(t->typeName))
        v->ensureWidgetVisible(c);
    c->flash();
}
void WorkTagSelector::updateAvailableVisibility()
{
    for (auto i = m_items.cbegin(); i != m_items.cend(); ++i)
        i.value()->setVisible(!m_selectedIds.contains(i.key()));
    for (auto *f : std::as_const(m_flows))
        f->relayout();
    m_selectedFlow->relayout();
}
void WorkTagSelector::setTags(const QList<TagOption> &tags)
{
    m_updating = true;
    m_tags = tags;
    m_items.clear();
    m_flows.clear();
    m_views.clear();
    while (m_tabs->count())
    {
        QWidget *p = m_tabs->widget(0);
        m_tabs->removeTab(0);
        p->deleteLater();
    }
    for (const auto &t : m_tags)
    {
        TagFlowWidget *f = m_flows.value(t.typeName);
        if (!f)
        {
            auto *v = new QScrollArea(m_tabs);
            v->setObjectName(QStringLiteral("WorkAvailableTagList"));
            v->setWidgetResizable(true);
            v->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            v->setFrameShape(QFrame::NoFrame);
            f = new TagFlowWidget;
            f->setObjectName(QStringLiteral("WorkAvailableTagFlow"));
            v->setWidget(f);
            applyViewStyle(v);
            m_flows.insert(t.typeName, f);
            m_views.insert(t.typeName, v);
            m_tabs->addTab(v, t.typeName);
        }
        auto *c = new TagCard(t.id, t.name, t.color, m_themes, [this, id = t.id] {
            if (!m_updating)
                setTagSelected(id, true);
        });
        c->setToolTip(t.detail);
        f->addCard(c);
        m_items.insert(t.id, c);
    }
    QSet<qint64> available;
    for (const auto &t : m_tags)
        available.insert(t.id);
    m_selectedIds.intersect(available);
    QList<qint64> order = m_selectedOrder;
    m_selectedOrder.clear();
    for (qint64 id : order)
        if (m_selectedIds.contains(id))
            m_selectedOrder.append(id);
    for (qint64 id : std::as_const(m_selectedIds))
        if (!m_selectedOrder.contains(id))
            m_selectedOrder.append(id);
    for (auto *c : std::as_const(m_selectedItems))
        m_selectedFlow->removeCard(c);
    m_selectedItems.clear();
    for (qint64 id : std::as_const(m_selectedOrder))
    {
        const TagOption *t = tagById(id);
        if (!t)
            continue;
        auto *c = new TagCard(id, t->name, t->color, m_themes,
                              [this, id] { QTimer::singleShot(0, this, [this, id] { setTagSelected(id, false); }); });
        m_selectedItems.insert(id, c);
        m_selectedFlow->addCard(c);
    }
    m_updating = false;
    updateAvailableVisibility();
    applyTabStyle();
    rebuildSearch(m_search->text());
}
const TagOption *WorkTagSelector::tagById(qint64 id) const
{
    for (const auto &t : m_tags)
        if (t.id == id)
            return &t;
    return nullptr;
}
void WorkTagSelector::applyViewStyle(QWidget *w) const
{
    ThemeTokens t = themeTokens(m_themes);
    w->setStyleSheet(QStringLiteral("QScrollArea{border:%1 dashed "
                                    "%2;border-radius:%3;background:%4}QScrollArea>QWidget>"
                                    "QWidget{background:%4}")
                         .arg(t.borderWidth, t.border, t.radiusMd, t.pageBackground));
}
void WorkTagSelector::applyTabStyle() const
{
    ThemeTokens t = themeTokens(m_themes);
    m_tabs->setStyleSheet(QStringLiteral("QTabWidget::pane{border:none;background:transparent;margin:0;"
                                         "padding:0}QTabBar::tab{background:%1;color:%2;border:%3 solid "
                                         "%4;padding:8px "
                                         "18px;font-size:%5;font-family:'%6';min-width:90px;border-top-left-"
                                         "radius:%7;border-bottom-left-radius:%7;margin-right:4px}QTabBar::"
                                         "tab:hover{background:%8;color:%2}QTabBar::tab:selected{background:%"
                                         "9;color:%10;font-weight:bold}")
                              .arg(t.pageBackground, t.text, t.borderWidth, t.border, t.fontSizeMiddle,
                                   t.fontFamilyBase, t.radiusMd, t.inputBackground, t.primary, t.textInverse));
}
} // namespace darkeye
