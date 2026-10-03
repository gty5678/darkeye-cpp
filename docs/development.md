# C++ / Qt 开发、构建与调试

本仓库是一个 Windows 桌面 C++ / Qt 项目。主程序由 CMake 管理，使用 Qt 6、MSVC、
Ninja 和 vcpkg manifest 模式；本文档是本项目唯一的本地构建说明。

## 前置条件

在 Windows 上准备以下工具：
- Git
- [Visual Studio 2022](https://aka.ms/vs/17/release/vs_community.exe)，
- CMake 3.25 或更高版本、Ninja。
- Qt 6.10.3 ，并且安装的 **MSVC 2022 64-bit** kit。
- vcpkg。项目的 C++ 第三方依赖由根目录的 `vcpkg.json` 自动解析，不要手动复制第三方库目录。

### 安装Git

### 安装 Visual Studio 2022
安装“使用 C++ 的桌面开发”工作负载（MSVC x64 工具链和 Windows SDK）。

### 安装 CMake 和 Ninja
这个一般随着vs C++桌面开发会同时安装上去

### 安装Qt

使用在线安装的方式，需要注册登陆账号，只安装Qt 6.10.3的MSVC依赖即可，大概5G不到

添加用户环境变量，这个很重要
```powershell
$env:Qt6_DIR = "C:/Qt/6.10.3/msvc2022_64/lib/cmake/Qt6"
```


### 安装vcpkg
实际上vcpkg会随着vs C++工作负载安装，但是最好自己装一个新的

若尚未安装 vcpkg，可执行：

```powershell
git clone https://github.com/microsoft/vcpkg C:/src/vcpkg
C:/src/vcpkg/bootstrap-vcpkg.bat
```

添加VCPKG_ROOT的环境变量，这个很重要
```
$env:VCPKG_ROOT = "C:/src/vcpkg"
```

#### vcpkg换源
首先先去安装[powershell 7.5.3](https://github.com/PowerShell/PowerShell/releases/download/v7.5.3/PowerShell-7.5.3-win-x64.msi) 这个后面编译的时候会用到，需要安装

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
如果 vcpkg 使用浅克隆，缺少锁定基线时先补取该提交（无需切换分支）：

```powershell
$VcpkgBaseline = (Get-Content vcpkg.json -Raw | ConvertFrom-Json).'builtin-baseline'
git -C $env:VCPKG_ROOT fetch --depth=1 origin $VcpkgBaseline
```

若 vcpkg 的构建工具下载失败，可在当前终端临时使用已经安装的系统工具：

```powershell
$env:VCPKG_FORCE_SYSTEM_BINARIES = "1"
python tools/package.py
```

此方式需要系统已有可用的 CMake、Ninja、Git 等工具；Windows 构建脚本会把 VS 自带工具加入环境。

构建目录位于 `build/windows-msvc-*`；不要把生成文件提交到 Git。

## 命令行构建与运行

安装 Python 3.9 或更高版本后，可使用统一构建入口（不需要额外 Python 包）：

```powershell
python tools/build.py                       # 默认 Release，不含 data
python tools/build.py --config Debug        # Debug
python tools/build.py --config Debug --test # 编译并运行测试
python tools/build.py --dry-run             # 查看将执行的命令
```

Windows 下脚本自动查找 Visual Studio、加载 x64 MSVC / Windows SDK 环境，
并使用上面的 CMake 预设。Linux / macOS 下使用 PATH 中的 CMake，构建目录为
`build/<系统>-<配置>`；需自行安装相应编译器、Qt 和依赖，项目在这些平台上的构建尚未验证。

编译 Release、安装并生成 7z（不含用户 data，需安装 7-Zip）：

```powershell
python tools/package.py
python tools/package.py --destination out/Darkeye-release
```

版本号自动读取根目录 `CMakeLists.txt` 中的 `project(Darkeye VERSION ...)`，与软件显示版本一致。
默认安装到 `out/Daryeye-<版本号>`，7z 位于同级的 `out/Daryeye-<版本号>.7z`，例如 `Daryeye-1.2.5.7z`。
每次打包前先删除对应的整个构建目录（Windows 为 `build/windows-msvc-release`），
清除 CMake 缓存和旧编译产物后重新配置、全量编译。单独运行 `build.py` 仍采用增量编译。
压缩包内保留一层安装目录名，主程序、DLL 和资源目录都位于其中。
安装阶段由 CMake 部署 Qt 运行库和第三方 DLL；指定的目标目录必须不存在，避免混入旧文件。
收集完成后、生成 7z 前，脚本会删除 `vc_redist.x64.exe` 和 `opengl32sw.dll`。
压缩后使用 `7z t` 验证完整性；Windows 自动查找常见安装目录，其他平台从 PATH 查找 `7zz` / `7z` / `7za`。
压缩和校验时显示 7-Zip 实时进度；结束时显示压缩用时和整个打包流程的总用时，失败时也会显示总用时。

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

# webdav测试

```
winget install Rclone.Rclone
```

```
mkdir D:\webdav-test
```

```
rclone serve webdav D:\webdav-test --addr 127.0.0.1:8090 --user test --pass 123456
```
