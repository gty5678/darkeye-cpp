#pragma once

#include <QScrollArea>

class QColor;
class QLabel;
class QResizeEvent;
class QSlider;

namespace darkeye {

class ThemeService;

/** Floating controls for the force-directed graph, matching the Python view's
 * effect, display, and diagnostic sections. */
class ForceViewSettingsPanel final : public QScrollArea
{
    Q_OBJECT

public:
    explicit ForceViewSettingsPanel(ThemeService &themeService, QWidget *parent = nullptr);
    [[nodiscard]] int preferredHeight() const;

signals:
    void manyBodyStrengthChanged(float value);
    void centerStrengthChanged(float value);
    void linkStrengthChanged(float value);
    void linkDistanceChanged(float value);
    void radiusFactorChanged(float value);
    void textThresholdFactorChanged(float value);
    void linkWidthFactorChanged(float value);
    void neighborDepthChanged(int value);
    void graphNeighborDepthChanged(int value);
    void arrowEnabledChanged(bool enabled);
    void arrowScaleChanged(float value);
    void imageOverlayEnabledChanged(bool enabled);
    void nodeColorChanged(const QString &group, const QColor &color);
    void fitInViewRequested();
    void restartRequested();
    void pauseRequested();
    void resumeRequested();
    void addNodeRequested();
    void editNodeRequested();
    void removeNodeRequested();
    void addEdgeRequested();
    void removeEdgeRequested();
    void graphModeChanged(const QString &mode);
    void contentSizeChanged();

public slots:
    void setFps(float value);
    void setTickTime(float value);
    void setPaintTime(float value);
    void setScale(float value);
    void setAlpha(float value);

private:
    void resizeEvent(QResizeEvent *event) override;
    void updateContentWidth();
    static QSlider *slider(int minimum, int maximum, int value, QWidget *parent);

    QLabel *m_tick = nullptr;
    QLabel *m_paint = nullptr;
    QLabel *m_scale = nullptr;
    QLabel *m_fps = nullptr;
    QLabel *m_alpha = nullptr;
};

} // namespace darkeye
