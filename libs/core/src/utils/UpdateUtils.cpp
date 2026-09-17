#include "utils/UpdateUtils.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace darkeye::utils {

std::optional<QList<int>> parseVersion(const QString &version)
{
    QString normalized = version.trimmed();
    if (normalized.isEmpty()) return std::nullopt;
    if (normalized.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) normalized.remove(0, 1);
    QList<int> parsed;
    for (const QString &part : normalized.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int value = part.trimmed().toInt(&ok);
        if (!ok) return std::nullopt;
        parsed.append(value);
    }
    return parsed.isEmpty() ? std::nullopt : std::optional<QList<int>>(parsed);
}

bool isNewerVersion(const QString &remoteVersion, const QString &localVersion)
{
    const auto remote = parseVersion(remoteVersion);
    const auto local = parseVersion(localVersion);
    if (!remote || !local) return remoteVersion.trimmed() != localVersion.trimmed();
    const qsizetype count = qMin(remote->size(), local->size());
    for (qsizetype index = 0; index < count; ++index) {
        const int remotePart = remote->at(index);
        const int localPart = local->at(index);
        if (remotePart != localPart) return remotePart > localPart;
    }
    return remote->size() > local->size();
}

UpdateCheckResult evaluateUpdateManifest(const QString &localVersion,
                                         const QByteArray &manifestJson)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(manifestJson, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {false, QStringLiteral("更新检查失败"),
                QStringLiteral("无法解析 latest.json：%1").arg(error.errorString())};
    }
    const QJsonObject root = document.object();
    const QString latest = root.value(QStringLiteral("latestVersion")).toVariant().toString().trimmed();
    const QString notes = root.value(QStringLiteral("releaseNotes")).toVariant().toString().trimmed();
    const QString packageUrl = root.value(QStringLiteral("package")).toObject()
        .value(QStringLiteral("url")).toVariant().toString().trimmed();
    if (latest.isEmpty()) {
        return {false, QStringLiteral("更新检查结果"),
                QStringLiteral("latest.json 缺少 latestVersion 字段。")};
    }
    const bool available = isNewerVersion(latest, localVersion);
    UpdateCheckResult result;
    result.success = true;
    result.title = QStringLiteral("更新检查结果");
    result.updateAvailable = available;
    result.latestVersion = latest;
    result.releaseNotes = notes;
    result.packageUrl = packageUrl;
    if (!available) {
        result.message = QStringLiteral("当前已是最新版本：%1。").arg(localVersion);
    } else {
        result.message = QStringLiteral("检测到新版本：%1 -> %2").arg(localVersion, latest);
        if (!notes.isEmpty()) result.message += QStringLiteral("\n更新内容：%1").arg(notes);
    }
    return result;
}

} // namespace darkeye::utils
