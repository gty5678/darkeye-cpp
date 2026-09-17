#include "ui/components/AsyncImageLabel.h"

#include <QResizeEvent>
#include <QRunnable>
#include <QThreadPool>

namespace darkeye {
namespace {

class ImageLoadTask final : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ImageLoadTask(QString path, QSize target, ImageFitMode fitMode, quint64 requestId)
        : m_path(std::move(path)), m_target(target), m_fitMode(fitMode),
          m_requestId(requestId)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QImage image(m_path);
        if (!image.isNull() && m_target.isValid() && !m_target.isEmpty()) {
            if (m_fitMode == ImageFitMode::RightCover) {
                const qreal ratio = qreal(m_target.width()) / m_target.height();
                const int cropWidth = qMin(image.width(), qRound(image.height() * ratio));
                image = image.copy(image.width() - cropWidth, 0, cropWidth, image.height());
            }
            const Qt::AspectRatioMode aspect =
                m_fitMode == ImageFitMode::Contain ? Qt::KeepAspectRatio
                                                   : Qt::KeepAspectRatioByExpanding;
            image = image.scaled(m_target, aspect, Qt::SmoothTransformation);
            if (m_fitMode == ImageFitMode::Cover && image.size() != m_target) {
                const int x = qMax(0, (image.width() - m_target.width()) / 2);
                const int y = qMax(0, (image.height() - m_target.height()) / 2);
                image = image.copy(x, y, m_target.width(), m_target.height());
            }
        }
        emit completed(m_requestId, m_path, image);
    }

signals:
    void completed(quint64 requestId, const QString &path, const QImage &image);

private:
    QString m_path;
    QSize m_target;
    ImageFitMode m_fitMode;
    quint64 m_requestId;
};

} // namespace

AsyncImageLabel::AsyncImageLabel(QWidget *parent) : QLabel(parent)
{
    setObjectName(QStringLiteral("DesignAsyncImage"));
    setAlignment(Qt::AlignCenter);
    setAttribute(Qt::WA_TranslucentBackground);
    setText(m_placeholder);
}

QString AsyncImageLabel::source() const { return m_source; }

void AsyncImageLabel::setSource(const QString &path)
{
    const QString normalized = path.trimmed();
    if (m_source == normalized) return;
    m_source = normalized;
    startLoad();
}

void AsyncImageLabel::clearSource()
{
    m_source.clear();
    ++m_requestId;
    clear();
    setText(m_placeholder);
}

void AsyncImageLabel::reload() { startLoad(); }

ImageFitMode AsyncImageLabel::fitMode() const { return m_fitMode; }

void AsyncImageLabel::setFitMode(ImageFitMode mode)
{
    if (m_fitMode == mode) return;
    m_fitMode = mode;
    startLoad();
}

bool AsyncImageLabel::greenMode() const { return m_greenMode; }

void AsyncImageLabel::setGreenMode(bool enabled)
{
    if (m_greenMode == enabled) return;
    m_greenMode = enabled;
    startLoad();
}

QString AsyncImageLabel::placeholderText() const { return m_placeholder; }

void AsyncImageLabel::setPlaceholderText(const QString &text)
{
    m_placeholder = text;
    if (pixmap().isNull()) setText(m_placeholder);
}

void AsyncImageLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    if (!m_source.isEmpty() && event->size() != event->oldSize()) startLoad();
}

void AsyncImageLabel::startLoad()
{
    const quint64 requestId = ++m_requestId;
    clear();
    if (m_source.isEmpty() || size().isEmpty()) {
        setText(m_placeholder);
        return;
    }
    setText(QStringLiteral("加载中…"));
    auto *task = new ImageLoadTask(m_source, size(), m_fitMode, requestId);
    connect(task, &ImageLoadTask::completed, this,
            [this](quint64 id, const QString &path, const QImage &image) {
                applyResult(id, path, image);
            });
    QThreadPool::globalInstance()->start(task);
}

void AsyncImageLabel::applyResult(quint64 requestId, const QString &path,
                                  const QImage &image)
{
    if (requestId != m_requestId || path != m_source) return;
    if (image.isNull()) {
        clear();
        setText(m_placeholder);
        emit imageLoadFailed(path);
        return;
    }
    const QImage displayImage = m_greenMode ? mosaic(image) : image;
    setText({});
    setPixmap(QPixmap::fromImage(displayImage));
    emit imageLoaded(path);
}

QImage AsyncImageLabel::mosaic(const QImage &source)
{
    if (source.isNull()) return {};
    const QSize tiny(qMax(1, source.width() / 18), qMax(1, source.height() / 18));
    return source.scaled(tiny, Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .scaled(source.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

} // namespace darkeye

#include "AsyncImageLabel.moc"
