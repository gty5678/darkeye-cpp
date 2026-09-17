#include "ui/components/FanartStripWidget.h"

#include "services/ImageFetchService.h"
#include "ui/components/AsyncImageLabel.h"
#include "darkeye_ui/components/DesignButton.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <utility>

namespace darkeye
{

FanartStripWidget::FanartStripWidget(QString fanartDirectory, QString legacyCoverDirectory,
                                     QUrl imageFetchEndpoint, QWidget *parent)
    : QWidget(parent), m_fanartDirectory(QDir::cleanPath(std::move(fanartDirectory))),
      m_legacyCoverDirectory(QDir::cleanPath(std::move(legacyCoverDirectory)))
{
    qRegisterMetaType<QList<FanartEntry>>();
    setObjectName(QStringLiteral("FanartStripWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("FanartScrollArea"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(126);
    auto *strip = new QWidget(scroll);
    strip->setObjectName(QStringLiteral("FanartStripContent"));
    m_stripLayout = new QHBoxLayout(strip);
    m_stripLayout->setContentsMargins(4, 4, 4, 4);
    m_stripLayout->setSpacing(8);
    m_stripLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    scroll->setWidget(strip);
    root->addWidget(scroll, 1);

    auto *actions = new QHBoxLayout;
    actions->addStretch();
    m_addUrlButton = new DesignButton(QStringLiteral("添加网址"), this);
    m_addUrlButton->setObjectName(QStringLiteral("FanartAddUrlButton"));
    m_addImageButton = new DesignButton(QStringLiteral("添加本地图片"), this);
    m_addImageButton->setObjectName(QStringLiteral("FanartAddImageButton"));
    actions->addWidget(m_addUrlButton);
    actions->addWidget(m_addImageButton);
    root->addLayout(actions);
    connect(m_addImageButton, &QPushButton::clicked, this, &FanartStripWidget::chooseLocalImage);
    connect(m_addUrlButton, &QPushButton::clicked, this, &FanartStripWidget::addUrl);
    m_imageFetch = new ImageFetchService(std::move(imageFetchEndpoint), this);
    connect(m_imageFetch, &ImageFetchService::requestFinished, this,
            [this](quint64 requestId, bool succeeded, const QString &destinationPath,
                   const QString &errorMessage)
            {
                if (requestId != m_activeRequestId)
                    return;
                const int index = m_activeDownloadIndex;
                m_activeRequestId = 0;
                m_activeDownloadIndex = -1;
                if (succeeded && index >= 0 && index < m_entries.size())
                {
                    m_entries[index].file = QFileInfo(destinationPath).fileName();
                    m_entries[index].localPath.clear();
                    rebuild();
                    emit fanartChanged(m_entries);
                }
                else
                {
                    rebuild();
                    if (!errorMessage.isEmpty())
                        emit imageRejected(errorMessage);
                }
                emit downloadStateChanged(false);
            });
    rebuild();
}

void FanartStripWidget::setEntries(const QList<FanartEntry> &entries)
{
    if (downloadInProgress())
    {
        const quint64 requestId = m_activeRequestId;
        m_activeRequestId = 0;
        m_activeDownloadIndex = -1;
        m_imageFetch->cancel(requestId);
        emit downloadStateChanged(false);
    }
    m_entries = entries;
    rebuild();
}

QList<FanartEntry> FanartStripWidget::entries() const
{
    return m_entries;
}

void FanartStripWidget::setUrlList(const QStringList &urls)
{
    if (downloadInProgress())
    {
        const quint64 requestId = m_activeRequestId;
        m_activeRequestId = 0;
        m_activeDownloadIndex = -1;
        m_imageFetch->cancel(requestId);
        emit downloadStateChanged(false);
    }
    QList<FanartEntry> replacement;
    for (const QString &value : urls)
    {
        const QString url = value.trimmed();
        if (!url.isEmpty())
            replacement.append({url, {}, {}});
    }
    QList<QString> oldUrls;
    for (const FanartEntry &entry : std::as_const(m_entries))
        oldUrls.append(entry.url.trimmed());
    QList<QString> newUrls;
    for (const FanartEntry &entry : std::as_const(replacement))
        newUrls.append(entry.url);
    if (oldUrls == newUrls)
        return;
    m_entries = replacement;
    rebuild();
    emit fanartChanged(m_entries);
}

void FanartStripWidget::setCanAdd(bool canAdd)
{
    m_canAdd = canAdd;
    m_addImageButton->setEnabled(canAdd && !downloadInProgress());
    m_addUrlButton->setEnabled(canAdd && !downloadInProgress());
}

bool FanartStripWidget::canAdd() const noexcept
{
    return m_canAdd;
}

bool FanartStripWidget::addLocalImage(const QString &path, QString *errorMessage)
{
    if (downloadInProgress())
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("请等待剧照下载完成");
        return false;
    }
    if (!m_canAdd)
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("请先填写番号后再添加剧照");
        return false;
    }
    QImageReader reader(path);
    if (!reader.canRead())
    {
        const QString message = QStringLiteral("不是可读取的图片文件：%1").arg(path);
        if (errorMessage != nullptr)
            *errorMessage = message;
        emit imageRejected(message);
        return false;
    }
    m_entries.append({{}, {}, QDir::cleanPath(path)});
    rebuild();
    emit fanartChanged(m_entries);
    return true;
}

bool FanartStripWidget::removeEntry(int index)
{
    if (downloadInProgress() || index < 0 || index >= m_entries.size())
        return false;
    m_entries.removeAt(index);
    rebuild();
    emit fanartChanged(m_entries);
    return true;
}

bool FanartStripWidget::moveEntry(int from, int to)
{
    if (downloadInProgress() || from < 0 || from >= m_entries.size() || to < 0 ||
        to >= m_entries.size() || from == to)
        return false;
    m_entries.move(from, to);
    rebuild();
    emit fanartChanged(m_entries);
    return true;
}

bool FanartStripWidget::updateEntry(int index, const QString &url, const QString &file)
{
    if (downloadInProgress() || index < 0 || index >= m_entries.size())
        return false;
    m_entries[index].url = url.trimmed();
    m_entries[index].file = file.trimmed();
    rebuild();
    emit fanartChanged(m_entries);
    return true;
}

bool FanartStripWidget::downloadEntry(int index)
{
    if (downloadInProgress() || index < 0 || index >= m_entries.size())
        return false;
    const QUrl sourceUrl(m_entries.at(index).url.trimmed());
    const QString fileName = ImageFetchService::suggestedJpegFileName(sourceUrl);
    if (fileName.isEmpty())
    {
        emit imageRejected(QStringLiteral("剧照网址没有可用的文件名"));
        return false;
    }
    m_activeDownloadIndex = index;
    m_activeRequestId =
        m_imageFetch->fetchToJpeg(sourceUrl, QDir(m_fanartDirectory).filePath(fileName));
    m_addImageButton->setEnabled(false);
    m_addUrlButton->setEnabled(false);
    rebuild();
    emit downloadStateChanged(true);
    return true;
}

bool FanartStripWidget::cancelDownload()
{
    return downloadInProgress() && m_imageFetch->cancel(m_activeRequestId);
}

bool FanartStripWidget::downloadInProgress() const noexcept
{
    return m_activeRequestId != 0;
}

bool FanartStripWidget::finalizedEntries(const QString &serialNumber,
                                         QList<FanartEntry> *finalEntries,
                                         QStringList *createdFiles, QString *errorMessage) const
{
    if (downloadInProgress())
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("请等待剧照下载完成后再保存作品");
        return false;
    }
    if (finalEntries == nullptr || createdFiles == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("未提供剧照保存结果");
        return false;
    }
    finalEntries->clear();
    createdFiles->clear();
    QString serialBase = serialNumber.trimmed().toLower();
    serialBase.remove(QChar('-'));
    for (const QChar character : QStringLiteral("\\/:*?\"<>|"))
        serialBase.replace(character, QChar('_'));
    if (serialBase.isEmpty())
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("番号不能为空");
        return false;
    }
    if (!QDir().mkpath(m_fanartDirectory))
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("无法创建剧照目录：%1").arg(m_fanartDirectory);
        return false;
    }

    for (FanartEntry entry : m_entries)
    {
        entry.url = entry.url.trimmed();
        entry.file = entry.file.trimmed();
        entry.localPath = entry.localPath.trimmed();
        if (!entry.localPath.isEmpty())
        {
            const QString name =
                QStringLiteral("%1_fa_%2.jpg")
                    .arg(serialBase, QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
            const QString targetPath = QDir(m_fanartDirectory).filePath(name);
            if (!saveAsJpeg(entry.localPath, targetPath, errorMessage))
            {
                for (const QString &created : std::as_const(*createdFiles))
                    QFile::remove(created);
                finalEntries->clear();
                createdFiles->clear();
                return false;
            }
            createdFiles->append(targetPath);
            entry.file = name;
            entry.localPath.clear();
        }
        if (!entry.url.isEmpty() || !entry.file.isEmpty())
            finalEntries->append(entry);
    }
    return true;
}

bool FanartStripWidget::parseJson(const QString &json, QList<FanartEntry> *entries,
                                  QString *errorMessage)
{
    if (entries == nullptr)
        return false;
    entries->clear();
    if (json.trimmed().isEmpty())
        return true;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
    {
        if (errorMessage != nullptr)
            *errorMessage =
                parseError.error == QJsonParseError::NoError
                    ? QStringLiteral("剧照 JSON 无效：根节点必须是数组")
                    : QStringLiteral("剧照 JSON 无效：%1").arg(parseError.errorString());
        return false;
    }
    for (const QJsonValue &value : document.array())
    {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        const FanartEntry entry{object.value(QStringLiteral("url")).toString().trimmed(),
                                object.value(QStringLiteral("file")).toString().trimmed(),
                                {}};
        if (!entry.url.isEmpty() || !entry.file.isEmpty())
            entries->append(entry);
    }
    return true;
}

QString FanartStripWidget::toJson(const QList<FanartEntry> &entries)
{
    QJsonArray array;
    for (const FanartEntry &entry : entries)
    {
        const QString url = entry.url.trimmed();
        const QString file = entry.file.trimmed();
        if (url.isEmpty() && file.isEmpty())
            continue;
        QJsonObject object;
        object.insert(QStringLiteral("url"), url);
        object.insert(QStringLiteral("file"), file);
        array.append(object);
    }
    return array.isEmpty() ? QString()
                           : QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void FanartStripWidget::chooseLocalImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择剧照"), {},
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp *.gif)"));
    if (!path.isEmpty())
        addLocalImage(path);
}

void FanartStripWidget::addUrl()
{
    if (!m_canAdd || downloadInProgress())
        return;
    bool accepted = false;
    const QString url =
        QInputDialog::getText(this, QStringLiteral("添加剧照网址"), QStringLiteral("HTTP(S) 地址"),
                              QLineEdit::Normal, {}, &accepted)
            .trimmed();
    if (!accepted || url.isEmpty())
        return;
    const QUrl parsed(url);
    if (!parsed.isValid() ||
        (parsed.scheme() != QStringLiteral("http") && parsed.scheme() != QStringLiteral("https")))
    {
        emit imageRejected(QStringLiteral("网址需要以 http(s) 开头"));
        return;
    }
    m_entries.append({url, {}, {}});
    rebuild();
    emit fanartChanged(m_entries);
}

void FanartStripWidget::editEntry(int index)
{
    if (downloadInProgress() || index < 0 || index >= m_entries.size())
        return;
    bool accepted = false;
    const QString url =
        QInputDialog::getText(this, QStringLiteral("编辑剧照网址"), QStringLiteral("HTTP(S) 地址"),
                              QLineEdit::Normal, m_entries.at(index).url, &accepted)
            .trimmed();
    if (!accepted)
        return;
    if (!url.isEmpty())
    {
        const QUrl parsed(url);
        if (!parsed.isValid() || (parsed.scheme() != QStringLiteral("http") &&
                                  parsed.scheme() != QStringLiteral("https")))
        {
            emit imageRejected(QStringLiteral("网址需要以 http(s) 开头"));
            return;
        }
    }
    updateEntry(index, url, m_entries.at(index).file);
}

void FanartStripWidget::rebuild()
{
    while (QLayoutItem *item = m_stripLayout->takeAt(0))
    {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    for (int index = 0; index < m_entries.size(); ++index)
    {
        const FanartEntry &entry = m_entries.at(index);
        auto *cell = new QWidget(this);
        cell->setObjectName(QStringLiteral("FanartCell"));
        cell->setFixedSize(180, 112);
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(2, 2, 2, 2);
        cellLayout->setSpacing(2);
        auto *preview = new AsyncImageLabel(cell);
        preview->setObjectName(QStringLiteral("FanartPreview"));
        preview->setFixedSize(176, 78);
        preview->setFitMode(ImageFitMode::Cover);
        preview->setPlaceholderText(
            index == m_activeDownloadIndex
                ? QStringLiteral("下载中…")
                : (entry.url.isEmpty() ? QStringLiteral("无本地剧照") : QStringLiteral("待下载")));
        const QString path = resolvedPath(entry);
        if (!path.isEmpty())
            preview->setSource(path);
        cellLayout->addWidget(preview);
        auto *buttons = new QHBoxLayout;
        auto addMoveButton = [&](const QString &text, int destination)
        {
            auto *button = new QPushButton(text, cell);
            button->setFixedWidth(28);
            button->setEnabled(!downloadInProgress() && destination >= 0 &&
                               destination < m_entries.size());
            connect(button, &QPushButton::clicked, this,
                    [this, index, destination]() { moveEntry(index, destination); });
            buttons->addWidget(button);
        };
        addMoveButton(QStringLiteral("←"), index - 1);
        auto *position = new QLabel(QStringLiteral("%1").arg(index + 1), cell);
        position->setAlignment(Qt::AlignCenter);
        position->setToolTip(entry.url.isEmpty() ? entry.file : entry.url);
        buttons->addWidget(position, 1);
        addMoveButton(QStringLiteral("→"), index + 1);
        auto *download = new QPushButton(
            index == m_activeDownloadIndex ? QStringLiteral("停") : QStringLiteral("下"), cell);
        download->setObjectName(QStringLiteral("FanartDownloadButton_%1").arg(index));
        download->setFixedWidth(28);
        download->setToolTip(index == m_activeDownloadIndex ? QStringLiteral("取消下载")
                                                            : QStringLiteral("下载远程剧照"));
        download->setEnabled(index == m_activeDownloadIndex ||
                             (!downloadInProgress() && !entry.url.trimmed().isEmpty()));
        connect(download, &QPushButton::clicked, this,
                [this, index]
                {
                    if (index == m_activeDownloadIndex)
                        cancelDownload();
                    else
                        downloadEntry(index);
                });
        buttons->addWidget(download);
        auto *edit = new QPushButton(QStringLiteral("编"), cell);
        edit->setFixedWidth(28);
        edit->setEnabled(!downloadInProgress());
        connect(edit, &QPushButton::clicked, this, [this, index]() { editEntry(index); });
        buttons->addWidget(edit);
        auto *remove = new QPushButton(QStringLiteral("×"), cell);
        remove->setFixedWidth(28);
        remove->setEnabled(!downloadInProgress());
        connect(remove, &QPushButton::clicked, this, [this, index]() { removeEntry(index); });
        buttons->addWidget(remove);
        cellLayout->addLayout(buttons);
        m_stripLayout->addWidget(cell);
    }
    m_stripLayout->addStretch();
    m_addImageButton->setEnabled(m_canAdd && !downloadInProgress());
    m_addUrlButton->setEnabled(m_canAdd && !downloadInProgress());
}

QString FanartStripWidget::resolvedPath(const FanartEntry &entry) const
{
    if (!entry.localPath.isEmpty() && QFileInfo::exists(entry.localPath))
        return entry.localPath;
    if (entry.file.isEmpty())
        return {};
    const QString fanartPath = QDir(m_fanartDirectory).filePath(entry.file);
    if (QFileInfo::exists(fanartPath))
        return fanartPath;
    const QString legacyPath = QDir(m_legacyCoverDirectory).filePath(entry.file);
    return QFileInfo::exists(legacyPath) ? legacyPath : QString();
}

bool FanartStripWidget::saveAsJpeg(const QString &sourcePath, const QString &targetPath,
                                   QString *errorMessage)
{
    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
    {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("无法读取剧照：%1").arg(reader.errorString());
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
    QSaveFile output(targetPath);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "JPEG", 92) || !output.commit())
    {
        if (errorMessage != nullptr)
            *errorMessage = output.errorString().isEmpty() ? QStringLiteral("剧照写入失败")
                                                           : output.errorString();
        return false;
    }
    return true;
}

} // namespace darkeye


