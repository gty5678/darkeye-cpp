#pragma once

#include <QSet>
#include <QVariantMap>

namespace darkeye::utils {

[[nodiscard]] QStringList loadSensitiveWords(const QString &filePath,
                                             QString *errorMessage = nullptr);
[[nodiscard]] QVariantMap loadTagMap(const QString &filePath,
                                     QString *errorMessage = nullptr);
[[nodiscard]] QSet<QString> loadExcludedGenres(const QString &filePath,
                                               QString *errorMessage = nullptr);

} // namespace darkeye::utils
