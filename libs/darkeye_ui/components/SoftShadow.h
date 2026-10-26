#pragma once

#include <QImage>
#include <QPainterPath>

namespace darkeye {

// Thread-safe cached black shadow: sigma 2 DIP, offset (0, 2) DIP, opacity 80/255.
// pixels is the physical canvas size. Call from a worker to keep cache misses
// off the UI thread; leave at least 8 DIP around the shape for the blur.
QImage renderSoftShadow(const QSize &pixels, const QPainterPath &path, qreal dpr);

} // namespace darkeye
