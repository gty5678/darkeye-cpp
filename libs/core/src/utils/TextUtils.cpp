#include "utils/TextUtils.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace darkeye::utils {
namespace {
QJsonDocument readJson(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError && errorMessage)
        *errorMessage = error.errorString();
    return error.error == QJsonParseError::NoError ? document : QJsonDocument();
}
}

QStringList loadSensitiveWords(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return {};
    }
    QStringList words;
    while (!file.atEnd()) {
        const QString word = QString::fromUtf8(file.readLine()).trimmed();
        if (!word.isEmpty()) words.append(word);
    }
    return words;
}

QVariantMap loadTagMap(const QString &filePath, QString *errorMessage)
{
    const QJsonDocument document = readJson(filePath, errorMessage);
    return document.isObject() ? document.object().toVariantMap() : QVariantMap();
}

QSet<QString> loadExcludedGenres(const QString &filePath, QString *errorMessage)
{
    const QJsonDocument document = readJson(filePath, errorMessage);
    if (!document.isObject()) return {};
    QSet<QString> result;
    for (const QJsonValue &value : document.object().value(QStringLiteral("exclude_genre")).toArray()) {
        if (!value.isNull() && !value.isUndefined()) result.insert(value.toVariant().toString());
    }
    return result;
}

} // namespace darkeye::utils
