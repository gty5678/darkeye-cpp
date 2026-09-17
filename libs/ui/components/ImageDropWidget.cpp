#include "ui/components/ImageDropWidget.h"

#include "ui/components/AsyncImageLabel.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSaveFile>
#include <QUrl>
#include <QVBoxLayout>

namespace darkeye
{

ImageDropWidget::ImageDropWidget(QString managedDirectory, QWidget *parent)
    : QWidget(parent), m_managedDirectory(QDir::cleanPath(std::move(managedDirectory)))
{
    setObjectName(QStringLiteral("ImageDropWidget"));
    setAcceptDrops(true);
    setMinimumSize(220, 260);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    m_preview = new AsyncImageLabel(this);
    m_preview->setObjectName(QStringLiteral("ImageDropPreview"));
    m_preview->setPlaceholderText(QStringLiteral("点击或拖入人物头像"));
    m_preview->setFitMode(ImageFitMode::Contain);
    m_preview->setMinimumSize(200, 240);
    m_preview->installEventFilter(this);
    layout->addWidget(m_preview);
    refreshStyle();
}

void ImageDropWidget::setPurpose(const QString &purpose, const QString &placeholder)
{
    m_purpose = purpose.trimmed().isEmpty() ? QStringLiteral("图片") : purpose.trimmed();
    m_preview->setPlaceholderText(placeholder.trimmed().isEmpty()
                                      ? QStringLiteral("点击或拖入%1").arg(m_purpose)
                                      : placeholder);
}

QString ImageDropWidget::purpose() const
{
    return m_purpose;
}

void ImageDropWidget::setImagePath(const QString &path)
{
    m_imagePath = path.trimmed();
    if (m_imagePath.isEmpty())
        m_preview->clearSource();
    else
        m_preview->setSource(resolvedImagePath());
    setDirty(false);
}

QString ImageDropWidget::imagePath() const
{
    return m_imagePath;
}

QString ImageDropWidget::resolvedImagePath() const
{
    if (m_imagePath.isEmpty())
        return {};
    const QFileInfo info(m_imagePath);
    return info.isAbsolute() ? QDir::cleanPath(m_imagePath)
                             : QDir(m_managedDirectory).filePath(m_imagePath);
}

bool ImageDropWidget::isDirty() const noexcept
{
    return m_dirty;
}

void ImageDropWidget::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    refreshStyle();
}

void ImageDropWidget::clearImage()
{
    if (m_imagePath.isEmpty())
        return;
    m_imagePath.clear();
    m_preview->clearSource();
    setDirty(true);
    emit imageChanged({});
}

bool ImageDropWidget::persistAsJpeg(const QString &fileName, QString *relativePath,
                                    QString *errorMessage) const
{
    if (relativePath == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("未提供%1保存结果").arg(m_purpose);
        return false;
    }
    relativePath->clear();
    if (m_imagePath.isEmpty())
        return true;

    QImageReader reader(resolvedImagePath());
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("无法读取%1：%2").arg(m_purpose, reader.errorString());
        return false;
    }
    if (!QDir().mkpath(m_managedDirectory))
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("无法创建%1目录：%2").arg(m_purpose, m_managedDirectory);
        return false;
    }

    QString safeName = fileName.trimmed();
    for (const QChar character : QStringLiteral("\\/:*?\"<>|"))
        safeName.replace(character, QChar('_'));
    if (!safeName.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive))
        safeName += QStringLiteral(".jpg");
    if (safeName == QStringLiteral(".jpg"))
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("%1文件名不能为空").arg(m_purpose);
        return false;
    }

    const QString targetPath = QDir(m_managedDirectory).filePath(safeName);
    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly))
    {
        if (errorMessage != nullptr)
            *errorMessage = target.errorString();
        return false;
    }
    if (image.hasAlphaChannel())
    {
        QImage flattened(image.size(), QImage::Format_RGB32);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, image);
        image = flattened;
    }
    if (!image.save(&target, "JPEG", 92) || !target.commit())
    {
        if (errorMessage != nullptr)
            *errorMessage = target.errorString().isEmpty()
                                ? QStringLiteral("%1写入失败").arg(m_purpose)
                                : target.errorString();
        return false;
    }
    *relativePath = safeName;
    return true;
}

bool ImageDropWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_preview && event->type() == QEvent::MouseButtonPress)
    {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
            return chooseImage();
    }
    return QWidget::eventFilter(watched, event);
}

void ImageDropWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ImageDropWidget::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls())
    {
        if (url.isLocalFile() && acceptImage(url.toLocalFile()))
        {
            event->acceptProposedAction();
            return;
        }
    }
}

void ImageDropWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *select = menu.addAction(QStringLiteral("选择%1").arg(m_purpose));
    QAction *clear = menu.addAction(QStringLiteral("清除%1").arg(m_purpose));
    clear->setEnabled(!m_imagePath.isEmpty());
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == select)
        chooseImage();
    else if (chosen == clear)
        clearImage();
}

bool ImageDropWidget::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择%1").arg(m_purpose), QString(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.bmp *.gif *.webp)"));
    return path.isEmpty() ? false : acceptImage(path);
}

bool ImageDropWidget::acceptImage(const QString &path, QString *errorMessage)
{
    QImageReader reader(path);
    if (!reader.canRead())
    {
        const QString message = QStringLiteral("不是可读取的图片文件：%1").arg(path);
        if (errorMessage != nullptr)
            *errorMessage = message;
        emit imageRejected(message);
        return false;
    }
    m_imagePath = QDir::cleanPath(path);
    m_preview->setSource(m_imagePath);
    setDirty(true);
    emit imageChanged(m_imagePath);
    return true;
}

void ImageDropWidget::refreshStyle()
{
    const QString border = m_dirty ? QStringLiteral("#ff9800") : QStringLiteral("#8a8a8a");
    m_preview->setStyleSheet(
        QStringLiteral("#ImageDropPreview { border: 2px dashed %1; border-radius: 4px; }")
            .arg(border));
}

} // namespace darkeye


