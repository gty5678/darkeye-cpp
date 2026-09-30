# DarkEye 项目架构说明

> 基于代码与现有文档整理，描述整体分层、模块职责与关键数据流。

---

## 一、项目概述

**DarkEye（暗眼）** 是一款 PC 端本地化、隐私导向的暗黑影片元数据管理与分析软件。技术要点：

- **GUI**：PySide6 (Qt6) + Qt Quick 3D（拟物 DVD 等）
- **存储**：SQLite 双库（公共库 public.db + 私有库 private.db）
- **采集**：Firefox 插件 + 本地 FastAPI 接口 + 多源爬虫
- **关系发现**：力导向图（C++ 加速，Shiboken6 绑定）

---

## 二、技术栈概览

| 类别     | 技术 |
|----------|------|
| 语言     | Python 3.13、C++17（力导向图）、JavaScript（浏览器插件） |
| GUI      | PySide6 6.10、Qt Quick 3D、OpenGL |
| 数据库   | SQLite（WAL、外键）、双库分离 |
| 本地 API | FastAPI、Uvicorn、Pydantic |
| 爬虫/解析 | asyncio、Worker 线程池、BS4、requests |
| 图/可视化 | NetworkX、C++ ForceView（Shiboken6）、QPainter 自绘 |
| 打包     | PyInstaller / Nuitka |
| 文档     | MkDocs、MkDocs Material |

---

## 三、整体架构分层

```
┌─────────────────────────────────────────────────────────────────────────┐
│  入口：main.py                                                           │
│  （日志、性能分析、Qt/OpenGL 设置、数据库初始化、样式、主窗口、延迟启动 API）  │
└─────────────────────────────────────────────────────────────────────────┘
                                      │
        ┌─────────────────────────────┼─────────────────────────────┐
        ▼                             ▼                             ▼
┌───────────────┐           ┌───────────────────┐           ┌───────────────┐
│ config.py     │           │ controller/       │           │ 资源路径       │
│ 路径/版本/    │           │ app_context.py    │           │ settings.ini  │
│ settings.ini  │           │ ThemeManager 单例 │           │ resources/    │
└───────────────┘           └───────────────────┘           └───────────────┘
        │                             │
        └─────────────────────────────┼─────────────────────────────────────┐
                                      ▼                                     │
┌─────────────────────────────────────────────────────────────────────────┐
│  UI 层：ui/                                                              │
│  MainWindow → Sidebar2 + QStackedWidget → Router（懒加载页面工厂）         │
│  pages/（HomePage, WorkPage, ActressPage, ForceDirectPage, ShelfPage…）   │
│  widgets/, basic/, dialogs/, navigation/, statistics/                     │
└─────────────────────────────────────────────────────────────────────────┘
        │
        │ 依赖
        ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  设计系统与组件库：darkeye_ui/                                            │
│  ThemeManager / tokens / 主题切换；Button、ToggleSwitch、FlowLayout 等     │
└─────────────────────────────────────────────────────────────────────────┘
        │
        │ 依赖
        ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  控制器层：controller/                                                    │
│  app_context（ThemeManager 单例）；ShortcutRegistry、ShortcutBindings；   │
│  GlobalSignalBus（数据变更信号）；MessageService 等                        │
└─────────────────────────────────────────────────────────────────────────┘
        │
        │ 依赖
        ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  核心层：core/                                                           │
│  database/（连接、init、migrations、query、insert、update、delete、备份） │
│  graph/（GraphManager、力导向视图、过滤、异步图数据加载）                   │
│  crawler/（CrawlerManager、Worker、多源爬虫、结果合并）                    │
│  dvd/（Qt Quick 3D 拟物 DVD）                                             │
│  recommendation/、utils/、schema/                                         │
└─────────────────────────────────────────────────────────────────────────┘
        │
        │ 可选 C++ 加速
        ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  C++ 绑定：cpp_bindings/                                                 │
│  forced_direct_view：力导向物理仿真 + 渲染（PyForceView）                  │
│  color_wheel：取色等                                                      │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│  服务层：server/                                                          │
│  FastAPI app（后台线程）←→ ServerBridge（Qt 信号）←→ 主线程 GUI            │
│  供 Firefox 插件、本地前端调用                                            │
└─────────────────────────────────────────────────────────────────────────┘
        ▲
        │ HTTP
        │
┌─────────────────────────────────────────────────────────────────────────┐
│  扩展：extensions/firefox_capture/                                        │
│  WebExtension：popup、content scripts、background；站点脚本 javdb/javlib 等 │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 四、模块说明

### 4.1 入口与启动（main.py）

- 设置 `QSG_RHI_BACKEND=opengl`、Qt 属性、OpenGL SurfaceFormat。
- 初始化日志（`utils.log_config`）、性能分析（`utils.profiler`）。
- 可选首次启动协议（TermsDialog）。
- **数据库**：`init_private_db` → `check_and_upgrade_private_db` → `check_and_upgrade_public_db` → `init_database`（见 [SCHEMA](SCHEMA.md)）。
- **图**：`GraphManager.instance().initialize()` 异步建图。
- **样式**：`load_app_stylesheet(app)` 注册 `ThemeManager` 到 `controller.app_context`。
- 创建并显示 `MainWindow`；`QTimer.singleShot(0, start_server)` 延迟启动 API，避免阻塞首帧。

### 4.2 配置与上下文

- **config.py**：`resource_path` 兼容打包；从 `settings.ini` 读取路径（数据库、公/私库备份、女优/男优/封面图、SQL、敏感词、快捷键、爬虫导航按钮等）；`REQUIRED_PUBLIC_DB_VERSION` / `REQUIRED_PRIVATE_DB_VERSION` 用于迁移。
- **controller/app_context.py**：全局 `ThemeManager` 的 get/set，供各页面与 darkeye_ui 组件使用。

### 4.3 核心层（core/）

| 子模块 | 职责 |
|--------|------|
| **database/** | `connection.get_connection()` 统一 SQLite 连接（WAL、外键）；`init` 建库；`migrations` 版本检测与升级、重建私有库链接、maker 前缀导入导出；`query/` 按领域拆分（work、actress、actor、tag、statistics、dashboard、private）；`insert`/`update`/`delete` 写操作，并在完成后由调用方 emit `GlobalSignalBus` 对应信号（见 [write_ops_signal_mapping](write_ops_signal_mapping.md)）；`backup_utils` 备份/还原与资源快照；`db_utils` 附件私有库、图片一致性、清理临时文件等。 |
| **graph/** | `GraphManager` 单例维护总图 G，基于元数据/标签等生成节点与边；发出 `graph_diff_signal`、`initialization_finished`；`ForceDirectedViewWidget` 等与 C++ `PyForceView` 或 Python 实现对接；`graph_filter`、`async_image_loader`、`text_parser`（wikilink 等）支撑图展示与筛选。 |
| **crawler/** | `CrawlerManager`（含 CrawlerManager2）单例：任务队列、定时调度、多源（javlib、javdb、javtxt、fanza、avdanyuwiki 等）；`Worker` 执行单次爬取；结果经 `ResultRelay`/`MergeRelay` 回主线程；与 `server.bridge.captureone_received` 连接，实现「插件推送番号 → 自动开始爬取」。 |
| **dvd/** | Qt Quick 3D 拟物 DVD 展示（QML、DvdShelfView 等）。 |
| **recommendation/** | 推荐逻辑（如随机推荐、养生模式，见 PRD）。 |
| **utils/** | 日志、性能分析、通用工具。 |
| **schema/** | 爬虫结果等数据结构（如 Pydantic/CrawledWorkData）。 |

### 4.4 UI 层（ui/）

- **main_window.py**：`QMainWindow`，侧边栏 `Sidebar2` + `QStackedWidget`，`Router` 注册路由与懒加载工厂，菜单与路由映射；快捷键通过 `ShortcutRegistry`/`setup_mainwindow_actions` 绑定；`server.bridge.capture_received` 处理插件抓取数据；延后初始化 `CrawlerManager`。
- **navigation/router.py**：`Router` 单例，`register(route_name, factory, menu_id)`，`push`/`back` 维护历史栈并切换 stack 页面；`_get_page_instance` 懒加载页面。
- **pages/**：各功能页（HomePage、WorkPage、ActressPage、ActorPage、ForceDirectPage、ShelfPage、SettingPage、ManagementPage、StatisticsPage、AvPage 等），以及单条详情/编辑（SingleWorkPage、SingleActressPage、ModifyActressPage 等）。
- **widgets/**、**basic/**、**dialogs/**：可复用控件与对话框。
- **darkeye_ui/**：独立设计系统与组件库（主题、Token、Button、ToggleSwitch、FlowLayout、WaterfallLayout 等），被主 UI 与设置页主题切换使用。

### 4.5 控制器层（controller/）

- **ShortcutRegistry / ShortcutBindings**：统一注册与绑定主窗口及全局快捷键。
- **GlobalSignalBus**：全局 Qt 信号（`work_data_changed`、`actress_data_changed`、`tag_data_changed`、撸管/做爱/晨勃/喜欢作品/喜欢女优、`status_msg_changed`、`gui_update` 等），用于写操作后刷新列表/选择器/图等（见 write_ops_signal_mapping）。

### 4.6 服务层（server/）

- **launcher.py**：在后台线程启动 Uvicorn 运行 FastAPI，不阻塞主线程。
- **bridge.py**：`ServerBridge` 单例（主线程创建），信号如 `capture_received`、`captureone_received`、`javlib_finished` 等；FastAPI 路由通过 `bridge.xxx.emit()` 与 Qt 主线程通信（必要时使用 `QueuedConnection`）。
- **app**：FastAPI 应用，提供浏览器插件与本地调用的 HTTP API（如接收番号、抓取结果等）。

### 4.7 扩展（extensions/firefox_capture）

- Firefox WebExtension：manifest、popup、content scripts、background。
- 站点脚本适配 javdb、javlibrary、fanza、minnano 等；与本地 `server` API 通信，推送番号或抓取数据。

### 4.8 C++ 绑定（cpp_bindings/）

- **forced_direct_view**：力导向图物理状态（PhysicsState）、力（CenterForce、LinkForce、ManyBodyForce）、仿真循环、`NodeLayer`（QGraphicsObject）、`ForceView`（QGraphicsView）；通过 Shiboken6 暴露为 `PyForceView`，供 Python 设置图数据并接收节点点击等信号。
- **color_wheel**：取色等控件（bindings.xml 等）。

---

## 五、数据流与通信要点

1. **插件 → 桌面端**：浏览器 content/popup → HTTP 到 FastAPI → `ServerBridge.captureone_received` / `capture_received` → 主线程 CrawlerManager / MainWindow 处理。
2. **写操作 → UI 刷新**：database 的 insert/update/delete 调用方负责 emit `global_signals` 对应信号；列表页、选择器、图等监听这些信号并刷新（见 write_ops_signal_mapping）。
3. **图数据**：`GraphManager` 维护总图，异步初始化后通过 `graph_diff_signal` 或等价机制向力导向视图推送增量；视图层可过滤、按需加载图片与文案。
4. **主题**：`ThemeManager` 在 main 中创建并 `set_theme_manager`；设置页切换主题时调用 `apply_theme(theme_id)`，重新 `set_theme(app, theme_id)` 并刷新依赖 token 的组件。

---

## 六、资源与部署

- **资源路径**：由 config 从 settings.ini 解析，支持开发/打包（`resource_path`）；主要数据在 `resources/public`（公库、封面、女优/男优图）与 `resources/private`（私库、备份）。
- **SQL**：`resources/sql/` 下建表（initTABLE.sql、initPrivateTable.sql）与迁移、ReBuild 脚本。
- **文档**：MkDocs 配置在 `mkdocs.yml`，`mkdocs serve` / `mkdocs build`；架构、PRD、SCHEMA、API、写操作信号映射等见 `docs/`。
- **打包**：PyInstaller（build-pyinstaller.ps1）或 Nuitka（build-nuitka.ps1）；需注意 OpenGL 与运行库依赖。

---

## 七、小结

| 层次 | 目录/模块 | 核心职责 |
|------|-----------|----------|
| 入口 | main.py | 环境、数据库、图、样式、主窗口、延迟 API |
| 配置 | config | 路径、版本、settings.ini |
| UI | ui/, darkeye_ui | 主窗口、路由、页面、设计系统与组件 |
| 控制 | controller | 主题上下文、快捷键、全局信号总线、MessageService |
| 核心 | core/database, graph, crawler, dvd, … | 存储、图、爬虫、拟物 DVD、推荐 |
| 服务 | server | FastAPI + Bridge，供插件与本地调用 |
| 扩展 | extensions/firefox_capture | 浏览器端采集与推送 |
| 加速 | cpp_bindings | 力导向图、取色等 C++ 实现 |

整体为**单进程桌面应用**：主线程 Qt 事件循环，数据库与图在进程内，爬虫与 API 在子线程/异步中运行，通过 Qt 信号与 Bridge 与主线程安全通信。
