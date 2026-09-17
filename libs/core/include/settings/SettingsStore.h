#pragma once

#include <QString>
#include <QVariant>

namespace darkeye
{

class SettingsStore final
{
public:
    explicit SettingsStore(QString settingsFile);

    [[nodiscard]] QVariant value(const QString &key,
                                 const QVariant &defaultValue = {}) const;
    void setValue(const QString &key, const QVariant &value);
    void setValues(const QVariantMap &values);
    void remove(const QString &key);

    [[nodiscard]] const QString &fileName() const noexcept;

private:
    QString m_fileName;
};

} // namespace darkeye
