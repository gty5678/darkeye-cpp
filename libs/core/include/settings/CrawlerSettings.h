#pragma once

#include <QString>
#include <QUrl>

namespace darkeye
{

struct CrawlerSettings final
{
    QUrl workApiBaseUrl{QStringLiteral("http://127.0.0.1:56790/api/v1/work")};
    QUrl actressApiBaseUrl{QStringLiteral("http://127.0.0.1:56790/api/v1/actress")};
    QUrl coverFetchApiUrl{QStringLiteral("http://127.0.0.1:56790/api/v1/image")};
    QUrl topActressesApiUrl{QStringLiteral("http://127.0.0.1:56790/api/v1/top-actresses")};
    QString collectorExecutable;
    bool autoStartCollector = false;
};

} // namespace darkeye
