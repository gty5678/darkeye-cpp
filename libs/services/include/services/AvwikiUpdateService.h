#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

class QNetworkReply;

namespace darkeye
{

struct AvwikiUpdateResult final
{
    bool success = false;
    QString title;
    QString message;
};

/// Downloads, validates, extracts, and atomically replaces an AVWiki package.
/// All network and process work is asynchronous, so it is safe to use from a page.
class AvwikiUpdateService final : public QObject
{
    Q_OBJECT

public:
    explicit AvwikiUpdateService(QObject *parent = nullptr);

    void update(const QUrl &manifestUrl, const QString &targetDirectory,
                int timeoutMilliseconds = 30000);
    void cancel();

signals:
    void finished(const darkeye::AvwikiUpdateResult &result);

private:
    void requestManifest();
    void requestPackage(const QUrl &packageUrl);
    void extractPackage();
    void completeSuccess();
    void completeFailure(const QString &message);
    [[nodiscard]] bool replaceContent(QString *errorMessage);

    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QProcess m_extractor;
    QTimer m_timeout;
    QTemporaryDir m_temporaryDirectory;
    QUrl m_manifestUrl;
    QString m_targetDirectory;
    QString m_version;
    QString m_releaseNotes;
    QString m_expectedHash;
    QString m_packageFile;
    QString m_extractDirectory;
    int m_timeoutMilliseconds = 30000;
    bool m_active = false;
};

} // namespace darkeye
