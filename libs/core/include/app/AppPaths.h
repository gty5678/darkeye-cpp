#pragma once

#include <QDir>
#include <QString>

namespace darkeye
{

class AppPaths final
{
public:
    explicit AppPaths(QString applicationDirectory = {});

    [[nodiscard]] const QString &applicationDirectory() const noexcept;
    [[nodiscard]] QString resourcesDirectory() const;
    [[nodiscard]] QString dataDirectory() const;
    [[nodiscard]] QString settingsFile() const;
    [[nodiscard]] QString publicDatabase() const;
    [[nodiscard]] QString privateDatabase() const;
    [[nodiscard]] QString publicBackupDirectory() const;
    [[nodiscard]] QString workCoverDirectory() const;
    [[nodiscard]] QString fanartDirectory() const;
    [[nodiscard]] QString actressImageDirectory() const;
    [[nodiscard]] QString actorImageDirectory() const;
    [[nodiscard]] QString privateBackupDirectory() const;
    [[nodiscard]] QString shortcutsFile() const;
    [[nodiscard]] QString crawlerNavButtonsFile() const;
    [[nodiscard]] QString actressNavButtonsFile() const;
    [[nodiscard]] QString addWorkWorkspaceLayoutFile() const;
    [[nodiscard]] QString logFile() const;

    bool ensureRuntimeDirectories(QString *errorMessage = nullptr) const;

private:
    [[nodiscard]] QString configuredPath(const QString &settingsKey,
                                         const QString &fallbackPath) const;
    QString m_applicationDirectory;
};

} // namespace darkeye
