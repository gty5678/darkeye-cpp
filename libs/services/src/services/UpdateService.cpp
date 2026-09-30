#include "services/UpdateService.h"

#include <QNetworkReply>
#include <QNetworkRequest>

namespace darkeye
{

namespace
{
constexpr auto kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36 DarkEye-Updater/1.0";
}

UpdateService::UpdateService(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_retry.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply) m_reply->abort();
    });
    connect(&m_retry, &QTimer::timeout, this, &UpdateService::requestManifest);
}

void UpdateService::check(const QUrl &manifestUrl, const QString &localVersion,
                          int timeoutMilliseconds, int maximumAttempts)
{
    cancel();
    m_manifestUrl = manifestUrl;
    m_localVersion = localVersion;
    m_timeoutMilliseconds = qMax(1, timeoutMilliseconds);
    m_maximumAttempts = qMax(1, maximumAttempts);
    m_attempt = 0;
    m_active = true;
    if (!m_manifestUrl.isValid() || m_manifestUrl.scheme().isEmpty()) {
        completeFailure(QStringLiteral("更新地址无效：%1").arg(m_manifestUrl.toString()));
        return;
    }
    requestManifest();
}

void UpdateService::cancel()
{
    m_active = false;
    m_timeout.stop();
    m_retry.stop();
    if (m_reply) m_reply->abort();
    m_reply = nullptr;
}

void UpdateService::requestManifest()
{
    if (!m_active) return;
    ++m_attempt;
    QNetworkRequest request(m_manifestUrl);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    m_reply = m_network.get(request);
    QNetworkReply *reply = m_reply;
    m_timeout.start(m_timeoutMilliseconds);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (!m_active || reply != m_reply) {
            reply->deleteLater();
            return;
        }
        m_timeout.stop();
        m_reply = nullptr;
        const auto error = reply->error();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const QString errorText = reply->errorString();
        reply->deleteLater();
        if (error != QNetworkReply::NoError) {
            // HTTP responses are definitive; transport failures may be transient.
            retryOrFail(errorText, status < 400 || status >= 600);
            return;
        }
        m_active = false;
        emit finished(utils::evaluateUpdateManifest(m_localVersion, payload));
    });
}

void UpdateService::retryOrFail(const QString &message, bool retryable)
{
    if (retryable && m_attempt < m_maximumAttempts) {
        m_retry.start(350 * (1 << (m_attempt - 1)));
        return;
    }
    completeFailure(message);
}

void UpdateService::completeFailure(const QString &message)
{
    if (!m_active) return;
    m_active = false;
    emit finished({false, QStringLiteral("更新检查失败"),
                   QStringLiteral("无法获取最新版本信息：%1").arg(message)});
}

} // namespace darkeye
