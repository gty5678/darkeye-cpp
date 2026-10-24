# 主程序 QML 依赖清理

2026-10-24：主程序和常规测试不再查找或链接 Qt Qml、Quick、Quick3D、QuickWidgets。首页、书架与启动预热使用 C++ Widgets / QRhi。

- `cmake/Dependencies.cmake` 移除上述四个 Qt 组件。
- `libs/ui/CMakeLists.txt` 移除对应公共链接依赖。
- 构建后部署使用 `--no-quick-import`；安装部署移除 `--qmldir`，按可执行文件的实际依赖部署。
- 首页、书架测试使用 `QObject::inherits("QQuickWidget")` 检查子控件，不再包含 Quick 头文件，仍保留禁止旧控件回归的断言。

`resources/qml` 中的历史源文件及默认关闭的 `DARKEYE_RENDER3D_BASELINE` 对比/离线导出工具保留。只有显式开启该工具才需要 Quick 模块。资源路径 `:/qml/dvd/icons/...` 对应 SVG 图标，不需要 QML 引擎。

## 验证

- Debug 主程序、启动/主窗口/书架测试、render3d 与 DVD 场景测试及 demo 构建通过。
- 全新 `build/no-qml-configure` 配置通过；禁用四个模块的包查找后也不请求这些模块。生成的应用及测试构建图不含 QML/Quick 库或编译定义。
- 主程序安装到 `out/verification/no-qml-debug` 通过。该目录没有 `Qt6Qml*.dll`、`Qt6Quick*.dll`、`.qml` 文件或 `qml` 目录。部署报告仅包含 Core、Gui、HttpServer、Network、Sql、Svg、WebSockets、Widgets 八个 Qt 模块。
- 相关 CTest 共 15 组，13 组通过，涵盖启动接受/拒绝、主窗口、书架、场景/模型、两种缩放下的基础渲染/光照与 demo。
- DVD 的 DPR 1 和 1.5 两组失败，各有 `wheelRefreshesHoverWithoutMouseMovement`、`hoverStaysPulledOutUntilPointerLeaves` 两个悬浮断言失败。运行的是本次清理前已生成、未重编译的 DVD 测试可执行文件；本次没有改动 DVD 渲染或交互实现。日志见 `build/windows-msvc-debug-tests/tests/dvd_scene/dvd-scene-1.txt` 和 `dvd-scene-1.5.txt`，不能将扩展回归记为全部通过。

旧构建/安装目录中此前部署的 QML 文件不会由 `windeployqt` 自动删除；分发验证应使用全新安装目录。本次未替换既有发布安装目录或修改其中的用户数据。
