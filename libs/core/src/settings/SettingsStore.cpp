#include "settings/SettingsStore.h"

#include <QSettings>

namespace darkeye
{

SettingsStore::SettingsStore(QString settingsFile) : m_fileName(std::move(settingsFile))
{
}

QVariant SettingsStore::value(const QString &key, const QVariant &defaultValue) const
{
    QSettings settings(m_fileName, QSettings::IniFormat);
    return settings.value(key, defaultValue);
}

void SettingsStore::setValue(const QString &key, const QVariant &value)
{
    QSettings settings(m_fileName, QSettings::IniFormat);
    settings.setValue(key, value);
    settings.sync();
}

void SettingsStore::setValues(const QVariantMap &values)
{
    QSettings settings(m_fileName, QSettings::IniFormat);
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        settings.setValue(it.key(), it.value());
    }
    settings.sync();
}

void SettingsStore::remove(const QString &key)
{
    QSettings settings(m_fileName, QSettings::IniFormat);
    settings.remove(key);
    settings.sync();
}

const QString &SettingsStore::fileName() const noexcept
{
    return m_fileName;
}

} // namespace darkeye
