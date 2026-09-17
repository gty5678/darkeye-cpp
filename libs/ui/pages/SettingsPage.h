#pragma once

#include "settings/Settings.h"
#include "darkeye_ui/theme/ThemeService.h"

#include <QJsonObject>
#include <QSqlDatabase>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace darkeye
{

class ColorPicker;
class ToggleSwitch;
class TokenKeySequenceEdit;
class MultiplePathManagement;

class AboutSettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit AboutSettingsPage(ThemeService &themeService, QWidget *parent = nullptr);
};

class VideoSettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSettingsPage(Settings &settings, QSqlDatabase publicDatabase = {},
                               QWidget *parent = nullptr);

signals:
    void worksChanged();

private:
    void savePlayer();
    void savePaths();
    void browsePlayer();
    void scanMissingSerials();
    void synchronizeVideoUrls();
    [[nodiscard]] QStringList configuredPaths() const;
    void showFilesWithoutSerial(const QList<QPair<QString, QString>> &entries);

    Settings &m_settings;
    QLineEdit *m_player = nullptr;
    MultiplePathManagement *m_paths = nullptr;
    QSqlDatabase m_publicDatabase;
};

class ShortcutSettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutSettingsPage(const QString &shortcutsFile, QWidget *parent = nullptr);

private:
    void applyShortcut(const QString &actionId, TokenKeySequenceEdit *editor);
    void resetShortcut(const QString &actionId, const QString &defaultKey,
                       TokenKeySequenceEdit *editor);
    void save() const;

    QString m_shortcutsFile;
    QJsonObject m_userShortcuts;
};

class CommonSettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit CommonSettingsPage(ThemeService &themeService, Settings &settings,
                                QWidget *parent = nullptr);

    [[nodiscard]] QComboBox *themeSelector() const;

signals:
    void greenModeChanged(bool enabled);

private:
    void updatePrimaryPickerState();
    void savePrimaryColor(const QString &color);

    ThemeService &m_themeService;
    Settings &m_settings;
    QWidget *m_primaryColorRow = nullptr;
    ColorPicker *m_colorPicker = nullptr;
    QComboBox *m_themeSelector = nullptr;
    ToggleSwitch *m_greenMode = nullptr;
};

class CrawlerSettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit CrawlerSettingsPage(Settings &settings, QWidget *parent = nullptr);

private:
    void save();
    void browseCollector();

    Settings &m_settings;
    QLineEdit *m_workApi = nullptr;
    QLineEdit *m_actressApi = nullptr;
    QLineEdit *m_coverApi = nullptr;
    QLineEdit *m_topActressesApi = nullptr;
    QLineEdit *m_collectorExecutable = nullptr;
    ToggleSwitch *m_autoStartCollector = nullptr;
};

class TranslationSettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit TranslationSettingsPage(Settings &settings, QWidget *parent = nullptr);

private:
    void save();
    void updateLlmFields();
    void updateModeFields();
    void browseServerExecutable();
    void browseModel();

    Settings &m_settings;
    QComboBox *m_engine = nullptr;
    QLineEdit *m_model = nullptr;
    QLineEdit *m_baseUrl = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QComboBox *m_fallback = nullptr;
    QLineEdit *m_serverExecutable = nullptr;
    QLineEdit *m_modelPath = nullptr;
    QLineEdit *m_host = nullptr;
    QComboBox *m_mode = nullptr;
    ToggleSwitch *m_mlock = nullptr;
    ToggleSwitch *m_autoSync = nullptr;
    ToggleSwitch *m_autoStart = nullptr;
    QSpinBox *m_timeout = nullptr;
    QSpinBox *m_retries = nullptr;
    QSpinBox *m_port = nullptr;
    QSpinBox *m_contextSize = nullptr;
    QSpinBox *m_gpuLayers = nullptr;
    QSpinBox *m_threads = nullptr;
    QSpinBox *m_threadsBatch = nullptr;
    QSpinBox *m_batchSize = nullptr;
    QSpinBox *m_microBatchSize = nullptr;
};

class SettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(ThemeService &themeService, Settings &settings,
                          const QString &shortcutsFile,
                          QSqlDatabase publicDatabase = {},
                          QWidget *parent = nullptr);

    [[nodiscard]] QComboBox *themeSelector() const;

signals:
    void worksChanged();

private:
    CommonSettingsPage *m_commonPage = nullptr;
};

} // namespace darkeye
