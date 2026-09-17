#include "settings/Settings.h"

#include <QVariantMap>

namespace darkeye
{
namespace
{

QString normalizedUrl(const QString &value, const QUrl &fallback)
{
    QString normalized = value.trimmed();
    while (normalized.endsWith(QChar('/')))
        normalized.chop(1);
    return normalized.isEmpty() ? fallback.toString() : normalized;
}

QString normalizedEngine(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized.isEmpty() ? QStringLiteral("google") : normalized;
}

QString normalizedFallback(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("source") ? normalized : QStringLiteral("empty");
}

QString normalizedMode(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("gpu") ? normalized : QStringLiteral("cpu");
}

int bounded(int value, int minimum, int maximum)
{
    return qBound(minimum, value, maximum);
}

QStringList readVideoPaths(const QString &value)
{
    QStringList paths;
    for (const QString &path : value.split(u',', Qt::SkipEmptyParts))
    {
        const QString trimmed = path.trimmed();
        if (!trimmed.isEmpty())
            paths.append(trimmed);
    }
    return paths;
}

QStringList normalizedVideoPaths(const QStringList &paths)
{
    QStringList normalized;
    for (const QString &path : paths)
    {
        const QString trimmed = path.trimmed();
        if (!trimmed.isEmpty() && trimmed != QStringLiteral("."))
            normalized.append(trimmed);
    }
    return normalized;
}

} // namespace

Settings::Settings(QString settingsFile) : m_store(std::move(settingsFile))
{
}

AppSettings Settings::app() const
{
    AppSettings settings;
    settings.themeId = m_store.value(QStringLiteral("App/Theme"), settings.themeId).toString();
    settings.customPrimary = m_store.value(QStringLiteral("App/CustomPrimary")).toString();
    settings.greenMode = m_store.value(QStringLiteral("App/GreenMode"), settings.greenMode).toBool();
    settings.firstLaunch =
        m_store.value(QStringLiteral("window/first_lunch"), settings.firstLaunch).toBool();
    settings.maximized =
        m_store.value(QStringLiteral("window/maximized"), settings.maximized).toBool();
    settings.windowSize =
        m_store.value(QStringLiteral("window/size"), settings.windowSize).toSize();
    settings.windowPosition =
        m_store.value(QStringLiteral("window/pos"), settings.windowPosition).toPoint();
    settings.workLargeCoverView = m_store
                                      .value(QStringLiteral("WorkPage/LargeCoverView"),
                                             settings.workLargeCoverView)
                                      .toBool();
    settings.workTagSelectorVisible = m_store
                                          .value(QStringLiteral("WorkPage/TagSelectorVisible"),
                                                 settings.workTagSelectorVisible)
                                          .toBool();
    settings.shelfTagSelectorVisible = m_store
                                           .value(QStringLiteral("ShelfPage/TagSelectorVisible"),
                                                  settings.shelfTagSelectorVisible)
                                           .toBool();
    settings.localVideoPlayer =
        m_store.value(QStringLiteral("Video/LocalPlayerExe")).toString().trimmed();
    settings.videoPaths = readVideoPaths(m_store.value(QStringLiteral("Paths/Videos")).toString());
    return settings;
}

CrawlerSettings Settings::crawler() const
{
    CrawlerSettings settings;
    settings.workApiBaseUrl = QUrl(normalizedUrl(
        m_store.value(QStringLiteral("Crawler/WorkApiBaseUrl"),
                      settings.workApiBaseUrl.toString())
            .toString(),
        settings.workApiBaseUrl));
    settings.actressApiBaseUrl = QUrl(normalizedUrl(
        m_store.value(QStringLiteral("Crawler/ActressApiBaseUrl"),
                      settings.actressApiBaseUrl.toString())
            .toString(),
        settings.actressApiBaseUrl));
    settings.coverFetchApiUrl = QUrl(normalizedUrl(
        m_store.value(QStringLiteral("Crawler/CoverFetchApiUrl"),
                      settings.coverFetchApiUrl.toString())
            .toString(),
        settings.coverFetchApiUrl));
    settings.topActressesApiUrl = QUrl(normalizedUrl(
        m_store.value(QStringLiteral("Crawler/TopActressesApiUrl"),
                      settings.topActressesApiUrl.toString())
            .toString(),
        settings.topActressesApiUrl));
    settings.collectorExecutable =
        m_store.value(QStringLiteral("Crawler/CollectorExe")).toString().trimmed();
    settings.autoStartCollector =
        m_store.value(QStringLiteral("Crawler/AutoStartCollector"),
                      settings.autoStartCollector)
            .toBool();
    return settings;
}

TranslationSettings Settings::translation() const
{
    TranslationSettings settings;
    settings.engine = normalizedEngine(
        m_store.value(QStringLiteral("Translation/Engine"), settings.engine).toString());
    settings.model = m_store.value(QStringLiteral("Translation/Model")).toString().trimmed();
    settings.baseUrl = m_store.value(QStringLiteral("Translation/BaseUrl"), settings.baseUrl)
                           .toString()
                           .trimmed();
    settings.apiKey = m_store.value(QStringLiteral("Translation/ApiKey")).toString().trimmed();
    settings.timeoutSeconds = qMax(
        1, m_store.value(QStringLiteral("Translation/TimeoutS"), settings.timeoutSeconds).toInt());
    settings.retries = qMax(
        0, m_store.value(QStringLiteral("Translation/Retries"), settings.retries).toInt());
    settings.fallback = normalizedFallback(
        m_store.value(QStringLiteral("Translation/Fallback"), settings.fallback).toString());
    settings.llama.serverExecutable =
        m_store.value(QStringLiteral("LlamaCpp/ServerExePath")).toString().trimmed();
    settings.llama.modelPath =
        m_store.value(QStringLiteral("LlamaCpp/ModelPath")).toString().trimmed();
    settings.llama.host = m_store.value(QStringLiteral("LlamaCpp/Host"), settings.llama.host)
                              .toString()
                              .trimmed();
    if (settings.llama.host.isEmpty())
        settings.llama.host = QStringLiteral("127.0.0.1");
    settings.llama.port = bounded(
        m_store.value(QStringLiteral("LlamaCpp/Port"), settings.llama.port).toInt(), 1, 65535);
    settings.llama.mode = normalizedMode(
        m_store.value(QStringLiteral("LlamaCpp/Mode"), settings.llama.mode).toString());
    settings.llama.contextSize = qMax(
        256, m_store.value(QStringLiteral("LlamaCpp/CtxSize"), settings.llama.contextSize).toInt());
    settings.llama.gpuLayers = qMax(
        0, m_store.value(QStringLiteral("LlamaCpp/GpuLayers"), settings.llama.gpuLayers).toInt());
    settings.llama.threads = qMax(
        1, m_store.value(QStringLiteral("LlamaCpp/Threads"), settings.llama.threads).toInt());
    settings.llama.threadsBatch = qMax(
        1, m_store.value(QStringLiteral("LlamaCpp/ThreadsBatch"), settings.llama.threadsBatch)
               .toInt());
    settings.llama.batchSize = qMax(
        1, m_store.value(QStringLiteral("LlamaCpp/BatchSize"), settings.llama.batchSize).toInt());
    settings.llama.microBatchSize = qMax(
        1, m_store.value(QStringLiteral("LlamaCpp/UBatchSize"), settings.llama.microBatchSize)
               .toInt());
    settings.llama.mlock =
        m_store.value(QStringLiteral("LlamaCpp/Mlock"), settings.llama.mlock).toBool();
    settings.llama.autoSyncTranslation = m_store
                                             .value(QStringLiteral("LlamaCpp/AutoSyncTranslation"),
                                                    settings.llama.autoSyncTranslation)
                                             .toBool();
    settings.llama.autoStart =
        m_store.value(QStringLiteral("LlamaCpp/AutoStart"), settings.llama.autoStart).toBool();
    return settings;
}

void Settings::saveApp(const AppSettings &settings)
{
    m_store.setValues({
        {QStringLiteral("App/Theme"), settings.themeId},
        {QStringLiteral("App/CustomPrimary"), settings.customPrimary},
        {QStringLiteral("App/GreenMode"), settings.greenMode},
        {QStringLiteral("window/first_lunch"), settings.firstLaunch},
        {QStringLiteral("window/maximized"), settings.maximized},
        {QStringLiteral("window/size"), settings.windowSize},
        {QStringLiteral("window/pos"), settings.windowPosition},
        {QStringLiteral("WorkPage/LargeCoverView"), settings.workLargeCoverView},
        {QStringLiteral("WorkPage/TagSelectorVisible"), settings.workTagSelectorVisible},
        {QStringLiteral("ShelfPage/TagSelectorVisible"), settings.shelfTagSelectorVisible},
        {QStringLiteral("Video/LocalPlayerExe"), settings.localVideoPlayer.trimmed()},
        {QStringLiteral("Paths/Videos"), normalizedVideoPaths(settings.videoPaths).join(u',')},
    });
}

void Settings::saveCrawler(const CrawlerSettings &settings)
{
    m_store.setValues({
        {QStringLiteral("Crawler/WorkApiBaseUrl"),
         normalizedUrl(settings.workApiBaseUrl.toString(), CrawlerSettings{}.workApiBaseUrl)},
        {QStringLiteral("Crawler/ActressApiBaseUrl"),
         normalizedUrl(settings.actressApiBaseUrl.toString(), CrawlerSettings{}.actressApiBaseUrl)},
        {QStringLiteral("Crawler/CoverFetchApiUrl"),
         normalizedUrl(settings.coverFetchApiUrl.toString(), CrawlerSettings{}.coverFetchApiUrl)},
        {QStringLiteral("Crawler/TopActressesApiUrl"),
         normalizedUrl(settings.topActressesApiUrl.toString(),
                       CrawlerSettings{}.topActressesApiUrl)},
        {QStringLiteral("Crawler/CollectorExe"), settings.collectorExecutable.trimmed()},
        {QStringLiteral("Crawler/AutoStartCollector"), settings.autoStartCollector},
    });
}

void Settings::saveTranslation(const TranslationSettings &settings)
{
    m_store.setValues({
        {QStringLiteral("Translation/Engine"), normalizedEngine(settings.engine)},
        {QStringLiteral("Translation/Model"), settings.model.trimmed()},
        {QStringLiteral("Translation/BaseUrl"), settings.baseUrl.trimmed()},
        {QStringLiteral("Translation/ApiKey"), settings.apiKey.trimmed()},
        {QStringLiteral("Translation/TimeoutS"), qMax(1, settings.timeoutSeconds)},
        {QStringLiteral("Translation/Retries"), qMax(0, settings.retries)},
        {QStringLiteral("Translation/Fallback"), normalizedFallback(settings.fallback)},
        {QStringLiteral("LlamaCpp/ServerExePath"), settings.llama.serverExecutable.trimmed()},
        {QStringLiteral("LlamaCpp/ModelPath"), settings.llama.modelPath.trimmed()},
        {QStringLiteral("LlamaCpp/Host"),
         settings.llama.host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1")
                                                 : settings.llama.host.trimmed()},
        {QStringLiteral("LlamaCpp/Port"), bounded(settings.llama.port, 1, 65535)},
        {QStringLiteral("LlamaCpp/Mode"), normalizedMode(settings.llama.mode)},
        {QStringLiteral("LlamaCpp/CtxSize"), qMax(256, settings.llama.contextSize)},
        {QStringLiteral("LlamaCpp/GpuLayers"), qMax(0, settings.llama.gpuLayers)},
        {QStringLiteral("LlamaCpp/Threads"), qMax(1, settings.llama.threads)},
        {QStringLiteral("LlamaCpp/ThreadsBatch"), qMax(1, settings.llama.threadsBatch)},
        {QStringLiteral("LlamaCpp/BatchSize"), qMax(1, settings.llama.batchSize)},
        {QStringLiteral("LlamaCpp/UBatchSize"), qMax(1, settings.llama.microBatchSize)},
        {QStringLiteral("LlamaCpp/Mlock"), settings.llama.mlock},
        {QStringLiteral("LlamaCpp/AutoSyncTranslation"), settings.llama.autoSyncTranslation},
        {QStringLiteral("LlamaCpp/AutoStart"), settings.llama.autoStart},
    });
}

const QString &Settings::fileName() const noexcept
{
    return m_store.fileName();
}

} // namespace darkeye
