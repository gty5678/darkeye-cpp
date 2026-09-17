#include "app/AppPaths.h"

#include "settings/SettingsStore.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace darkeye
{

AppPaths::AppPaths(QString applicationDirectory)
    : m_applicationDirectory(applicationDirectory.isEmpty()
                                 ? QCoreApplication::applicationDirPath()
                                 : QDir::cleanPath(std::move(applicationDirectory)))
{
}

const QString &AppPaths::applicationDirectory() const noexcept
{
    return m_applicationDirectory;
}

QString AppPaths::resourcesDirectory() const
{
    return QDir(m_applicationDirectory).filePath(QStringLiteral("resources"));
}

QString AppPaths::dataDirectory() const
{
    const QString overridePath = qEnvironmentVariable("DARKEYE_DATA_DIR").trimmed();
    if (!overridePath.isEmpty())
    {
        return QDir::cleanPath(overridePath);
    }
    const QString previewData =
        QDir(m_applicationDirectory).filePath(QStringLiteral("preview-data"));
    if (QFileInfo::exists(QDir(previewData).filePath(QStringLiteral("public/public.db"))))
    {
        return QDir::cleanPath(previewData);
    }

    // Developer builds live below <source>/build/... while user data is kept in
    // <source>/data. Prefer that migrated data root over a stale empty database
    // created beside an earlier Debug/Release executable.
    QDir ancestor(m_applicationDirectory);
    for (int depth = 0; depth < 8; ++depth)
    {
        const bool projectRoot = QFileInfo::exists(
                                     ancestor.filePath(QStringLiteral("CMakeLists.txt")))
                                 && QFileInfo::exists(
                                     ancestor.filePath(QStringLiteral("data/README.md")));
        const QString projectData = ancestor.filePath(QStringLiteral("data"));
        if (projectRoot
            && QFileInfo::exists(
                QDir(projectData).filePath(QStringLiteral("public/public.db"))))
        {
            return QDir::cleanPath(projectData);
        }
        if (!ancestor.cdUp()) break;
    }
    return QDir(m_applicationDirectory).filePath(QStringLiteral("data"));
}

QString AppPaths::settingsFile() const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("settings.ini"));
}

QString AppPaths::publicDatabase() const
{
    return configuredPath(QStringLiteral("Paths/Database"),
                          QDir(dataDirectory()).filePath(QStringLiteral("public/public.db")));
}

QString AppPaths::privateDatabase() const
{
    return configuredPath(QStringLiteral("Paths/PrivateDatabase"),
                          QDir(dataDirectory()).filePath(QStringLiteral("private/private.db")));
}

QString AppPaths::publicBackupDirectory() const
{
    return configuredPath(QStringLiteral("Paths/DatabaseBackups"),
                          QDir(dataDirectory()).filePath(QStringLiteral("public/public_backup")));
}

QString AppPaths::workCoverDirectory() const
{
    return configuredPath(QStringLiteral("Paths/WorkCovers"),
                          QDir(dataDirectory()).filePath(QStringLiteral("public/workcovers")));
}

QString AppPaths::fanartDirectory() const
{
    return configuredPath(QStringLiteral("Paths/Fanart"),
                          QDir(dataDirectory()).filePath(QStringLiteral("public/fanart")));
}

QString AppPaths::actressImageDirectory() const
{
    return configuredPath(QStringLiteral("Paths/Actressimages"),
                          QDir(dataDirectory()).filePath(QStringLiteral("public/actressimages")));
}

QString AppPaths::actorImageDirectory() const
{
    return configuredPath(QStringLiteral("Paths/Actorimages"),
                          QDir(dataDirectory()).filePath(QStringLiteral("public/actorimages")));
}

QString AppPaths::privateBackupDirectory() const
{
    return configuredPath(QStringLiteral("Paths/PrivateDatabaseBackups"),
                          QDir(dataDirectory()).filePath(QStringLiteral("private/private_backup")));
}

QString AppPaths::shortcutsFile() const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("shortcuts.json"));
}

QString AppPaths::crawlerNavButtonsFile() const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("crawler_nav_buttons.json"));
}

QString AppPaths::actressNavButtonsFile() const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("actress_nav_buttons.json"));
}

QString AppPaths::addWorkWorkspaceLayoutFile() const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("add_work_workspace_layout.json"));
}

QString AppPaths::logFile() const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("logs/darkeye.log"));
}

bool AppPaths::ensureRuntimeDirectories(QString *errorMessage) const
{
    const QStringList relativeDirectories = {
        QStringLiteral("public/public_backup"),
        QStringLiteral("public/workcovers"),
        QStringLiteral("public/fanart"),
        QStringLiteral("public/actressimages"),
        QStringLiteral("public/actorimages"),
        QStringLiteral("private/private_backup"),
        QStringLiteral("cache/images"),
        QStringLiteral("cache/graph"),
        QStringLiteral("cache/translations"),
        QStringLiteral("temp"),
        QStringLiteral("logs"),
    };

    QDir dataRoot(dataDirectory());
    for (const QString &relativeDirectory : relativeDirectories)
    {
        if (!dataRoot.mkpath(relativeDirectory))
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("无法创建运行数据目录：%1")
                                    .arg(dataRoot.filePath(relativeDirectory));
            }
            return false;
        }
    }
    return true;
}

QString AppPaths::configuredPath(const QString &settingsKey, const QString &fallbackPath) const
{
    const SettingsStore settings(settingsFile());
    const QString configured = settings.value(settingsKey).toString().trimmed();
    if (configured.isEmpty())
    {
        return QDir::cleanPath(fallbackPath);
    }
    if (QFileInfo(configured).isAbsolute())
    {
        return QDir::cleanPath(configured);
    }
    const QString dataParent = QFileInfo(dataDirectory()).absoluteDir().absolutePath();
    return QDir::cleanPath(QDir(dataParent).filePath(configured));
}

} // namespace darkeye
