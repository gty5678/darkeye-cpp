#include "services/AvwikiUpdateService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

namespace darkeye
{
namespace
{
constexpr auto kUserAgent = "DarkEye-AVWiki-Updater/1.0";

QString updateError(const QString &detail)
{
    return QStringLiteral("更新失败：%1\n建议检查网络连通性，稍后重试。").arg(detail);
}

QString findContentRoot(const QString &extractDirectory)
{
    QDir root(extractDirectory);
    if (!root.entryList({QStringLiteral("*.md")}, QDir::Files).isEmpty()) return extractDirectory;
    const auto children = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (children.size() == 1) return children.first().absoluteFilePath();
    return extractDirectory;
}

bool containsMarkdown(const QString &directory)
{
    QDirIterator iterator(directory, {QStringLiteral("*.md")}, QDir::Files,
                          QDirIterator::Subdirectories);
    return iterator.hasNext();
}

QString sha256(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}
} // namespace

AvwikiUpdateService::AvwikiUpdateService(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply) m_reply->abort();
        if (m_extractor.state() != QProcess::NotRunning) m_extractor.kill();
    });
    connect(&m_extractor, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
                if (!m_active) return;
                m_timeout.stop();
                if (status != QProcess::NormalExit || exitCode != 0) {
                    completeFailure(updateError(QStringLiteral("无法解压知识库资源包：%1")
                                                     .arg(QString::fromLocal8Bit(m_extractor.readAllStandardError()))));
                    return;
                }
                QString error;
                if (!replaceContent(&error)) {
                    completeFailure(updateError(error));
                    return;
                }
                completeSuccess();
            });
}

void AvwikiUpdateService::update(const QUrl &manifestUrl, const QString &targetDirectory,
                                 int timeoutMilliseconds)
{
    cancel();
    m_manifestUrl = manifestUrl;
    m_targetDirectory = QDir::cleanPath(QFileInfo(targetDirectory).absoluteFilePath());
    m_timeoutMilliseconds = qMax(1, timeoutMilliseconds);
    m_version.clear();
    m_releaseNotes.clear();
    m_expectedHash.clear();
    m_packageFile.clear();
    m_extractDirectory.clear();
    m_active = true;
    if (!m_temporaryDirectory.isValid() || !m_manifestUrl.isValid()
        || m_manifestUrl.scheme().isEmpty() || m_targetDirectory.isEmpty()) {
        completeFailure(QStringLiteral("更新地址或知识库目录无效。"));
        return;
    }
    requestManifest();
}

void AvwikiUpdateService::cancel()
{
    m_active = false;
    m_timeout.stop();
    if (m_reply) m_reply->abort();
    m_reply = nullptr;
    if (m_extractor.state() != QProcess::NotRunning) m_extractor.kill();
}

void AvwikiUpdateService::requestManifest()
{
    QNetworkRequest request(m_manifestUrl);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    m_reply = m_network.get(request);
    QNetworkReply *reply = m_reply;
    m_timeout.start(m_timeoutMilliseconds);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (!m_active || reply != m_reply) { reply->deleteLater(); return; }
        m_timeout.stop(); m_reply = nullptr;
        const QByteArray payload = reply->readAll();
        const QString error = reply->errorString();
        const bool failed = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();
        if (failed) { completeFailure(updateError(error)); return; }
        const QJsonObject manifest = QJsonDocument::fromJson(payload).object();
        const QJsonObject package = manifest.value(QStringLiteral("package")).toObject();
        m_version = manifest.value(QStringLiteral("latestVersion")).toString().trimmed();
        m_releaseNotes = manifest.value(QStringLiteral("releaseNotes")).toString().trimmed();
        m_expectedHash = package.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        const QUrl packageUrl(package.value(QStringLiteral("url")).toString().trimmed());
        if (m_version.isEmpty() || m_expectedHash.isEmpty() || !packageUrl.isValid()
            || packageUrl.scheme().isEmpty()) {
            completeFailure(QStringLiteral("更新清单格式无效。")); return;
        }
        requestPackage(packageUrl);
    });
}

void AvwikiUpdateService::requestPackage(const QUrl &packageUrl)
{
    m_packageFile = QDir(m_temporaryDirectory.path()).filePath(QStringLiteral("avwiki.zip"));
    auto *file = new QFile(m_packageFile, this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file->deleteLater(); completeFailure(updateError(QStringLiteral("无法创建临时资源包。"))); return;
    }
    QNetworkRequest request(packageUrl);
    request.setRawHeader("User-Agent", kUserAgent);
    m_reply = m_network.get(request);
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::readyRead, file, [reply, file] { file->write(reply->readAll()); });
    m_timeout.start(m_timeoutMilliseconds);
    connect(reply, &QNetworkReply::finished, this, [this, reply, file] {
        if (!m_active || reply != m_reply) { file->deleteLater(); reply->deleteLater(); return; }
        m_timeout.stop(); m_reply = nullptr;
        file->write(reply->readAll()); file->close();
        const QString error = reply->errorString();
        const bool failed = reply->error() != QNetworkReply::NoError;
        reply->deleteLater(); file->deleteLater();
        if (failed) { completeFailure(updateError(error)); return; }
        if (sha256(m_packageFile) != m_expectedHash) {
            completeFailure(QStringLiteral("资源包校验失败：sha256 不匹配，已终止覆盖。")); return;
        }
        extractPackage();
    });
}

void AvwikiUpdateService::extractPackage()
{
    m_extractDirectory = QDir(m_temporaryDirectory.path()).filePath(QStringLiteral("extract"));
    if (QFileInfo::exists(m_extractDirectory) && !QDir(m_extractDirectory).removeRecursively()) {
        completeFailure(updateError(QStringLiteral("无法清理临时解压目录。"))); return;
    }
    if (!QDir().mkpath(m_extractDirectory)) {
        completeFailure(updateError(QStringLiteral("无法创建临时解压目录。"))); return;
    }
    m_extractor.setProgram(QStringLiteral("tar"));
    m_extractor.setArguments({QStringLiteral("-xf"), m_packageFile, QStringLiteral("-C"), m_extractDirectory});
    m_extractor.start();
    if (!m_extractor.waitForStarted(1000)) {
        completeFailure(updateError(QStringLiteral("无法启动资源包解压工具。"))); return;
    }
    m_timeout.start(m_timeoutMilliseconds);
}

bool AvwikiUpdateService::replaceContent(QString *errorMessage)
{
    const QString contentRoot = findContentRoot(m_extractDirectory);
    if (!containsMarkdown(contentRoot)) { *errorMessage = QStringLiteral("解压内容无 Markdown 文件，已终止覆盖。"); return false; }
    const QFileInfo targetInfo(m_targetDirectory);
    QDir parent(targetInfo.absolutePath());
    const QString token = QUuid::createUuid().toString(QUuid::Id128);
    const QString staging = parent.filePath(targetInfo.fileName() + QStringLiteral("_incoming_") + token);
    const QString backup = parent.filePath(targetInfo.fileName() + QStringLiteral("_backup_") + token);
    if (!QDir().mkpath(parent.absolutePath()) || !QDir().mkpath(staging)) {
        *errorMessage = QStringLiteral("无法创建知识库更新目录。"); return false;
    }
    QDirIterator files(contentRoot, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString source = files.next();
        const QString relative = QDir(contentRoot).relativeFilePath(source);
        const QString destination = QDir(staging).filePath(relative);
        if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(source, destination)) {
            QDir(staging).removeRecursively(); *errorMessage = QStringLiteral("无法准备知识库更新文件。"); return false;
        }
    }
    const bool hadOld = QFileInfo::exists(m_targetDirectory);
    if (hadOld && !parent.rename(targetInfo.fileName(), QFileInfo(backup).fileName())) {
        QDir(staging).removeRecursively(); *errorMessage = QStringLiteral("无法备份现有知识库。"); return false;
    }
    if (!parent.rename(QFileInfo(staging).fileName(), targetInfo.fileName())) {
        if (hadOld) parent.rename(QFileInfo(backup).fileName(), targetInfo.fileName());
        QDir(staging).removeRecursively(); *errorMessage = QStringLiteral("无法替换现有知识库。"); return false;
    }
    if (hadOld) QDir(backup).removeRecursively();
    return true;
}

void AvwikiUpdateService::completeSuccess()
{
    m_active = false;
    QString message = QStringLiteral("更新完成，版本：%1").arg(m_version);
    if (!m_releaseNotes.isEmpty()) message += QStringLiteral("\n更新说明：%1").arg(m_releaseNotes);
    emit finished({true, QStringLiteral("AVWiki 更新"), message});
}

void AvwikiUpdateService::completeFailure(const QString &message)
{
    if (!m_active) return;
    m_active = false; m_timeout.stop();
    emit finished({false, QStringLiteral("AVWiki 更新"), message});
}

} // namespace darkeye
