#include "ui/components/DvdShelfView.h"

#include "database/SqliteConnection.h"
#include "database/repositories/PrivateRepository.h"
#include "database/repositories/WorkRepository.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "graph/GraphManager.h"
#include "graph_view/GraphViewWidget.h"
#include "settings/Settings.h"
#include "ui/components/FanartStripWidget.h"
#include "utils/MediaUtils.h"

#include <QApplication>
#include <QAction>
#include <QClipboard>
#include <QColor>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQmlContext>
#include <QRunnable>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

namespace darkeye
{

namespace
{

QString fileUrl(const QString &path)
{
    return QUrl::fromLocalFile(QDir::cleanPath(path)).toString(QUrl::FullyEncoded);
}

QString assetDirectory(const QString &name)
{
    return QDir(QStringLiteral(DARKEYE_SOURCE_DIR)).filePath(QStringLiteral("resources/") + name);
}

void trimThumbnailCache()
{
    constexpr int cacheLimit = 900;
    constexpr qint64 cacheBytesLimit = 900LL * 1024 * 1024;
    QDir directory(QDir(QDir::tempPath()).filePath(QStringLiteral("darkeye/dvd_3d_tex")));
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.jpg")}, QDir::Files,
                                                        QDir::Time);
    qint64 totalBytes = 0;
    for (const QFileInfo &file : files) totalBytes += file.size();
    for (int index = files.size() - 1;
         index >= 0 && (index >= cacheLimit || totalBytes > cacheBytesLimit); --index)
    {
        const QFileInfo &file = files.at(index);
        if (QFile::remove(file.absoluteFilePath())) totalBytes -= file.size();
    }
}

QColor readableTextColor(const QString &background)
{
    const QColor color(background);
    return color.isValid() && color.lightness() < 135 ? QColor(Qt::white) : QColor(Qt::black);
}

} // namespace

DvdShelfView::DvdShelfView(QSqlDatabase database, QSqlDatabase privateDatabase,
                           graph::GraphManager *graphManager, const QString &coverDirectory,
                           const QString &fanartDirectory, QWidget *parent)
    : QWidget(parent), m_database(std::move(database)), m_privateDatabase(std::move(privateDatabase)),
      m_graphManager(graphManager), m_coverDirectory(coverDirectory),
      m_fanartDirectory(fanartDirectory), m_quickWidget(new QQuickWidget(this))
{
    setObjectName(QStringLiteral("DvdShelfView"));
    // Two decoders prevent a large cover collection from briefly consuming
    // several gigabytes while its thumbnails are prepared.
    m_thumbnailPool.setMaxThreadCount(2);
    trimThumbnailCache();
    m_thumbnailRefreshTimer = new QTimer(this);
    m_thumbnailRefreshTimer->setSingleShot(true);
    m_thumbnailRefreshTimer->setInterval(50);
    connect(m_thumbnailRefreshTimer, &QTimer::timeout, this, [this] {
        m_thumbnailRefreshPending = false;
        refreshVisibleWindow(true);
    });
    m_cameraTimer = new QTimer(this);
    m_cameraTimer->setInterval(16);
    connect(m_cameraTimer, &QTimer::timeout, this, &DvdShelfView::advanceCamera);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quickWidget);
    if (m_graphManager != nullptr)
    {
        m_relationGraphContainer = new QWidget(this);
        m_relationGraphContainer->setObjectName(QStringLiteral("ShelfRelationGraphOverlay"));
        auto *graphLayout = new QVBoxLayout(m_relationGraphContainer);
        graphLayout->setContentsMargins(0, 0, 0, 0);
        m_relationGraph = new graph_view::GraphViewWidget(*m_graphManager, m_relationGraphContainer);
        graphLayout->addWidget(m_relationGraph);
        connect(m_relationGraph, &graph_view::GraphViewWidget::nodeLeftClicked, this,
                [this](const QString &nodeId) {
                    if (nodeId.size() < 2) return;
                    bool ok = false;
                    const qint64 id = nodeId.mid(1).toLongLong(&ok);
                    if (!ok || id <= 0) return;
                    if (nodeId.startsWith(QLatin1Char('w')))
                        emit workSelected(id);
                    else if (nodeId.startsWith(QLatin1Char('a')))
                        emit actressRequested(id);
                });
        m_relationGraphContainer->hide();
    }
    m_fanartContainer = new QWidget(this);
    m_fanartContainer->setObjectName(QStringLiteral("ShelfFanartOverlay"));
    auto *fanartLayout = new QVBoxLayout(m_fanartContainer);
    fanartLayout->setContentsMargins(6, 4, 6, 4);
    m_fanartStrip = new FanartStripWidget(m_fanartDirectory, m_coverDirectory, {}, m_fanartContainer);
    m_fanartStrip->setCanAdd(false);
    m_fanartStrip->setPreviewMode(true);
    fanartLayout->addWidget(m_fanartStrip);
    m_fanartContainer->hide();
    connect(m_fanartStrip, &FanartStripWidget::fanartChanged,
            this, &DvdShelfView::persistFanartPreview);
    m_relationGraphTimer = new QTimer(this);
    m_relationGraphTimer->setSingleShot(true);
    m_relationGraphTimer->setInterval(500);
    connect(m_relationGraphTimer, &QTimer::timeout, this, [this] {
        if (m_overlayWorkId > 0 && m_relationGraphContainer != nullptr)
            showRelationGraph(m_overlayWorkId);
    });
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->setMinimumHeight(600);
    m_quickWidget->setClearColor(Qt::transparent);
    auto *context = m_quickWidget->rootContext();
    context->setContextProperty(QStringLiteral("dvdBridge"), this);
    // dvd_scene.qml uses this unguarded for every Loader3D delegate.  Keep
    // the Python setup contract, otherwise filtering recreates delegates
    // whose scale binding throws and leaves the shelf visually empty.
    context->setContextProperty(QStringLiteral("modelScale"), 1.0);
    context->setContextProperty(QStringLiteral("dvdQmlUrl"),
                                QUrl::fromLocalFile(assetDirectory("qml/dvd/Dvd.qml")));
    context->setContextProperty(QStringLiteral("dvdCount"), 0);
    context->setContextProperty(QStringLiteral("dvdTextureSources"), QVariantList{});
    context->setContextProperty(QStringLiteral("dvdVisibleStart"), 0);
    context->setContextProperty(QStringLiteral("dvdShelfLength"), 0.0);
    context->setContextProperty(QStringLiteral("dvdSpacing"), 0.0145);
    context->setContextProperty(QStringLiteral("cameraDistance"), 0.25);
    context->setContextProperty(QStringLiteral("selectedDvdDistance"), 0.2);
    context->setContextProperty(QStringLiteral("showWireframe"), false);
    context->setContextProperty(QStringLiteral("meshesPath"), fileUrl(assetDirectory("meshes")) + "/");
    context->setContextProperty(QStringLiteral("mapsPath"), fileUrl(assetDirectory("maps")) + "/");
    context->setContextProperty(QStringLiteral("hdrPath"), fileUrl(assetDirectory("hdr")) + "/");
    m_quickWidget->setSource(QUrl::fromLocalFile(assetDirectory("qml/dvd/dvd_scene.qml")));
}

void DvdShelfView::setWorks(const QList<WorkSummary> &works)
{
    // A filtering reload invalidates any delayed route-open callback whose
    // delegate belonged to the previous virtualized window.
    ++m_openWorkRequest;
    m_works = works;
    m_cameraX = 0;
    m_cameraTargetX = 0;
    m_cameraVelocityX = 0;
    m_cameraElapsed.invalidate();
    m_cameraTimer->stop();
    m_visibleStart = 0;
    m_loadedStart = -1;
    m_textureCache.clear();
    m_textureCacheOrder.clear();
    m_thumbnailInFlight.clear();
    m_thumbnailRefreshPending = false;
    clearExpandedMeta();
    hideOverlays();
    refreshVisibleWindow();
}

void DvdShelfView::updateScene()
{
    refreshVisibleWindow();
}

void DvdShelfView::refreshVisibleWindow(bool force)
{
    constexpr int windowSize = 60;
    constexpr int maxShiftPerUpdate = 10;
    constexpr qreal spacing = 0.0145;
    const int count = m_works.size();
    int desiredStart = qBound(0, qRound(m_cameraX / spacing) - windowSize / 2,
                              qMax(0, count - windowSize));
    // Keep the Python shelf's window policy: once the camera moves beyond the
    // centered margin, follow it immediately, but constrain a single update
    // to ten covers so a large programmatic jump never rebuilds every delegate.
    if (!force && m_loadedStart >= 0)
    {
        const int shift = desiredStart - m_loadedStart;
        if (shift == 0) return;
        desiredStart = m_loadedStart + qBound(-maxShiftPerUpdate, shift, maxShiftPerUpdate);
    }
    m_visibleStart = desiredStart;
    m_loadedStart = desiredStart;
    const QList<WorkSummary> visible = m_works.mid(m_visibleStart, windowSize);
    QVariantList textures;
    textures.reserve(visible.size());
    for (const WorkSummary &work : visible)
    {
        textures.append(textureForWork(work));
    }
    auto *context = m_quickWidget->rootContext();
    context->setContextProperty(QStringLiteral("dvdVisibleStart"), m_visibleStart);
    context->setContextProperty(QStringLiteral("dvdCount"), visible.size());
    context->setContextProperty(QStringLiteral("dvdTextureSources"), textures);
    context->setContextProperty(QStringLiteral("dvdShelfLength"),
                                qMax(0, count - 1) * spacing);
}

QString DvdShelfView::textureForWork(const WorkSummary &work)
{
    const QString placeholder = fileUrl(assetDirectory("maps/0.png"));
    if (const auto cached = m_textureCache.constFind(work.id); cached != m_textureCache.cend()) {
        m_textureCacheOrder.removeAll(work.id);
        m_textureCacheOrder.append(work.id);
        return *cached;
    }
    QString source = work.imageUrl;
    if (!source.isEmpty() && !QFileInfo(source).isAbsolute())
        source = QDir(m_coverDirectory).filePath(source);
    const QFileInfo info(source);
    if (source.isEmpty() || !info.exists())
    {
        cacheTexture(work.id, placeholder);
        return placeholder;
    }
    if (info.size() <= 320 * 1024)
    {
        const QString url = fileUrl(source);
        cacheTexture(work.id, url);
        return url;
    }
    const QString cacheDirectory = QDir(QDir::tempPath()).filePath(QStringLiteral("darkeye/dvd_3d_tex"));
    QDir().mkpath(cacheDirectory);
    const QString thumb = QDir(cacheDirectory).filePath(
        QStringLiteral("%1_%2_%3.jpg").arg(work.id).arg(info.lastModified().toMSecsSinceEpoch()).arg(info.size()));
    if (QFileInfo::exists(thumb))
    {
        const QString url = fileUrl(thumb);
        cacheTexture(work.id, url);
        return url;
    }
    queueThumbnail(work.id, source, thumb);
    return placeholder;
}

void DvdShelfView::cacheTexture(qint64 workId, const QString &texture)
{
    constexpr int cacheLimit = 320;
    m_textureCache.insert(workId, texture);
    m_textureCacheOrder.removeAll(workId);
    m_textureCacheOrder.append(workId);
    while (m_textureCacheOrder.size() > cacheLimit)
        m_textureCache.remove(m_textureCacheOrder.takeFirst());
}

void DvdShelfView::queueThumbnail(qint64 workId, const QString &sourcePath, const QString &thumbnailPath)
{
    if (m_thumbnailInFlight.contains(workId)) return;
    m_thumbnailInFlight.insert(workId);
    QPointer<DvdShelfView> view(this);
    m_thumbnailPool.start(QRunnable::create([view, workId, sourcePath, thumbnailPath] {
        QImageReader reader(sourcePath);
        reader.setAutoTransform(true);
        QSize size = reader.size();
        if (size.isValid() && qMax(size.width(), size.height()) > 2048)
            reader.setScaledSize(size.scaled(2048, 2048, Qt::KeepAspectRatio));
        const QImage image = reader.read();
        const bool saved = !image.isNull() && image.save(thumbnailPath, "JPEG", 88);
        QMetaObject::invokeMethod(qApp, [view, workId, sourcePath, thumbnailPath, saved] {
            if (!view) return;
            view->m_thumbnailInFlight.remove(workId);
            view->cacheTexture(workId, fileUrl(saved ? thumbnailPath : sourcePath));
            // Updating dvdTextureSources forces Qt Quick 3D to synchronize and
            // potentially upload a texture.  Never do that while the camera is
            // moving: collecting completed thumbnails is cheap; applying them
            // after it settles keeps panning independent of disk/image work.
            view->m_thumbnailRefreshPending = true;
            if (!view->m_cameraTimer->isActive()
                && !view->m_thumbnailRefreshTimer->isActive())
                view->m_thumbnailRefreshTimer->start();
        }, Qt::QueuedConnection);
    }));
}

qint64 DvdShelfView::workIdAt(int virtualIndex) const
{
    return virtualIndex >= 0 && virtualIndex < m_works.size() ? m_works.at(virtualIndex).id : 0;
}

void DvdShelfView::set_camera_x(qreal cameraX)
{
    constexpr qreal spacing = 0.0145;
    constexpr qreal settleEpsilon = spacing * 0.02;
    constexpr qreal settleVelocity = spacing * 0.1;
    m_cameraTargetX = qBound<qreal>(0, cameraX, qMax(0, m_works.size() - 1) * spacing);
    emit cameraTargetXChanged();
    if (qAbs(m_cameraX - m_cameraTargetX) <= settleEpsilon
        && qAbs(m_cameraVelocityX) <= settleVelocity)
    {
        m_cameraVelocityX = 0;
        m_cameraX = m_cameraTargetX;
        emit cameraXChanged();
        return;
    }
    if (!m_cameraTimer->isActive())
    {
        m_cameraElapsed.restart();
        m_cameraTimer->start();
    }
}

void DvdShelfView::scroll_camera_by(qreal delta, qreal) { set_camera_x(m_cameraTargetX + delta); }

void DvdShelfView::advanceCamera()
{
    constexpr qreal spacing = 0.0145;
    constexpr qreal smoothTime = 0.11;
    constexpr qreal maxSpeed = spacing * 96;
    constexpr qreal settleEpsilon = spacing * 0.02;
    constexpr qreal settleVelocity = spacing * 0.1;
    const qreal deltaTime = qBound<qreal>(0.001, m_cameraElapsed.restart() / 1000.0, 0.05);
    const qreal omega = 2.0 / smoothTime;
    const qreal x = omega * deltaTime;
    const qreal exponential = 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x);
    qreal change = m_cameraX - m_cameraTargetX;
    const qreal originalTarget = m_cameraTargetX;
    const qreal maxChange = maxSpeed * smoothTime;
    change = qBound(-maxChange, change, maxChange);
    const qreal target = m_cameraX - change;
    const qreal temporary = (m_cameraVelocityX + omega * change) * deltaTime;
    m_cameraVelocityX = (m_cameraVelocityX - omega * temporary) * exponential;
    qreal nextX = target + (change + temporary) * exponential;
    if ((originalTarget - m_cameraX > 0) == (nextX > originalTarget))
    {
        nextX = originalTarget;
        m_cameraVelocityX = 0;
    }
    if (qAbs(m_cameraTargetX - nextX) <= settleEpsilon
        && qAbs(m_cameraVelocityX) <= settleVelocity)
    {
        m_cameraX = m_cameraTargetX;
        m_cameraVelocityX = 0;
        m_cameraTimer->stop();
    }
    else
    {
        m_cameraX = nextX;
    }
    emit cameraXChanged();
    // Texture/window replacement remains independent from the 60 FPS camera
    // animation, so an animation tick cannot rebuild the 3D scene by itself.
    refreshVisibleWindow(false);
    if (!m_cameraTimer->isActive() && m_thumbnailRefreshPending
        && !m_thumbnailRefreshTimer->isActive())
        m_thumbnailRefreshTimer->start();
}

void DvdShelfView::selection_changed(int selectedDelegateIndex, int expandedDelegateIndex)
{
    const int localIndex = expandedDelegateIndex >= 0 ? expandedDelegateIndex : selectedDelegateIndex;
    if (localIndex < 0)
    {
        m_suppressRelationGraphUntilDeselected = false;
        clearExpandedMeta();
        hideOverlays();
        return;
    }
    const qint64 workId = workIdAt(m_visibleStart + localIndex);
    if (expandedDelegateIndex >= 0)
    {
        m_suppressRelationGraphUntilDeselected = false;
        if (m_relationGraphTimer != nullptr) m_relationGraphTimer->stop();
        if (m_relationGraphContainer != nullptr) m_relationGraphContainer->hide();
    }
    else if (workId > 0 && !m_suppressRelationGraphUntilDeselected)
    {
        m_overlayWorkId = workId;
        if (m_relationGraphTimer != nullptr) m_relationGraphTimer->start();
    }
    refresh_expanded_work_meta(m_visibleStart + localIndex);
}

void DvdShelfView::refresh_expanded_work_meta(int virtualIndex)
{
    const qint64 workId = workIdAt(virtualIndex);
    if (!workId)
    {
        clearExpandedMeta();
        clearFanartOverlay();
        return;
    }
    QString error;
    SqliteConnection connection;
    if (!connection.open(m_database.databaseName(), true, &error)) return;
    const auto details = WorkRepository(connection.database()).findDetailsById(workId, &error);
    if (!details.has_value()) { clearExpandedMeta(); return; }
    const WorkDetails &value = *details;
    m_title = !value.work.chineseTitle.isEmpty() ? value.work.chineseTitle
                                                  : value.work.japaneseTitle;
    if (m_title.isEmpty()) m_title = value.work.serialNumber;
    m_story = !value.work.chineseStory.isEmpty() ? value.work.chineseStory
                                                  : value.work.japaneseStory;
    m_code = value.work.serialNumber;
    m_releaseDate = value.work.releaseDate;
    m_director = value.work.director;
    m_studio = value.makerName;
    m_label = value.labelName;
    m_series = value.seriesName;
    m_makerId = value.work.makerId.value_or(-1);
    m_labelId = value.work.labelId.value_or(-1);
    m_seriesId = value.work.seriesId.value_or(-1);
    m_hasLocalVideo = !value.work.videoUrl.trimmed().isEmpty();
    m_actresses.clear(); m_actors.clear(); m_tags.clear();
    for (const auto &person : value.actresses)
        m_actresses.append(QVariantMap{{"actress_id", person.id}, {"actress_name", person.name}});
    for (const auto &person : value.actors)
        m_actors.append(QVariantMap{{"actor_id", person.id}, {"actor_name", person.name}});
    for (const auto &tag : value.tags)
        m_tags.append(QVariantMap{{"tag_id", tag.id}, {"tag_name", tag.name},
                                  {"color", tag.color},
                                  {"text_color", readableTextColor(tag.color).name()}});
    emit expandedWorkMetaChanged();
    refresh_expanded_favorite_state(virtualIndex);

    QList<FanartEntry> fanartEntries;
    QString fanartError;
    if (m_fanartStrip != nullptr &&
        FanartStripWidget::parseJson(value.work.fanartJson, &fanartEntries, &fanartError))
    {
        m_overlayWorkId = workId;
        m_fanartStrip->setEntries(fanartEntries);
    }
    else if (m_fanartStrip != nullptr)
    {
        m_fanartStrip->setEntries({});
    }
}

void DvdShelfView::refresh_expanded_favorite_state(int virtualIndex)
{
    const qint64 workId = workIdAt(virtualIndex);
    QString error;
    SqliteConnection connection;
    const bool favorite = workId && m_privateDatabase.isValid()
        && connection.open(m_privateDatabase.databaseName(), true, &error)
        && PrivateRepository(connection.database()).isFavoriteWork(workId, &error);
    if (m_favorited != favorite) { m_favorited = favorite; emit expandedWorkFavoritedChanged(); }
}

void DvdShelfView::on_heart_clicked(int virtualIndex)
{
    const qint64 workId = workIdAt(virtualIndex);
    if (!workId || !m_privateDatabase.isValid()) return;
    QString error;
    SqliteConnection connection;
    if (!connection.open(m_privateDatabase.databaseName(), true, &error)) return;
    PrivateRepository privateRepository(connection.database());
    const bool changed = m_favorited
        ? privateRepository.removeFavoriteWork(workId, &error)
        : privateRepository.addFavoriteWork(workId, m_works.at(virtualIndex).serialNumber, &error);
    if (changed) { m_favorited = !m_favorited; emit expandedWorkFavoritedChanged(); }
}

void DvdShelfView::on_cd_clicked(int virtualIndex)
{
    const qint64 workId = workIdAt(virtualIndex);
    if (!workId) return;
    QString error;
    SqliteConnection connection;
    if (!connection.open(m_database.databaseName(), true, &error)) return;
    const auto work = WorkRepository(connection.database()).findById(workId, &error);
    if (!work.has_value()) return;
    QStringList paths;
    for (const QString &raw : work->videoUrl.split(',', Qt::SkipEmptyParts))
    {
        const QString path = raw.trimmed();
        if (!path.isEmpty()) paths.append(path);
    }
    if (paths.isEmpty())
    {
        ToastNotification::showMessage(window(), QStringLiteral("没有可播放的视频"),
                                       ToastNotification::Level::Info, 2500);
        return;
    }
    const auto play = [this](const QString &path) {
        if (!utils::playVideo(path, settings::app().localVideoPlayer))
            ToastNotification::showMessage(window(), QStringLiteral("无法播放视频：%1").arg(path),
                                           ToastNotification::Level::Error, 3500);
    };
    if (paths.size() == 1)
    {
        play(paths.constFirst());
        return;
    }
    QMenu menu(this);
    for (const QString &path : std::as_const(paths))
    {
        QAction *action = menu.addAction(QFileInfo(path).fileName());
        action->setData(path);
    }
    if (QAction *selected = menu.exec(QCursor::pos())) play(selected->data().toString());
}

void DvdShelfView::on_edit_clicked(int virtualIndex)
{
    const qint64 workId = workIdAt(virtualIndex);
    if (workId) emit editRequested(workId);
}

void DvdShelfView::on_delete_clicked(int virtualIndex)
{
    const qint64 workId = workIdAt(virtualIndex);
    if (!workId || QMessageBox::question(this, QStringLiteral("确认删除"),
        QStringLiteral("确定要删除该作品吗？")) != QMessageBox::Yes) return;
    QString error;
    SqliteConnection connection;
    if (!connection.open(m_database.databaseName(), false, &error)) return;
    if (WorkRepository(connection.database()).setDeleted(workId, true, &error))
    {
        ToastNotification::showSuccess(window(), QStringLiteral("已标记删除"));
        emit workDeleted(workId);
    }
}

void DvdShelfView::copy_to_clipboard(const QString &text) const
{
    if (text.isEmpty() || !QApplication::clipboard()) return;
    QApplication::clipboard()->setText(text);
    ToastNotification::showSuccess(window(), QStringLiteral("番号已复制"), nullptr, 2000);
}

void DvdShelfView::on_actress_clicked(qint64 actressId)
{
    if (actressId > 0) emit actressRequested(actressId);
}

void DvdShelfView::on_actor_clicked(qint64 actorId)
{
    if (actorId > 0) emit actorRequested(actorId);
}

void DvdShelfView::on_tag_clicked(qint64 tagId)
{
    if (tagId > 0) emit tagRequested(tagId);
}

void DvdShelfView::on_director_clicked()
{
    if (!m_director.isEmpty()) emit directorRequested(m_director);
}

void DvdShelfView::on_studio_clicked()
{
    if (m_makerId > 0) emit makerRequested(m_makerId);
}

void DvdShelfView::on_label_clicked()
{
    if (m_labelId > 0) emit labelRequested(m_labelId);
}

void DvdShelfView::on_series_clicked()
{
    if (m_seriesId > 0) emit seriesRequested(m_seriesId);
}

void DvdShelfView::clearExpandedMeta()
{
    m_title.clear(); m_story.clear(); m_code.clear(); m_releaseDate.clear(); m_director.clear();
    m_studio.clear(); m_label.clear(); m_series.clear(); m_actresses.clear(); m_actors.clear(); m_tags.clear();
    m_makerId = m_labelId = m_seriesId = -1; m_hasLocalVideo = false; m_favorited = false;
    emit expandedWorkMetaChanged(); emit expandedWorkFavoritedChanged();
}

void DvdShelfView::clearFanartOverlay()
{
    m_fanartLeft = m_fanartTop = -1;
    m_fanartWidth = 0;
    if (m_fanartStrip != nullptr) m_fanartStrip->setEntries({});
    if (m_fanartContainer != nullptr) m_fanartContainer->hide();
}

bool DvdShelfView::openWork(qint64 workId)
{
    const auto iterator = std::find_if(m_works.cbegin(), m_works.cend(),
                                       [workId](const WorkSummary &work) { return work.id == workId; });
    if (iterator == m_works.cend()) return false;
    constexpr qreal spacing = 0.0145;
    const int virtualIndex = static_cast<int>(std::distance(m_works.cbegin(), iterator));
    const quint64 request = ++m_openWorkRequest;
    m_suppressRelationGraphUntilDeselected = true;
    if (m_relationGraphTimer != nullptr) m_relationGraphTimer->stop();
    hideOverlays();
    m_cameraTimer->stop();
    m_cameraVelocityX = 0;
    m_cameraX = virtualIndex * spacing;
    m_cameraTargetX = m_cameraX;
    emit cameraXChanged();
    emit cameraTargetXChanged();
    m_loadedStart = -1;
    refreshVisibleWindow(true);
    const int delegateIndex = virtualIndex - m_visibleStart;
    QObject *root = m_quickWidget->rootObject();
    if (root == nullptr || delegateIndex < 0 || delegateIndex >= 60) return false;
    // The selected delegate freezes its content while it animates.  A route
    // jump can land on the same local delegate index after the visible window
    // moves (for example, both work 40 and 41 are index 30).  Clear the old
    // selection first and wait one event-loop turn before selecting the new
    // delegate, otherwise QML never emits selectedChanged and keeps showing
    // the old frozen DVD.
    root->setProperty("hoveredDelegateIndex", -1);
    root->setProperty("pressedDelegateIndex", -1);
    root->setProperty("fullyExpandedDelegateIndex", -1);
    root->setProperty("expandedDelegateIndex", -1);
    root->setProperty("selectedDelegateIndex", -1);
    root->setProperty("_pendingCollapseSelectedIndex", -1);
    root->setProperty("_pendingCollapseCloseSpeedMultiplier", 1.0);
    root->setProperty("_frozenSelectedDelegateIndex", -1);
    root->setProperty("_frozenSelectedVirtualIndex", -1);
    // Do not make the route transition depend solely on QML change signals:
    // when the shelf is already open, a delegate can be recycled at the same
    // local index and QML may not emit every intermediate state change.
    refresh_expanded_work_meta(virtualIndex);
    QTimer::singleShot(0, this, [this, delegateIndex, virtualIndex, request] {
        QObject *root = m_quickWidget->rootObject();
        if (request != m_openWorkRequest || root == nullptr) return;
        root->setProperty("selectedDelegateIndex", delegateIndex);
        QTimer::singleShot(550, this, [this, delegateIndex, virtualIndex, request] {
            QObject *root = m_quickWidget->rootObject();
            if (request == m_openWorkRequest && root != nullptr
                && root->property("selectedDelegateIndex").toInt() == delegateIndex)
            {
                root->setProperty("expandedDelegateIndex", delegateIndex);
                // Keep C++ state authoritative for a route-driven switch.  QML
                // refreshes this as well, but this makes the page correct even if
                // an unchanged QML property suppresses its notifier.
                refresh_expanded_work_meta(virtualIndex);
            }
        });
    });
    return true;
}

void DvdShelfView::showRelationGraph(qint64 workId)
{
    if (m_relationGraph == nullptr || m_relationGraphContainer == nullptr || workId <= 0 ||
        m_suppressRelationGraphUntilDeselected)
        return;
    if (!m_graphManager->isInitialized()) m_graphManager->scheduleInitialize();
    m_relationGraph->setEgoGraph(QStringLiteral("w%1").arg(workId), 3);
    m_relationGraphContainer->show();
    if (m_relationGraphCenterX >= 0 && m_relationGraphCenterY >= 0)
        updateRelationGraphGeometry(m_relationGraphCenterX, m_relationGraphCenterY);
    m_relationGraphContainer->raise();
}

void DvdShelfView::hideOverlays()
{
    m_overlayWorkId = 0;
    m_relationGraphCenterX = m_relationGraphCenterY = -1;
    if (m_relationGraphTimer != nullptr) m_relationGraphTimer->stop();
    if (m_relationGraphContainer != nullptr) m_relationGraphContainer->hide();
    clearFanartOverlay();
}

void DvdShelfView::set_force_view_overlay(qreal centerX, qreal centerY)
{
    m_relationGraphCenterX = centerX;
    m_relationGraphCenterY = centerY;
    if (m_relationGraphContainer != nullptr && m_relationGraphContainer->isVisible())
        updateRelationGraphGeometry(centerX, centerY);
}

void DvdShelfView::updateRelationGraphGeometry(qreal centerX, qreal centerY)
{
    if (m_relationGraphContainer == nullptr || m_quickWidget == nullptr ||
        !std::isfinite(centerX) || !std::isfinite(centerY) ||
        centerX < -90000.0 || centerY < -90000.0)
        return;

    // Keep the relation graph aligned with the Python shelf: the anchor comes
    // from the QML info panel, while its size and left offset are based on the
    // actual QQuickWidget viewport rather than the surrounding page.
    constexpr qreal heightRatio = 0.88;
    constexpr qreal widthRatio = 0.61;
    constexpr qreal shiftLeftRatio = 0.42;
    constexpr int leftMargin = 0;
    const int viewWidth = m_quickWidget->width();
    const int viewHeight = m_quickWidget->height();
    if (viewWidth < 1 || viewHeight < 1) return;

    const int graphHeight = qMin(qMax(40, qRound(viewHeight * heightRatio)),
                                 qMax(40, viewHeight - 4));
    const int graphWidth = qMin(qMax(40, qRound(viewHeight * widthRatio)),
                                qMax(40, viewWidth - leftMargin));
    const int left = qBound(leftMargin,
                            qRound(centerX - graphWidth * 0.5 - viewHeight * shiftLeftRatio),
                            qMax(leftMargin, viewWidth - graphWidth));
    const int top = qBound(0, qRound(centerY - graphHeight * 0.5),
                           qMax(0, viewHeight - graphHeight));
    m_relationGraphContainer->setGeometry(left, top, graphWidth, graphHeight);
    m_relationGraphContainer->raise();
}

void DvdShelfView::set_fanart_strip_layout(qreal left, qreal top, qreal width)
{
    m_fanartLeft = left;
    m_fanartTop = top;
    m_fanartWidth = width;
    if (m_fanartStrip == nullptr || m_fanartStrip->entries().isEmpty()) return;
    m_fanartContainer->show();
    updateFanartGeometry(left, top, width);
}

void DvdShelfView::updateFanartGeometry(qreal left, qreal top, qreal width)
{
    if (m_fanartContainer == nullptr) return;
    constexpr int stripHeight = 124;
    int stripWidth = qMax(160, qRound(width));
    int stripLeft = qBound(0, qRound(left), qMax(0, this->width() - stripWidth));
    stripWidth = qMin(stripWidth, this->width() - stripLeft);
    const int stripTop = qBound(0, qRound(top), qMax(0, height() - stripHeight));
    m_fanartContainer->setGeometry(stripLeft, stripTop, stripWidth, stripHeight);
    m_fanartContainer->raise();
}

void DvdShelfView::persistFanartPreview()
{
    if (m_overlayWorkId <= 0 || m_fanartStrip == nullptr) return;
    QString error;
    SqliteConnection connection;
    if (!connection.open(m_database.databaseName(), false, &error)) return;
    WorkRepository repository(connection.database());
    const auto work = repository.findById(m_overlayWorkId, &error);
    if (!work.has_value()) return;
    Work updated = *work;
    updated.fanartJson = FanartStripWidget::toJson(m_fanartStrip->entries());
    repository.updateDetails(updated, &error);
}

void DvdShelfView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_relationGraphContainer != nullptr && m_relationGraphContainer->isVisible())
        updateRelationGraphGeometry(m_relationGraphCenterX, m_relationGraphCenterY);
    if (m_fanartContainer != nullptr && m_fanartContainer->isVisible())
        updateFanartGeometry(m_fanartLeft, m_fanartTop, m_fanartWidth);
}

} // namespace darkeye
