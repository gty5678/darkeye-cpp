#pragma once

#include <QWidget>

class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QLabel;
class QResizeEvent;

namespace darkeye
{

class AsyncImageLabel;
enum class ImageFitMode;

class ImageDropWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ImageDropWidget(QString managedDirectory, QWidget *parent = nullptr);

    void setPurpose(const QString &purpose, const QString &placeholder = {});
    [[nodiscard]] QString purpose() const;
    void setFitMode(ImageFitMode mode);
    void setPreviewAspectRatio(qreal aspectRatio);
    void setQualityBadgeEnabled(bool enabled);
    void setImagePath(const QString &path);
    [[nodiscard]] QString imagePath() const;
    [[nodiscard]] QString resolvedImagePath() const;
    [[nodiscard]] bool isDirty() const noexcept;
    void setDirty(bool dirty);
    void clearImage();

    bool persistAsJpeg(const QString &fileName, QString *relativePath,
                       QString *errorMessage = nullptr) const;

signals:
    void imageChanged(const QString &path);
    void imageRejected(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    bool chooseImage();
    bool acceptImage(const QString &path, QString *errorMessage = nullptr);
    void updatePreviewGeometry();
    void updateQualityBadge();
    void refreshStyle();

    QString m_managedDirectory;
    QString m_purpose = QStringLiteral("人物头像");
    QString m_imagePath;
    AsyncImageLabel *m_preview = nullptr;
    QLabel *m_qualityBadge = nullptr;
    qreal m_previewAspectRatio = 0.0;
    bool m_qualityBadgeEnabled = false;
    bool m_dirty = false;
};

} // namespace darkeye
