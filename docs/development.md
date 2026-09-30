

## 开发环境准备
### python环境
1. 用conda的输入下面指令

```
conda create -n venv python=3.13
conda activate venv
pip install -e ".[docs]"
```

2. 复制resources/develop_resources 复制到data下面

简单来说就是运行脚本scripts/develop_pre.ps1


### C++ qt环境(如果要修改绑定项目)
安装qt6.10.3

vs2022，选择C++桌面开发

下载第三方C++包

C++ 绑定项目在 `cpp_bindings/` 下，目前有两个：

- `cpp_bindings/color_wheel`：生成 `PyColorWheel.pyd`、`colorwheellib.dll`
- `cpp_bindings/forced_direct_view`：生成 `PyForceView.pyd`、`forceviewlib.dll`

编译前需要安装/准备：

- Visual Studio 2022，安装“使用 C++ 的桌面开发”
- Qt 6.10.3，MSVC 2022 64-bit kit
- CMake、Ninja
- 对应 conda 环境中安装 PySide6、shiboken6、shiboken6-generator
- `cpp_bindings/3rdparty.zip`、`cpp_bindings/forced_direct_view.zip` 等第三方包按需要解压到对应目录

脚本里目前硬编码了 VS、Qt、conda 环境路径，例如：

- VS：`C:\Program Files\Microsoft Visual Studio\2022\Community`
- Qt：`E:\Qt\6.10.1\msvc2022_64\lib\cmake`
- conda 环境：`avlite3`
- PySide6/Shiboken6 cmake 路径：`C:/Users/yin/anaconda3/envs/avlite3/...`

如果本机路径或环境名不同，先修改对应 `build.ps1` 里的路径。

在仓库根目录执行：

```
powershell -NoProfile -ExecutionPolicy Bypass -File .\cpp_bindings\color_wheel\build.ps1
```

或：

```
powershell -NoProfile -ExecutionPolicy Bypass -File .\cpp_bindings\forced_direct_view\build.ps1
```

脚本会清理并重新创建绑定项目下的 `build/` 目录，使用 Release 配置编译，然后执行 `ninja install`。安装后 `.pyd`、`.dll` 和 `shiboken6.abi3.dll` 会复制到对应绑定目录，Python 可以直接从该目录导入绑定模块。

这部分只有在修改 C++/Shiboken 绑定时才需要编译；日常 Python/UI 开发不需要重新编译。




### 开发前准备
下载后请运行`scripts/develop_pre.ps1`

插件加载，需要手动的按照上面去浏览器临时加载选择extensions/firefox_capture里的manifest.json


## 运行
vscode解释器选择
Ctrl + Shift + P

```
Python: Select Interpreter
```
venv
python main.py

或者直接按F5

### 外部信息补充器（56790）

主程序自身会启动 `server/` 中的 FastAPI 服务，默认监听
`http://127.0.0.1:56789`。爬虫设置中的 work、actress、image 和
top-actresses API 默认指向另一个服务：`http://127.0.0.1:56790`。

`56790` 服务是外部的信息补充器（Collector/Bridge），本仓库只包含它的客户端配置、
启动和状态探测逻辑，不包含该服务的源码、Python 包或构建脚本，无法从本仓库直接启动该服务。

本地联调时，需要先单独准备并启动信息补充器，然后在“设置 → 信息补充器相关设置”中：

1. 选择信息补充器的 `.exe`，可按需启用“打开软件自动启动信息服务器”；或在外部手动启动该程序。
2. 如果服务地址不是默认的 `127.0.0.1:56790`，修改四个爬虫 API 地址。
3. 使用设置页的“测试”按钮探测 `{Bridge 根地址}/api/v1/exist`。

信息补充器未运行且没有配置其他可用服务地址时，上述四个爬虫 API 会调用失败；
这不影响主程序内部的 `56789` 服务启动。


### 代码规范

官方样式指南，约定包括：
缩进：4 个空格，不用 Tab 混用。
行长：常见约定每行 ≤79（文档/注释）或 ≤88/100（很多项目用工具放宽）。
命名：模块/包 lowercase；类 CapWords；函数/方法/变量 lower_with_underscores；常量 ALL_CAPS；私有约定 _leading_underscore。
qt信号，小驼峰

导入：标准库 → 第三方 → 本地，各组空一行；尽量不用 import *。
空格：运算符两侧、逗号后等留白习惯；不在括号里无故加空格。
字符串：与同文件已有风格一致；无特殊理由可优先双引号或统一用一种。
PEP 257
docstring 的写法和格式约定。

类型注解
PEP 484 及后续（typing / | 联合类型等），是否强制由项目决定。

使用black作为代码的风格的整理


## 打包发布
日常开发使用 `venv` 环境；正式打包请切换到专用的 `pack` 环境。`pack` 环境完全根据 `pyproject.toml` 安装依赖，只保留项目运行和打包需要的包，避免开发环境里的额外依赖污染打包结果，也能减少 Nuitka 的依赖分析范围。

在 powershell 里运行：

```
conda activate pack
python scripts/build-nuitka.py --debug
```

debug 版本用于本地验证：不启用 LTO、强制显示控制台、会生成 `report.xml`，不会生成发布压缩包。
编译后的可移动目录在 `dist/main.dist`，运行 `dist/main.dist/DarkEye.exe` 即可启动。

确认无问题后，发布版使用：

```
conda activate pack
python scripts/build-nuitka.py --release
```

release 版本会启用 LTO、隐藏控制台，编译完成后自动运行 `scripts/pack.py`，生成 `tar.zst`、`zip` 和 `update/latest.json`。

现在的打包属于激进排除，几乎把不需要的 dll 文件全删除了，所以当需要用到新的东西时很可能少 dll。需要重新修改 `scripts/build-nuitka.py` 里的打包配置。

本仓库的发布脚本只负责打包 DarkEye 主程序，不提供外部信息补充器的构建脚本；
信息补充器的构建与发布需要在对应项目中完成。





