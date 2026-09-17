#pragma once

#include <QWidget>

class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;

namespace darkeye
{

class AsyncImageLabel;

class ImageDropWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ImageDropWidget(QString managedDirectory, QWidget *parent = nullptr);

    void setPurpose(const QString &purpose, const QString &placeholder = {});
    [[nodiscard]] QString purpose() const;
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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    bool chooseImage();
    bool acceptImage(const QString &path, QString *errorMessage = nullptr);
    void refreshStyle();

    QString m_managedDirectory;
    QString m_purpose = QStringLiteral("人物头像");
    QString m_imagePath;
    AsyncImageLabel *m_preview = nullptr;
    bool m_dirty = false;
};

} // namespace darkeye
