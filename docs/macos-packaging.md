# macOS arm64 编译与打包

本文从一台 Apple Silicon Mac 的环境准备开始，说明如何编译 Darkeye、运行测试，并生成便携 ZIP 与安装 PKG。命令在终端中执行；进入源码目录后，后续命令均以仓库根目录为工作目录。

当前已提供 macOS 构建和打包实现，但尚未取得本项目在 Mac 上的完整原生验证结果。执行成功后仍需进行本文末尾的安装、启动与数据保留验收。

## 1. 确认系统和工具链

| 项目 | 要求 |
| --- | --- |
| CPU | Apple Silicon，终端和 Python 均原生运行于 arm64 |
| 应用最低运行系统 | 当前预设为 macOS 14.4 |
| 构建工具链 | Xcode 16 或更新版本，并满足该 Xcode 对构建机系统的要求 |
| Qt | 6.12.0 或更新的 Qt 6，macOS Desktop kit 必须含 arm64 |
| 构建工具 | CMake 3.25+、Ninja、Python 3.9+、Git |
| 依赖管理 | vcpkg，目标 triplet 为 arm64-osx |
| 打包工具 | ditto、codesign、lipo、pkgbuild，由系统及开发工具提供 |

先检查架构：

```sh
uname -m
python3 -c 'import platform; print(platform.machine())'
```

两条命令均应显示 `arm64`。不要使用 Rosetta 下的 x86_64 终端或 Python；本项目不生成 Intel 包。

安装 Xcode 并完成首次启动初始化。使用完整 Xcode 时，选择其工具链：

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -runFirstLaunch
xcode-select -p
xcrun clang++ --version
```

如果 Xcode 安装在别处，替换第一条命令的路径。构建机系统要求与应用最低运行系统是两回事，部署目标 14.4 不代表所有版本的 Xcode 都能安装在 14.4 上。[Qt 支持平台](https://doc.qt.io/qt-6/supported-platforms.html)

已安装 Homebrew 时，可以用它准备通用工具：

```sh
brew install cmake ninja python git pkg-config
cmake --version
ninja --version
python3 --version
```

Homebrew 安装与支持系统要求见[官方说明](https://docs.brew.sh/Installation)。若当前系统不在其支持范围内，可分别从 CMake、Python 等工具的官方渠道安装。无需通过 Homebrew 安装 Qt；以下步骤使用明确版本的 Qt kit。

## 2. 安装 Qt

通过 [Qt Online Installer](https://www.qt.io/download-qt-installer-oss) 安装 **Qt 6.12.0 的 macOS Desktop kit**，或更新的兼容 Qt 6 版本。应包含：

- Qt Core、Gui、Widgets、Network、Sql、Test 等基础模块。
- Qt Gui/Core 对应版本的私有开发头文件；项目使用 Qt 私有 API。
- Qt HttpServer、WebSockets、Svg、ShaderTools 模块及构建工具。

这些模块必须来自同一套 Qt 安装。当前项目不要求为了编译主程序而额外安装 Quick3D。若使用官方通用 macOS kit，库同时包含 Intel/arm64 切片也可以；本项目只编译 arm64 主程序。

假设安装目录为 `$HOME/Qt/6.12.0/macos`：

```sh
export Qt6_DIR="$HOME/Qt/6.12.0/macos/lib/cmake/Qt6"
test -f "$Qt6_DIR/Qt6Config.cmake"
```

最后一条命令应成功退出。安装位置可能不同，请根据实际存在的 `Qt6Config.cmake` 设置变量，不要照搬 Windows 的 Qt 路径。

## 3. 获取源码和配置 vcpkg

如果已有源码，进入现有仓库即可。新机器可以执行：

```sh
mkdir -p "$HOME/src"
cd "$HOME/src"
git clone https://github.com/gty5678/darkeye-cpp.git
cd darkeye-cpp
```

源码需要包含最新的 macOS 预设与打包脚本。已有仓库请确认自己正在使用包含这些改动的版本。

单独安装 vcpkg，不要复制 Windows 的 `vcpkg_installed`：

```sh
git clone https://github.com/microsoft/vcpkg.git "$HOME/src/vcpkg"
"$HOME/src/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/src/vcpkg"
test -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

首次配置时，CMake 根据 `vcpkg.json` 自动构建 FreeType、msdfgen 及其依赖，使用仓库固定的 baseline。无需手动执行 `vcpkg install`，也不需要用 vcpkg 再安装一套 Qt。首次构建需联网。[vcpkg 配置说明](https://learn.microsoft.com/vcpkg/get_started/get-started?pivots=shell-bash)

`export` 只影响当前终端。若需要每次打开终端自动生效，将以下两行加入自己的 `~/.zshrc`，路径按实际情况填写：

```sh
export VCPKG_ROOT="$HOME/src/vcpkg"
export Qt6_DIR="$HOME/Qt/6.12.0/macos/lib/cmake/Qt6"
```

## 4. 编译 Release 并运行测试

先确认位于包含 `CMakeLists.txt` 的项目根目录。仅编译：

```sh
python3 tools/build.py --config Release
```

脚本自动选择 `macos-arm64-release` 预设，使用 Ninja 调用 Apple Clang。构建目录为 `build/macos-arm64-release/`，主程序目标是 `Darkeye.app`。可查找构建出的应用：

```sh
find build/macos-arm64-release -name Darkeye.app -type d
```

构建目录中的应用用于开发，尚未经过完整发布依赖部署。分发与干净机器验证请使用打包产物。

编译测试并运行全部 CTest：

```sh
python3 tools/build.py --config Release --test
```

测试使用独立目录 `build/macos-arm64-release-tests/`。界面和 Metal 渲染测试需要正常图形登录会话，不能用纯 SSH/headless 环境的结果替代桌面验收。系统钥匙串测试默认会跳过需要显式启用的用例；在可使用测试钥匙串的桌面会话中，可设置 `DARKEYE_TEST_SYSTEM_KEYRING=1` 后单独验证。

查看失败测试详情：

```sh
ctest --test-dir build/macos-arm64-release-tests -C Release --output-on-failure
```

也可直接使用 CMake：

```sh
cmake --preset macos-arm64-release -DQt6_DIR="$Qt6_DIR"
cmake --build --preset macos-arm64-release
```

这里生成的是 Ninja 构建目录，不是 `.xcodeproj`；Xcode 提供编译工具链。更换 Qt 或编译器后可使用 `python3 tools/build.py --config Release --clean` 重建；此操作会清理该预设的构建目录，用户数据不应放在那里。

## 5. 生成便携版和安装版

环境配置完成后，一条命令构建并打包两种格式：

```sh
python3 tools/package.py
```

该命令默认不运行测试。希望测试通过后才打包，执行：

```sh
python3 tools/package.py --test
```

如果第 4 节的 Release 测试构建已经通过，可复用该目录：

```sh
python3 tools/package.py --skip-build --build-dir build/macos-arm64-release-tests
```

`--skip-build` 不会重新运行测试；不要用它把测试失败的构建标记为合格发布。它不能与 `--test` 或 `--clean` 同时使用。

默认输出：

```text
out/Darkeye-<版本>-macos-arm64-portable.zip
out/Darkeye-<版本>-macos-arm64-portable.zip.sha256
out/Darkeye-<版本>-macos-arm64-setup.pkg
out/Darkeye-<版本>-macos-arm64-setup.pkg.sha256
```

版本自动读取顶层 `CMakeLists.txt`。仅生成一种格式，或先预览命令：

```sh
python3 tools/package.py --format portable
python3 tools/package.py --format installer
python3 tools/package.py --dry-run
```

重复执行会替换同名暂存目录和产物。若暂存的便携目录已经含有用户数据，脚本会拒绝删除；此时可通过 `--destination out/new-release/Darkeye-<版本>-macos-arm64` 使用新位置，`<版本>` 需替换为实际版本。

## 6. 使用与校验产物

验证校验和：

```sh
shasum -a 256 -c out/*-macos-arm64-*.sha256
```

便携 ZIP 解压后为：

```text
DarkEye/
  Darkeye.app/
  portable.ini
  data/
  README.txt
```

把整个 `DarkEye` 文件夹放在可写位置，再打开 `Darkeye.app`。`portable.ini` 的 `DataDirectory=data` 以标记文件所在目录为基准，因此数据在 `.app` 外。迁移时携带整个文件夹；升级时退出程序，替换 `.app` 并保留 `portable.ini`、`data/`。不要在已签名的 `.app` 内创建数据文件。

安装版双击 `.pkg`，将应用安装到 `/Applications/Darkeye.app`。它不带便携标记，使用 Qt 的系统用户数据目录；实际位置可在软件设置的运行路径中查看。初始知识库从 `.app/Contents/Resources/avwiki` 复制到数据目录后才编辑和更新，安装资源保持只读。

可检查签名和架构；下面路径需按实际暂存目录替换：

```sh
codesign --verify --deep --strict /Applications/Darkeye.app
lipo -archs /Applications/Darkeye.app/Contents/MacOS/Darkeye
```

架构应仅显示 `arm64`。这些检查不等于 Gatekeeper 公证通过。

## 7. 正式签名与公证

默认打包使用 **ad-hoc 签名**，方便开发验证，不等于 Developer ID 签名或公证。已在钥匙串中安装相应证书时，可执行：

```sh
python3 tools/package.py \
  --mac-sign-identity "Developer ID Application: 你的开发者名称 (TEAMID)" \
  --mac-installer-identity "Developer ID Installer: 你的开发者名称 (TEAMID)"
```

脚本会先部署依赖，再签名内部代码和 `.app`，并使用 Installer 身份签名 PKG。**当前脚本不自动提交公证、不自动 stapling**；正式公开分发需完成 Apple 的公证流程，再重新生成最终分发文件的校验和。不要把本地签名校验成功当作已完成公证。[Apple 公证说明](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

## 8. 常见问题与验收

| 现象 | 处理 |
| --- | --- |
| 提示要求 arm64 host | 检查 `uname -m` 和 Python 架构，退出 Rosetta 终端 |
| 找不到 Ninja/CMake | 安装工具并确认它们位于 PATH |
| 找不到 Qt6/GuiPrivate/ShaderTools | 修正 Qt6_DIR，补齐同版本 kit 的模块和私有头文件 |
| vcpkg toolchain 不存在 | 检查 VCPKG_ROOT 指向 vcpkg 仓库根目录，执行 bootstrap |
| 更换 Qt 后链接或运行异常 | 用新的构建目录，或清理对应构建目录后重建 |
| 缺少 codesign/pkgbuild 或 SDK | 检查 Xcode 安装、初始化和 xcode-select 指向 |
| 找不到签名身份 | 检查证书及私钥已导入当前用户钥匙串；本地验证可先用默认签名 |
| 无法打开、系统提示开发者未验证 | 检查分发签名和公证状态；ad-hoc 包不是正式公证包 |
| 数据目录不可写 | 移动整个便携目录到可写位置，检查 portable.ini 和数据目录权限 |
| 测试失败 | 保留 Testing/Temporary/LastTest.log 与具体断言，修复或明确诊断后再发布 |

在未安装 Qt、vcpkg 的另一台兼容 Mac 上分别验证 ZIP 和 PKG：启动、首次建库、知识库编辑、网络访问、凭据读写、图形渲染、中文路径、退出后重启。便携目录移动后应继续读取原数据；安装升级应保留用户数据库和知识库。系统钥匙串密码不会随便携目录自动迁移到另一台机器。

通用规则见[三平台构建与发布](cross-platform-packaging.md)，Linux 流程见[Linux 编译与打包](linux-packaging.md)。
