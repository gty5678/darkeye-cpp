# C++ / Qt 开发、构建与调试

本仓库是一个 Windows 桌面 C++ / Qt 项目。主程序由 CMake 管理，使用 Qt 6、MSVC、
Ninja 和 vcpkg manifest 模式；本文档是本项目唯一的本地构建说明。

## 前置条件

在 Windows 上准备以下工具：

- Visual Studio 2022，并安装“使用 C++ 的桌面开发”工作负载（MSVC x64 工具链和 Windows SDK）。
- CMake 3.25 或更高版本、Ninja。
- Qt 6.10.3 ，并且安装的 **MSVC 2022 64-bit** kit。
- vcpkg。项目的 C++ 第三方依赖由根目录的 `vcpkg.json` 自动解析，不要手动复制第三方库目录。

在 **Developer PowerShell for VS 2022** 中设置 Qt 和 vcpkg 的位置。以下路径仅为示例，请按实际安装位置替换：

```powershell
$env:Qt6_DIR = "C:/Qt/6.10.3/msvc2022_64/lib/cmake/Qt6"
$env:VCPKG_ROOT = "C:/src/vcpkg"
```

若尚未安装 vcpkg，可执行：

```powershell
git clone https://github.com/microsoft/vcpkg C:/src/vcpkg
C:/src/vcpkg/bootstrap-vcpkg.bat
```

建议将 `Qt6_DIR` 和 `VCPKG_ROOT` 配置为用户环境变量，避免每次新开终端都要设置。Qt 必须使用
MSVC 2022 64-bit kit，并与项目的 x64 Windows 构建一致。

## vcpkg换源
首先先去安装[powershell 7.5.3](https://github.com/PowerShell/PowerShell/releases/download/v7.5.3/PowerShell-7.5.3-win-x64.msi)

Git URL重定向
```
git config --global url."https://gitee.com/mirrors/vcpkg".insteadOf "https://github.com/microsoft/vcpkg"
```

```
git config --global url."https://mirrors.tuna.tsinghua.edu.cn/git/vcpkg/".insteadOf "https://github.com/microsoft/vcpkg"
```

设置环境变量
```
$env:VCPKG_DOWNLOAD_MIRROR="https://mirrors.ustc.edu.cn/github-release/ninja-build/ninja/"
$env:X_VCPKG_ASSET_SOURCES="x-azurl,https://mirrors.ustc.edu.cn/vcpkg/assets/"
```

## CMake 预设

根目录的 `CMakePresets.json` 已定义下列预设：

| 用途 | 配置预设 | 构建预设 |
| --- | --- | --- |
| 日常 Debug 构建 | `windows-msvc-debug` | `debug` |
| Debug 构建并启用测试 | `windows-msvc-debug-tests` | `debug-tests` |
| Release（不含 `data`） | `windows-msvc-release-no-data` | `release-no-data` |
| Release（含 `data`） | `windows-msvc-release-data` | `release-data` |
| Release 构建并启用测试 | `windows-msvc-release-tests` | `release-tests` |

首次配置时，vcpkg 会下载并构建 `vcpkg.json` 中锁定的依赖，耗时会比后续配置更长。
构建目录位于 `build/windows-msvc-*`；不要把生成文件提交到 Git。

## 命令行构建与运行

在仓库根目录执行日常 Debug 构建：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset debug
```

生成的可执行文件为：

```text
build/windows-msvc-debug/apps/desktop/Darkeye.exe
```

构建完成后，CMake 会自动调用 Qt 的 `windeployqt`，因此可以直接运行该文件：

```powershell
& .\build\windows-msvc-debug\apps\desktop\Darkeye.exe
```

如需构建和运行测试，改用测试预设：

```powershell
cmake --preset windows-msvc-debug-tests
cmake --build --preset debug-tests
ctest --preset debug
```

`ctest --preset debug` 会在测试失败时输出失败信息。也可用 `ctest --test-dir build/windows-msvc-debug-tests --output-on-failure`
运行同一套测试。

## Visual Studio 2022 调试

1. 使用 **File → Open → Folder** 打开仓库根目录；Visual Studio 会读取 `CMakePresets.json`。
2. 在工具栏的配置下拉框选择 `windows-msvc-debug`；若要调试测试，选择 `windows-msvc-debug-tests`。
3. 在 CMake Targets View 中将 `Darkeye` 设为启动项，等待首次 CMake 配置和构建完成。
4. 在 C++ 源文件中设置断点，按 `F5` 开始调试；使用 `Ctrl+F5` 直接运行而不附加调试器。

若 Visual Studio 提示找不到 Qt 或 vcpkg，请在启动 Visual Studio 前设置 `Qt6_DIR`、`VCPKG_ROOT`，然后执行
**Project → Delete Cache and Reconfigure**。切换 Debug/Release 或切换依赖配置后，也应重新配置。

## VS Code 调试

安装 Microsoft 的 C/C++ 与 CMake Tools 扩展后，打开仓库根目录：

1. 在 CMake Tools 的配置预设中选择 `windows-msvc-debug`。
2. 执行 **CMake: Configure**，再执行 **CMake: Build**。
3. 选择 `Darkeye` 作为启动目标；在源文件添加断点后按 `F5`。

VS Code 必须从已设置 `Qt6_DIR`、`VCPKG_ROOT` 的 Developer PowerShell 启动，或在系统环境变量中配置它们；否则 CMake 无法定位 Qt 和 vcpkg 工具链。

## 安装目录与发布构建

Release 构建不等同于安装包。构建完成后执行 `cmake --install`，才能得到可分发目录：

```powershell
cmake --preset windows-msvc-release-data
cmake --build --preset release-data
cmake --install build/windows-msvc-release-data --config Release
```

输出目录为 `out/install/release-data`。`release-data` 会一并安装 `data/`；若发布包不需要数据，使用
`windows-msvc-release-no-data` / `release-no-data`，输出到 `out/install/release-no-data`。

安装步骤会复制程序、`resources/`、Qt 运行时和所需的 vcpkg DLL。交付前请在未安装开发环境的 Windows
机器上验证 `Darkeye.exe` 能否直接启动。

## 常见问题

### CMake 找不到 Qt6

确认 `Qt6_DIR` 指向类似
`C:/Qt/6.10.3/msvc2022_64/lib/cmake/Qt6` 的目录，而不是 Qt 安装根目录；然后删除 CMake 缓存并重新配置。

### vcpkg 工具链或依赖找不到

确认 `VCPKG_ROOT` 是 vcpkg 根目录，其中包含 `scripts/buildsystems/vcpkg.cmake`。不要用全局安装的包替代 manifest
依赖；`vcpkg.json` 的 baseline 应随依赖更新一并提交。

### 程序启动时缺少 Qt DLL 或插件

先重新执行对应的 `cmake --build --preset ...`。默认构建会在可执行文件旁运行 `windeployqt`；如果关闭了
`DARKEYE_DEPLOY_QT_RUNTIME`，需自行部署 Qt runtime，或改用 `cmake --install` 生成安装目录。
