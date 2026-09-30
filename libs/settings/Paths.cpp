#include "settings/Paths.h"

#include "settings/SettingsStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

//主要是给软件各种需要路径的地方提供路径

namespace darkeye::settings
{

Paths::Paths() : m_applicationDirectory(QCoreApplication::applicationDirPath())
{
}

Paths::Paths(QString applicationDirectory)
    : m_applicationDirectory(applicationDirectory.isEmpty()
                                 ? QCoreApplication::applicationDirPath()
                                 : QDir::cleanPath(std::move(applicationDirectory)))
{
}

const QString &Paths::applicationDirectory() const noexcept
{
    return m_applicationDirectory;
}

QString Paths::absolutePathFromApplicationDirectory(const QString &relativePath) const
{
    return QDir(m_applicationDirectory).absoluteFilePath(relativePath);
}

QString Paths::configuredPath(const QString &key, const QString &defaultRelativePath) const
{
    // 此处只负责读取配置并选择默认值，不处理相对路径的解析细节。
    const SettingsStore store(settingsFile());
    const QString configured = store.value(key).toString().trimmed();
    if (configured.isEmpty())
        return absolutePathFromApplicationDirectory(defaultRelativePath);

    // 绝对路径保持不变；相对路径交由专用函数按程序目录解析。
    if (QFileInfo(configured).isAbsolute()) return QDir::cleanPath(configured);
    return absolutePathFromApplicationDirectory(configured);
}

QString Paths::resourcesDirectory() const { return QDir(m_applicationDirectory).filePath(QStringLiteral("resources")); }
QString Paths::dataDirectory() const { return QDir(m_applicationDirectory).filePath(QStringLiteral("data")); }
QString Paths::settingsFile() const { return QDir(dataDirectory()).filePath(QStringLiteral("settings.ini")); } // 这个东西是固定位置的在 exe 目录同级的 data/settings.ini
QString Paths::publicDatabase() const { return configuredPath(QStringLiteral("Paths/Database"), QStringLiteral("data/public/public.db")); }
QString Paths::privateDatabase() const { return configuredPath(QStringLiteral("Paths/PrivateDatabase"), QStringLiteral("data/private/private.db")); }
QString Paths::publicBackupDirectory() const { return configuredPath(QStringLiteral("Paths/DatabaseBackups"), QStringLiteral("data/public/public_backup")); }
QString Paths::workCoverDirectory() const { return configuredPath(QStringLiteral("Paths/WorkCovers"), QStringLiteral("data/public/workcovers")); }
QString Paths::fanartDirectory() const { return configuredPath(QStringLiteral("Paths/Fanart"), QStringLiteral("data/public/fanart")); }
QString Paths::actressImageDirectory() const { return configuredPath(QStringLiteral("Paths/Actressimages"), QStringLiteral("data/public/actressimages")); }
QString Paths::actorImageDirectory() const { return configuredPath(QStringLiteral("Paths/Actorimages"), QStringLiteral("data/public/actorimages")); }
QString Paths::privateBackupDirectory() const { return configuredPath(QStringLiteral("Paths/PrivateDatabaseBackups"), QStringLiteral("data/private/private_backup")); }
QString Paths::shortcutsFile() const { return QDir(dataDirectory()).filePath(QStringLiteral("shortcuts.json")); }
QString Paths::crawlerNavButtonsFile() const { return QDir(dataDirectory()).filePath(QStringLiteral("crawler_nav_buttons.json")); }
QString Paths::actressNavButtonsFile() const { return QDir(dataDirectory()).filePath(QStringLiteral("actress_nav_buttons.json")); }
QString Paths::addWorkWorkspaceLayoutFile() const { return QDir(dataDirectory()).filePath(QStringLiteral("add_work_workspace_layout.json")); }
QString Paths::logFile() const { return QDir(dataDirectory()).filePath(QStringLiteral("logs/darkeye.log")); }

bool Paths::ensureRuntimeDirectories(QString *errorMessage) const
{
    // 仅创建应用托管的运行目录，不创建用户配置的外部数据路径。
    const QStringList directories = {QStringLiteral("public/public_backup"), QStringLiteral("public/workcovers"), QStringLiteral("public/fanart"), QStringLiteral("public/actressimages"), QStringLiteral("public/actorimages"), QStringLiteral("private/private_backup"), QStringLiteral("cache/images"), QStringLiteral("cache/graph"), QStringLiteral("cache/translations"), QStringLiteral("temp"), QStringLiteral("logs")};
    QDir dataRoot(dataDirectory());
    for (const QString &directory : directories)
    {
        if (dataRoot.mkpath(directory)) continue;
        // 返回首个失败目录，便于调用方将原因展示给用户或写入日志。
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("无法创建运行数据目录：%1").arg(dataRoot.filePath(directory));
        return false;
    }
    return true;
}

} // namespace darkeye::settings
