# 三平台构建与发布

各平台从环境准备到产物验收的独立操作文档：

- [Windows 打包与发布](windows-packaging.md)
- [macOS arm64 编译与打包](macos-packaging.md)
- [Linux x86_64 编译与打包](linux-packaging.md)

## 支持范围

- Qt **6.12.0 或更新的 Qt 6**；C++20、CMake 3.25+、Ninja、Python 3.9+。
- Windows x64：MSVC 2022；便携 `.7z` 和 Inno Setup `.exe`。
- macOS **arm64**：Xcode 16+，macOS 14.4+；便携 `.zip` 和安装 `.pkg`。不生成 Intel 版本。
- Linux x86_64：官方 Qt 6.12 二进制以 Ubuntu 24.04 / glibc 2.39 为基线；便携 `.tar.gz` 和 Debian/Ubuntu 安装 `.deb`。不宣称覆盖全部发行版。

Qt 的最低运行系统随实际使用的 Qt kit 确定。使用更新版本时需重新核对其系统要求。
本项目使用 Qt 私有 API，因此每个发布包必须携带构建时对应的 Qt 运行库，不可混装其他 Qt 版本。

参考：[Qt 支持平台](https://doc.qt.io/qt-6/supported-platforms.html)、[Qt CMake 部署](https://doc.qt.io/qt-6/qt-generate-deploy-app-script.html)。

## 目录与数据

| 内容 | Windows | macOS | Linux |
| --- | --- | --- | --- |
| 程序 | 安装目录/Darkeye.exe | Darkeye.app/Contents/MacOS/Darkeye | bin/Darkeye |
| 只读配置、知识库种子、许可证 | 程序目录 | .app/Contents/Resources | share/darkeye |
| 标准模式数据 | Qt AppLocalDataLocation | Qt AppLocalDataLocation | Qt AppLocalDataLocation |
| 便携标记 | app-版本/portable.ini | .app 外同级 portable.ini | app-版本/bin/portable.ini |
| 便携数据 | DarkEye/data | DarkEye/data | DarkEye/data |

知识库首次运行从安装资源复制到数据目录的 `avwiki/`，随后编辑和下载更新均写入此副本。已有副本不会被启动或升级覆盖，用户删除的页面也不会在下次启动重新出现。原安装目录中的知识库保留不变。

`--config-dir`、`--avwiki-dir`、`--data-dir` 仍可显式覆盖默认目录；`--avwiki-dir` 表示直接使用调用者指定的工作知识库，不自动初始化。
数据目录优先级：命令行 > `DARKEYE_DATA_DIR` > `portable.ini` > Qt 系统用户目录。便携配置损坏或数据不可写时报告错误，不静默切换数据库。

便携升级替换程序部分，保留 `data/`。macOS 用户应先把整个文件夹移到可写位置再打开 `.app`，不在只读镜像中运行。系统凭据存储中的密码仍绑定系统账号，换机器需重新输入；数据库中的外部媒体绝对路径也不会自动转换为另一个系统的路径。

Linux 保存 WebDAV 密码需要运行中的 Secret Service。知识库 ZIP 更新在 Windows/macOS 使用系统 `tar`，在 Linux 使用 `unzip`，缺少工具会明确报错。打开文件位置在 macOS 使用 `open -R`，Linux 使用系统文件管理器。

macOS/Linux 更新检查提供发布页入口，由用户选择本平台发布包；不调用 Windows 更新器，也不会为打开发布页而退出程序。Windows 保留原更新器流程，更新器本身需另外交付。

## 准备构建环境

三平台均需安装对应架构的 Qt Core/Gui（含私有头文件）、Widgets、HttpServer、WebSockets、Network、Sql、Svg、ShaderTools；测试还需要 Qt Test。配置 vcpkg，其 baseline 由 `vcpkg.json` 固定。

Windows：

```powershell
$env:VCPKG_ROOT = "C:/src/vcpkg"
$env:Qt6_DIR = "C:/Qt/6.12.0/msvc2022_64/lib/cmake/Qt6"
python tools/build.py --config Release --test
```

脚本自动导入 MSVC 环境；未设置 VCPKG_ROOT 时尝试使用 Visual Studio 自带 vcpkg。Windows 打包需 7-Zip 和 Inno Setup。

macOS（Apple Silicon）：

```sh
export VCPKG_ROOT="$HOME/src/vcpkg"
export Qt6_DIR="$HOME/Qt/6.12.0/macos/lib/cmake/Qt6"
python3 tools/build.py --config Release --test
```

预设强制 `CMAKE_OSX_ARCHITECTURES=arm64` 和部署目标 14.4。打包会用 `lipo` 再次核对主程序仅包含 arm64。需要 Xcode Command Line Tools、Ninja、`ditto`、`codesign`、`pkgbuild`。

Linux（Ubuntu 24.04）：

```sh
sudo apt-get install ninja-build pkg-config libsecret-1-dev libgl1-mesa-dev libegl1-mesa-dev libxkbcommon-dev libxcb-cursor0 xvfb unzip dpkg-dev fonts-noto-cjk
export VCPKG_ROOT="$HOME/src/vcpkg"
export Qt6_DIR="$HOME/Qt/6.12.0/gcc_64/lib/cmake/Qt6"
dbus-run-session -- xvfb-run -a python3 tools/build.py --config Release --test
```

原生界面和 GPU 测试不能全部使用 offscreen 替代，Linux 自动验证使用 Xvfb。系统钥匙串测试在非 Windows 默认跳过，只有准备好真实 keyring 后才设置 `DARKEYE_TEST_SYSTEM_KEYRING=1`。

所有平台可用 `--build-dir` 指定独立构建目录、`--clean` 重建、`--dry-run` 预览。Linux/macOS 的 CMake 预设分别为 `linux-x64-release[-tests]`、`macos-arm64-release[-tests]`；Windows 原有预设名称保持兼容。环境中的 Qt6_DIR、CMAKE_TOOLCHAIN_FILE、VCPKG_TARGET_TRIPLET 可以覆盖预设中的默认值。

## 统一打包

在各自原生系统执行：

```sh
python tools/package.py
```

或先构建并测试，再复用已完成的 Release 测试构建：

```sh
# 根据平台选择对应目录：
python tools/package.py --skip-build --build-dir build/windows-msvc-release-tests
python3 tools/package.py --skip-build --build-dir build/linux-x64-release-tests
python3 tools/package.py --skip-build --build-dir build/macos-arm64-release-tests
```

`--test` 会在打包前构建和运行全套测试，失败时停止打包。`--skip-build` 明确跳过构建和测试，适合在独立验证通过后使用。两者不能同时使用。

- `--format portable|installer|both`：默认 both。
- `--destination out/某目录/Darkeye-版本-平台-架构`：隔离发布暂存目录与产物。
- 所有清理和输出限制在项目 `out/` 内；带有用户数据的暂存目录以及指向外部的链接不会被自动删除。
- 版本号取自顶层 CMake `project(VERSION)`；每个产物旁生成 `.sha256` 文件。
- 安装资源包含项目 LICENSE、cgltf LICENSE、vcpkg 依赖 copyright，以及能从 Qt 安装目录找到的许可证。可通过 CMake `DARKEYE_QT_LICENSE_DIR` 指定 Qt 许可证目录；正式分发前应核对实际使用模块的完整第三方声明。

每个版本产生六个主文件：

```text
Darkeye-版本-windows-x64-portable.7z
Darkeye-版本-windows-x64-setup.exe
Darkeye-版本-macos-arm64-portable.zip
Darkeye-版本-macos-arm64-setup.pkg
Darkeye-版本-linux-x64-portable.tar.gz
Darkeye-版本-linux-x64-setup.deb
```

Linux `.deb` 把应用及其私有运行库安装到 `/opt/darkeye`，注册 `darkeye` 命令和桌面入口；系统依赖由 `dpkg-shlibdeps` 分析 ELF 文件产生，并补充 unzip、libsecret、字体等运行条件。它不会安装或删除用户数据。

macOS 默认进行 ad-hoc 签名，适用于构建验证，不等于面向公众的 Developer ID 签名和公证。可通过 `--mac-sign-identity`、`--mac-installer-identity` 指定已在本机钥匙串准备好的 Developer ID 身份。正式发布还需对分发包进行公证和 stapling；本入口不会自动上传或公证。参见 [Apple 分发说明](https://developer.apple.com/documentation/xcode/packaging-mac-software-for-distribution)。

## 原生验证工作流

`.github/workflows/native-release.yml` 提供手动触发的三平台原生构建、全套测试、双格式打包和校验文件上传。仅上传 Actions artifacts，不创建公开 Release。macOS 使用 Apple Silicon runner，Windows 使用 MSVC，Linux 使用 Ubuntu 24.04。

工具自身的主机无关回归测试：

```sh
python tools/test_packaging.py
```

本地 Windows 通过不能替代 Linux/macOS 原生结果；必须查看对应 runner 的成功结果和下载包的干净环境运行结果后，才能标记该平台已验证。
