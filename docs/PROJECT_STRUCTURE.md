# C++ 工程目录

```text
darkeye-cpp/
├─ CMakeLists.txt                 全局选项与子项目入口
├─ CMakePresets.json             本机构建预设
├─ apps/
│  ├─ CMakeLists.txt             EXE 子项目聚合
│  ├─ desktop/                   Darkeye 桌面程序
│  ├─ component_gallery/         组件展厅
│  └─ ui_snapshot/               UI 快照工具
├─ libs/
│  ├─ CMakeLists.txt             库子项目聚合
│  ├─ core/                      路径、日志、领域和采集协议
│  ├─ database/                  SQLite、迁移和 repositories
│  ├─ services/                  跨数据库的业务服务
│  ├─ darkeye_ui/                独立基础组件、主题与通用布局
│  └─ ui/                        页面、业务控件和 MyADS
├─ 3rdparty/                     外部依赖目标入口
├─ tests/                        单元、集成和兼容测试
├─ resources/                    QRC、SQL、样式和图标
├─ cmake/                        编译、依赖和打包公共模块
└─ tools/                        构建、运行、截图和部署脚本
```

每个 `libs/<name>/` 都采用相同布局：

```text
libs/<name>/
├─ CMakeLists.txt
├─ include/                      对其他目标公开的头文件
└─ src/                          库内部实现
```

## 子项目依赖

```text
darkeye::core
├─ darkeye::database
│  └─ darkeye::services
├─ darkeye::darkeye_ui           可独立使用的基础 UI 门面
│  ├─ darkeye::theme
│  ├─ darkeye::layouts
│  └─ darkeye::widgets
└─ darkeye::ui                   Darkeye 业务 UI
   ├─ database + services
   └─ darkeye_ui + docking

darkeye::app                     完整业务依赖门面
darkeye::desktop_ui              桌面主窗口与页面装配
├─ Darkeye
├─ darkeye_component_gallery
├─ darkeye_ui_snapshot
└─ tests
```

根 `CMakeLists.txt` 不登记任何具体源码，只设置全局选项、加载公共 CMake 模块并
连接 `3rdparty`、`libs`、`apps` 和 `tests` 子项目。每个库和每个 EXE 都拥有自己的
`CMakeLists.txt`，通过 `target_link_libraries()` 和 `darkeye::<name>` 别名连接。

`Application.*` 和 `MainWindow.*` 属于桌面 EXE 的 composition root，因此位于
`apps/desktop/`；基础库不会反向依赖最终窗口。公共头文件保留 `app/...`、
`database/...`、`ui/...` 等稳定 include 路径。

Windows 上构建 `Darkeye` 后，CMake 会自动调用 Qt 部署工具，将当前配置对应的
Qt DLL 和插件复制到 `Darkeye.exe` 同目录，可直接从 Visual Studio 启动 Debug
或 Release 目标。
