#pragma once

#include <QImage>
#include <QLabel>
#include <QString>

namespace darkeye {

enum class ImageFitMode
{
    Contain,
    Cover,
    RightCover,
};

class AsyncImageLabel final : public QLabel
{
    Q_OBJECT

public:
    explicit AsyncImageLabel(QWidget *parent = nullptr);

    QString source() const;
    void setSource(const QString &path);
    void clearSource();
    void reload();

    ImageFitMode fitMode() const;
    void setFitMode(ImageFitMode mode);
    QString placeholderText() const;
    void setPlaceholderText(const QString &text);
    void setDeferredLoading(bool deferred);
    void startDeferredLoad(int priority = 0);

signals:
    void imageLoaded(const QString &path);
    void imageLoadFailed(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void startLoad();
    void applyResult(quint64 requestId, const QString &path, const QImage &image);

    QString m_source;
    QString m_placeholder = QStringLiteral("无图片");
    ImageFitMode m_fitMode = ImageFitMode::Contain;
    bool m_deferredLoading = false;
    bool m_loadPending = false;
    int m_loadPriority = 0;
    quint64 m_requestId = 0;
};

} // namespace darkeye
