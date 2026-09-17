#include "darkeye_ui/components/OctImage.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QMetaObject>
#include <QPointer>
#include <QRegion>
#include <QThreadPool>

#include <cmath>

namespace darkeye {

OctImage::OctImage(const QString &imagePath, const QString &basePath, int diameter,
                   bool shadow, QWidget *parent)
    : QLabel(parent), m_diameter(qMax(16, diameter)), m_basePath(basePath)
{
    setObjectName(QStringLiteral("DesignOctImage"));
    setFixedSize(m_diameter, m_diameter);
    setAlignment(Qt::AlignCenter);
    setAttribute(Qt::WA_TranslucentBackground);
    if (shadow) {
        auto *effect = new QGraphicsDropShadowEffect(this);
        effect->setBlurRadius(10);
        effect->setOffset(0, 2);
        effect->setColor(QColor(0, 0, 0, 80));
        setGraphicsEffect(effect);
    }
    connect(this, &OctImage::imageReady, this,
            [this](const QImage &image) {
                if (!image.isNull()) setPixmap(QPixmap::fromImage(image));
            });
    updateImage(imagePath);
}

QString OctImage::source() const { return m_source; }

void OctImage::updateImage(const QString &imagePath)
{
    m_source = resolvedPath(imagePath);
    ++m_request;
    clearMask();
    clear();
    if (m_source.isEmpty() || !QFileInfo::exists(m_source)) {
        setText(QStringLiteral("无图片"));
        return;
    }
    startLoad();
}

QString OctImage::resolvedPath(const QString &imagePath) const
{
    if (imagePath.trimmed().isEmpty()) return {};
    if (QFileInfo(imagePath).isAbsolute() || m_basePath.isEmpty()) return imagePath;
    return QDir(m_basePath).filePath(imagePath);
}

void OctImage::startLoad()
{
    const quint64 request = m_request;
    const QString path = m_source;
    const QSize size = this->size();
    QPointer<OctImage> guard(this);
    QThreadPool::globalInstance()->start([guard, request, path, size] {
        QImage image(path);
        if (!image.isNull()) image = image.scaled(size, Qt::KeepAspectRatioByExpanding,
                                                  Qt::SmoothTransformation);
        if (guard.isNull()) return;
        QMetaObject::invokeMethod(guard, [guard, request, path, image] {
            if (!guard.isNull()) guard->applyImage(request, path, image);
        }, Qt::QueuedConnection);
    });
}

void OctImage::applyImage(quint64 request, const QString &path, const QImage &image)
{
    if (request != m_request || path != m_source) return;
    if (image.isNull()) {
        setText(QStringLiteral("无图片"));
        return;
    }
    const qreal cut = m_diameter / (2.0 + std::sqrt(2.0));
    QPolygon polygon{{qRound(cut), 0}, {qRound(m_diameter - cut), 0},
                     {m_diameter, qRound(cut)}, {m_diameter, qRound(m_diameter - cut)},
                     {qRound(m_diameter - cut), m_diameter}, {qRound(cut), m_diameter},
                     {0, qRound(m_diameter - cut)}, {0, qRound(cut)}};
    setMask(QRegion(polygon));
    emit imageReady(image);
    emit imageLoaded(path);
}

} // namespace darkeye
