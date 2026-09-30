#include "ui/pages/ForceViewSettingsPanel.h"

#include "darkeye_ui/components/CollapsibleSection.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/InteractionEffects.h"
#include "darkeye_ui/components/AnimatedIndicators.h"
#include "darkeye_ui/components/TokenControls.h"
#include "darkeye_ui/theme/ThemeService.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace darkeye {

namespace {

QPushButton *makeColorButton(const QString &group, const QColor &initial,
                             ThemeService &themes, ForceViewSettingsPanel *panel)
{
    auto *button = new QPushButton(panel);
    button->setFixedSize(56, 24);
    button->setCursor(Qt::PointingHandCursor);
    auto color = std::make_shared<QColor>(initial);
    const auto applyStyle = [button, color, &themes] {
        const ThemeTokens tokens = themes.currentTokens();
        button->setStyleSheet(QStringLiteral("background-color:%1; border:1px solid %2; "
                                             "border-radius:3px;")
                                  .arg(color->name(), tokens.border));
    };
    applyStyle();
    QObject::connect(&themes, &ThemeService::themeChanged, button, applyStyle);
    QObject::connect(button, &QPushButton::clicked, panel, [panel, button, group, color, applyStyle] {
        const QColor selected = QColorDialog::getColor(*color, button, QStringLiteral("选择颜色"),
                                                        QColorDialog::ShowAlphaChannel);
        if (!selected.isValid()) return;
        *color = selected;
        applyStyle();
        emit panel->nodeColorChanged(group, selected);
    });
    return button;
}

} // namespace

ForceViewSettingsPanel::ForceViewSettingsPanel(ThemeService &themeService, QWidget *parent)
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("ForceViewSettingsPanel"));
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(250);
    setMaximumWidth(250);

    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    content->setObjectName(QStringLiteral("ForceViewSettingsContent"));
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *effects = new TokenCollapsibleSection(QStringLiteral("效果"), &themeService, content);
    auto *effectForm = new QFormLayout;
    auto *manyBody = slider(1000, 50000, 10000, content);
    auto *center = slider(1, 50, 10, content);
    auto *linkStrength = slider(1, 100, 30, content);
    auto *linkDistance = slider(10, 80, 40, content);
    effectForm->addRow(new DesignLabel(QStringLiteral("斥力强度"), content), manyBody);
    effectForm->addRow(new DesignLabel(QStringLiteral("中心力强度"), content), center);
    effectForm->addRow(new DesignLabel(QStringLiteral("连接力强度"), content), linkStrength);
    effectForm->addRow(new DesignLabel(QStringLiteral("连接距离"), content), linkDistance);
    auto *effectContent = new QWidget(content);
    effectContent->setLayout(effectForm);
    effects->addWidget(effectContent);
    connect(manyBody, &QSlider::valueChanged, this,
            [this](int value) { emit manyBodyStrengthChanged(float(value)); });
    connect(center, &QSlider::valueChanged, this,
            [this](int value) { emit centerStrengthChanged(float(value) / 1000.0F); });
    connect(linkStrength, &QSlider::valueChanged, this,
            [this](int value) { emit linkStrengthChanged(float(value) / 100.0F); });
    connect(linkDistance, &QSlider::valueChanged, this,
            [this](int value) { emit linkDistanceChanged(float(value)); });

    auto *display = new TokenCollapsibleSection(QStringLiteral("显示"), &themeService, content);
    auto *displayForm = new QFormLayout;
    auto *showArrow = new ToggleSwitch(48, 24, &themeService, content);
    showArrow->setChecked(true);
    auto *arrowSize = slider(3, 30, 10, content);
    auto *textFade = slider(10, 1000, 100, content);
    auto *nodeSize = slider(10, 300, 100, content);
    auto *lineWidth = slider(10, 300, 60, content);
    auto *neighborDepth = slider(1, 5, 2, content);
    auto *graphNeighborDepth = slider(1, 5, 3, content);
    displayForm->addRow(new DesignLabel(QStringLiteral("显示箭头"), content), showArrow);
    displayForm->addRow(new DesignLabel(QStringLiteral("箭头大小"), content), arrowSize);
    displayForm->addRow(new DesignLabel(QStringLiteral("文字渐隐"), content), textFade);
    displayForm->addRow(new DesignLabel(QStringLiteral("节点大小"), content), nodeSize);
    displayForm->addRow(new DesignLabel(QStringLiteral("连线宽度"), content), lineWidth);
    displayForm->addRow(new DesignLabel(QStringLiteral("邻居深度"), content), neighborDepth);
    displayForm->addRow(new DesignLabel(QStringLiteral("图邻居深度"), content), graphNeighborDepth);
    auto *colorColumn = new QVBoxLayout;
    colorColumn->setContentsMargins(0, 0, 0, 0);
    colorColumn->setSpacing(6);
    const auto addColor = [colorColumn, content, &themeService, this](const QString &name,
                                                                        const QString &group,
                                                                        const QColor &initial) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(new DesignLabel(name, content));
        row->addWidget(makeColorButton(group, initial, themeService, this));
        row->addStretch();
        colorColumn->addLayout(row);
    };
    addColor(QStringLiteral("女优"), QStringLiteral("actress"), QColor(QStringLiteral("#ff99cc")));
    addColor(QStringLiteral("作品"), QStringLiteral("work"), QColor(QStringLiteral("#99ccff")));
    addColor(QStringLiteral("中心"), QStringLiteral("center"), QColor(QStringLiteral("#ffd700")));
    addColor(QStringLiteral("默认"), QStringLiteral("default"), QColor(QStringLiteral("#5c5c5c")));
    displayForm->addRow(new DesignLabel(QStringLiteral("节点颜色"), content), colorColumn);
    auto *displayContent = new QWidget(content);
    displayContent->setLayout(displayForm);
    display->addWidget(displayContent);
    connect(showArrow, &ToggleSwitch::toggled, this,
            &ForceViewSettingsPanel::arrowEnabledChanged);
    connect(arrowSize, &QSlider::valueChanged, this,
            [this](int value) { emit arrowScaleChanged(float(value) / 10.0F); });
    connect(textFade, &QSlider::valueChanged, this,
            [this](int value) { emit textThresholdFactorChanged(float(value) / 100.0F); });
    connect(nodeSize, &QSlider::valueChanged, this,
            [this](int value) { emit radiusFactorChanged(float(value) / 100.0F); });
    connect(lineWidth, &QSlider::valueChanged, this,
            [this](int value) { emit linkWidthFactorChanged(float(value) / 100.0F); });
    connect(neighborDepth, &QSlider::valueChanged, this, &ForceViewSettingsPanel::neighborDepthChanged);
    connect(graphNeighborDepth, &QSlider::valueChanged, this,
            &ForceViewSettingsPanel::graphNeighborDepthChanged);

    auto *testing = new TokenCollapsibleSection(QStringLiteral("测试"), &themeService, content);
    const auto addButton = [this, testing, content](const QString &text, auto signal) {
        auto *button = new DesignButton(text, content);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        testing->addWidget(button);
        connect(button, &QPushButton::clicked, this, signal);
    };
    addButton(QStringLiteral("适配视图"), &ForceViewSettingsPanel::fitInViewRequested);
    addButton(QStringLiteral("重启"), &ForceViewSettingsPanel::restartRequested);
    addButton(QStringLiteral("暂停"), &ForceViewSettingsPanel::pauseRequested);
    addButton(QStringLiteral("继续"), &ForceViewSettingsPanel::resumeRequested);
    const auto addEditButton = [this, testing, content](const QString &text, auto signal) {
        auto *button = new DesignButton(text, content);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        testing->addWidget(button);
        connect(button, &QPushButton::clicked, this, signal);
    };
    addEditButton(QStringLiteral("加点"), &ForceViewSettingsPanel::addNodeRequested);
    addEditButton(QStringLiteral("编辑点"), &ForceViewSettingsPanel::editNodeRequested);
    addEditButton(QStringLiteral("减点"), &ForceViewSettingsPanel::removeNodeRequested);
    addEditButton(QStringLiteral("加边"), &ForceViewSettingsPanel::addEdgeRequested);
    addEditButton(QStringLiteral("减边"), &ForceViewSettingsPanel::removeEdgeRequested);
    auto *modes = new QHBoxLayout;
    auto *modeGroup = new QButtonGroup(content);
    const auto addMode = [this, modes, modeGroup, content](const QString &text, const QString &mode,
                                                            bool checked = false) {
        auto *button = new TokenRadioButton(text, content);
        button->setChecked(checked);
        modeGroup->addButton(button);
        modes->addWidget(button);
        connect(button, &QRadioButton::toggled, this, [this, mode](bool on) {
            if (on) emit graphModeChanged(mode);
        });
    };
    addMode(QStringLiteral("总图"), QStringLiteral("all"), true);
    addMode(QStringLiteral("中心图"), QStringLiteral("ego"));
    addMode(QStringLiteral("2000 点图"), QStringLiteral("test"));
    auto *stats = new QFormLayout;
    auto makeStat = [content] { return new DesignLabel(QStringLiteral("--"), content); };
    m_tick = makeStat();
    m_paint = makeStat();
    m_scale = makeStat();
    m_fps = makeStat();
    m_alpha = makeStat();
    auto *backend = new QHBoxLayout;
    auto *cpu = new TokenRadioButton(QStringLiteral("CPU"), content);
    cpu->setChecked(true);
    cpu->setEnabled(false);
    backend->addWidget(cpu);
    stats->addRow(new DesignLabel(QStringLiteral("Layout Backend"), content), backend);
    stats->addRow(new DesignLabel(QStringLiteral("Active Layout"), content), new DesignLabel(QStringLiteral("CPU Layout"), content));
    stats->addRow(new DesignLabel(QStringLiteral("图类型"), content), modes);
    stats->addRow(new DesignLabel(QStringLiteral("tick消耗"), content), m_tick);
    stats->addRow(new DesignLabel(QStringLiteral("paint消耗"), content), m_paint);
    stats->addRow(new DesignLabel(QStringLiteral("当前缩放"), content), m_scale);
    stats->addRow(new DesignLabel(QStringLiteral("当前帧率"), content), m_fps);
    stats->addRow(new DesignLabel(QStringLiteral("当前模拟热度"), content), m_alpha);
    auto *statsContent = new QWidget(content);
    statsContent->setLayout(stats);
    testing->addWidget(statsContent);

    layout->addWidget(effects);
    layout->addWidget(display);
    layout->addWidget(testing);
    layout->addStretch();
    setWidget(content);
    updateContentWidth();

    // Match the Python panel: a section toggle is measured after Qt has
    // committed the layout, then the parent repositions/resizes this overlay.
    const auto notifyContentSizeChanged = [this, content] {
        QTimer::singleShot(0, this, [this, content] {
            content->updateGeometry();
            if (content->layout() != nullptr) content->layout()->activate();
            updateGeometry();
            emit contentSizeChanged();
        });
    };
    connect(effects, &TokenCollapsibleSection::toggled, this,
            [notifyContentSizeChanged](bool) { notifyContentSizeChanged(); });
    connect(display, &TokenCollapsibleSection::toggled, this,
            [notifyContentSizeChanged](bool) { notifyContentSizeChanged(); });
    connect(testing, &TokenCollapsibleSection::toggled, this,
            [notifyContentSizeChanged](bool) { notifyContentSizeChanged(); });
}

QSlider *ForceViewSettingsPanel::slider(int minimum, int maximum, int value, QWidget *parent)
{
    auto *result = new ClickableSlider(Qt::Horizontal, nullptr, parent);
    result->setRange(minimum, maximum);
    result->setValue(value);
    return result;
}

void ForceViewSettingsPanel::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    updateContentWidth();
}

void ForceViewSettingsPanel::updateContentWidth()
{
    QWidget *content = widget();
    if (content == nullptr || viewport()->width() <= 0) return;
    // Same as Python's _update_content_width(): when the vertical scrollbar
    // consumes space, reflow the form to the remaining viewport width instead
    // of keeping a wider child clipped on the right.
    content->setFixedWidth(viewport()->width());
    content->setMinimumHeight(0);
    if (content->layout() != nullptr) content->layout()->activate();
}

int ForceViewSettingsPanel::preferredHeight() const
{
    QWidget *content = widget();
    if (content == nullptr) return QScrollArea::sizeHint().height();
    if (content->layout() != nullptr) content->layout()->activate();
    // Include the scroll area's frame so a panel assigned exactly this height
    // still has a viewport large enough for all folded/expanded content.
    return content->sizeHint().height() + frameWidth() * 2;
}

void ForceViewSettingsPanel::setFps(float value) { m_fps->setText(QString::number(value, 'f', 2)); }
void ForceViewSettingsPanel::setTickTime(float value) { m_tick->setText(QStringLiteral("%1 ms").arg(value, 0, 'f', 3)); }
void ForceViewSettingsPanel::setPaintTime(float value) { m_paint->setText(QStringLiteral("%1 ms").arg(value, 0, 'f', 3)); }
void ForceViewSettingsPanel::setScale(float value) { m_scale->setText(QString::number(value, 'f', 2)); }
void ForceViewSettingsPanel::setAlpha(float value) { m_alpha->setText(QString::number(value, 'f', 2)); }

} // namespace darkeye
