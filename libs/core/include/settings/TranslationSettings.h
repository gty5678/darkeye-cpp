#pragma once

#include <QString>

namespace darkeye
{

struct LlamaCppSettings final
{
    QString serverExecutable;
    QString modelPath;
    QString host = QStringLiteral("127.0.0.1");
    int port = 8080;
    QString mode = QStringLiteral("cpu");
    int contextSize = 2048;
    int gpuLayers = 99;
    int threads = 6;
    int threadsBatch = 24;
    int batchSize = 512;
    int microBatchSize = 256;
    bool mlock = true;
    bool autoSyncTranslation = true;
    bool autoStart = false;
};

struct TranslationSettings final
{
    QString engine = QStringLiteral("google");
    QString model;
    QString baseUrl;
    QString apiKey;
    int timeoutSeconds = 12;
    int retries = 2;
    QString fallback = QStringLiteral("empty");
    LlamaCppSettings llama;
};

} // namespace darkeye
