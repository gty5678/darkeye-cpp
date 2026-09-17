#pragma once

#include "settings/AppSettings.h"
#include "settings/CrawlerSettings.h"
#include "settings/SettingsStore.h"
#include "settings/TranslationSettings.h"

namespace darkeye
{

// Typed access to the settings.ini contract shared with the Python application.
// This class only persists configuration; it never starts external programs.
class Settings final
{
public:
    explicit Settings(QString settingsFile);

    [[nodiscard]] AppSettings app() const;
    [[nodiscard]] CrawlerSettings crawler() const;
    [[nodiscard]] TranslationSettings translation() const;

    void saveApp(const AppSettings &settings);
    void saveCrawler(const CrawlerSettings &settings);
    void saveTranslation(const TranslationSettings &settings);

    [[nodiscard]] const QString &fileName() const noexcept;

private:
    SettingsStore m_store;
};

} // namespace darkeye
