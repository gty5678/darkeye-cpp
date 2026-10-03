#pragma once

#include <QString>

namespace darkeye::settings
{

/// 表示一套以程序目录为根目录的路径。
///
/// 默认构造时使用当前可执行文件目录；测试或独立工具可在构造时传入根目录。
class Paths final
{
public:
    /// 使用当前可执行文件目录作为根目录。
    Paths();
    /// 使用指定目录作为路径根目录。
    explicit Paths(QString applicationDirectory);

    /// 返回该对象使用的程序根目录。
    [[nodiscard]] const QString &applicationDirectory() const noexcept;
    /// 返回资源目录和运行数据根目录。
    [[nodiscard]] QString resourcesDirectory() const;
    [[nodiscard]] QString configDirectory() const;
    [[nodiscard]] QString dataDirectory() const;
    /// 返回固定配置文件及各类 JSON、日志文件路径。
    [[nodiscard]] QString settingsFile() const;
    [[nodiscard]] QString publicDatabase() const;
    [[nodiscard]] QString privateDatabase() const;
    [[nodiscard]] QString publicBackupDirectory() const;
    [[nodiscard]] QString workCoverDirectory() const;
    [[nodiscard]] QString fanartDirectory() const;
    [[nodiscard]] QString actressImageDirectory() const;
    [[nodiscard]] QString actorImageDirectory() const;
    [[nodiscard]] QString privateBackupDirectory() const;
    /// 返回不支持自定义位置的运行配置文件路径。
    [[nodiscard]] QString shortcutsFile() const;
    [[nodiscard]] QString crawlerNavButtonsFile() const;
    [[nodiscard]] QString actressNavButtonsFile() const;
    [[nodiscard]] QString addWorkWorkspaceLayoutFile() const;
    [[nodiscard]] QString logFile() const;

    /// 创建运行所需的数据目录；失败时将错误原因写入 errorMessage。
    bool ensureRuntimeDirectories(QString *errorMessage = nullptr) const;

private:
    /// 将相对程序根目录的路径规范化为绝对路径。
    [[nodiscard]] QString absolutePathFromApplicationDirectory(const QString &relativePath) const;
    /// 从 settings.ini 读取路径；缺失时使用相对根目录的默认路径。
    [[nodiscard]] QString configuredPath(const QString &key,
                                         const QString &defaultRelativePath) const;

    QString m_applicationDirectory;
};

} // namespace darkeye::settings
