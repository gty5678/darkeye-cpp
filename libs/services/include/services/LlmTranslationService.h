#pragma once

#include "settings/Settings.h"

#include <QObject>
#include <QString>

namespace darkeye
{

class LlmTranslationService final : public QObject
{
    Q_OBJECT

public:
    explicit LlmTranslationService(TranslationSettings settings, QObject *parent = nullptr);

    [[nodiscard]] quint64 translate(const QString &source,
                                    const QString &destination = QStringLiteral("zh-CN"));

signals:
    void translationFinished(quint64 requestId, const QString &translation,
                             const QString &errorMessage);

private:
    class Private;
    Private *d = nullptr;
};

} // namespace darkeye
