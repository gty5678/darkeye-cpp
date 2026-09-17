# Darkeye C++

Darkeye 的独立 C++/Qt 迁移工程。

- 所有新实现均位于本仓库。
- Python 仓库仅作为行为和数据兼容基线，不作为构建或运行依赖。
- 用户数据默认存放在可执行文件同级的 `data/` 目录。

当前状态：阶段 0（兼容基线）、阶段 1（工程骨架）和阶段 2（数据库基础设施）
已建立；阶段 3 已完成七套主题令牌、运行时切换和第一批原生输入/按钮组件。
详细完成度以 `docs/MIGRATION_STATUS.md` 为准。

工程目录职责见 `docs/PROJECT_STRUCTURE.md`。Visual Studio 构建完成后会自动
部署 Qt 运行库，可直接启动生成的 `Darkeye.exe`。

## 本机首次构建

```powershell
.\tools\build.ps1
```

构建并运行测试：

```powershell
.\tools\build.ps1 -RunTests
```

启动 Debug 版本：

```powershell
.\tools\run.ps1
```

使用旧版数据的独立副本预览迁移效果（不会让新旧程序共写原库）：

```powershell
.\tools\prepare-preview-data.ps1 -SourceData "D:\path\to\darkeye\data"
.\tools\run.ps1 -DataDirectory ".\build\windows-msvc-debug\preview-data"
```

预览准备脚本会通过 SQLite 在线备份生成双库一致性快照，并复制封面、fanart
和人物图片。重复执行时，已有预览数据库会先归档到 `preview-backup/`。

生成不覆盖既有输出的独立发布目录：

```powershell
.\tools\package.ps1
```
