# DarkEye 开发白皮书

## 1. 顶层设计：愿景与定位 (Vision & Philosophy)

**DarkEye (暗眼)** 是一个本地化、隐私导向的 AV 领域元数据管理与情报分析系统。我们的目标不仅仅是管理文件，而是收藏与发掘数字时代地下艺术的价值。

### 核心理念
- **Curator (策展人)**：我们不只是收集番号，我们是在收藏和策展。通过独特的视角，找出最有艺术性的作品。
- **Intelligence (情报)**：从被动的数据记录转向主动的情报挖掘。不仅记录“看了什么”，更通过数据洞察“偏好什么”，挖掘数据背后的联系（如“共演关系”、“新人出道轨迹”）。
- **Privacy First (绝对隐私)**：基于本地优先架构，数据完全私有。具备隐私保护与伪装能力，让用户拥有绝对的安全感。
- **CJK Aesthetics (东方美学)**：专为东方文字设计的竖排 UI，还原阅读实体杂志的沉浸感，带来原汁原味的文化契合度。
- **Zero-Friction (零摩擦)**：极致的自动化采集与整理，让用户感知不到繁琐的录入过程，享受“刷流媒体”般的沉浸体验。



## 2. 中层架构：功能矩阵与用户故事 (Features & User Stories)

### 2.1 沉浸式采集 (Immersive Capture)
- **浏览即收藏**：
  通过 Firefox 插件 (`extensions/firefox_capture`) 与本地服务器联动。当你在 JavLibrary 或 JavDB 浏览时，插件会自动检测并同步状态到本地“收件箱”。
  *场景*：在浏览器中看到一部感兴趣的片子，插件图标直接显示“本地未收藏”，点击一下即可推送到 DarkEye，无需复制粘贴。
- **Inbox（收件箱）工作流**：
  采集的数据进入“待处理区”，系统后台自动完成爬虫抓取，元数据清洗。用户只需在闲暇时进行“确认归档”，如同处理邮件一样高效。

### 2.2 智能化管理 (Intelligent Management)
- **拟物化收藏体验**：
  强调仪式感。有封面的作品被包装成实体光盘或杂志的样子，陈列在虚拟书架上。鼠标滑过时可以抽出预览，双击即可播放或查看详情。
- **ForceView 关系网**：
  产品的核心差异化亮点。通过动态的力导向图，展示女优、作品、导演之间的复杂关联网络。
  *场景*：点击一位女优，图谱自动展开她的“朋友圈”——共演过的演员、常合作的导演，帮助用户发现潜在感兴趣的新目标。
- **多维视图与筛选**：
  - **画廊模式**：无边界的卡片流，适合漫无目的地浏览。
  - **时间轴**：按发布时间或收藏时间回顾历史。
  - **Tag 智能筛选**：支持复杂的标签逻辑（必须包含、必须排除），快速定位特定口味的作品。

### 2.3 量化自我 (Quantified Self)
- **贤者模式报告**：
  不仅记录行为（观看、自慰、评分），更分析偏好变化。
  *场景*：系统生成月度报告：“你这个月偏爱 MILF 类型，比上个月增加了 30%；你在周五晚上的活跃度最高。”让用户通过数据重新认识自己的性癖好。

### 2.4 极致安全 (Ultimate Privacy)
- **一键伪装 (Panic Button)**：
  按下快捷键，整个界面瞬间切换为 Excel 报表或代码编辑器界面，从容应对突发查岗。
- **本地化存储**：
  所有数据（包括图片、数据库）均存储在本地 `resources/` 目录下，不上传云端，确保绝对隐私。

---

## 3. 底层实现：技术栈与规范 (Tech Stack & Standards)

### 3.1 技术架构 (Architecture)
- **UI Framework**: PySide6 (Qt for Python) - 复杂交互可用 QGraphicsView；关系图 ForceView 当前由 C++ OpenGL 实现（`cpp_bindings/forced_direct_view`），Qt 侧为 `ForceDirectedViewWidget` 容器与 `ForceViewSettingsPanel`；书架与卡片流使用 `ShelfWidget`、`ShelfVirtualizedView` 等。
- **Backend**: FastAPI (Local Server) - 作为本地微服务，处理浏览器插件请求、爬虫任务调度和后台数据处理。
- **Database**: SQLite + sqlite3 - 轻量级、单文件 (`public.db`, `private.db`)，便于备份和迁移。数据层采用 `get_connection` 获取连接，**不引入 ORM 与 Repository 中间层**；UI 直接调用 `core.database.query/insert/update/delete` 进行读写。
- **Crawler**: 模块化爬虫系统 (`core/crawler`) - 支持 JavBus, JavDB, Minnano, Fanza 等多源数据抓取与清洗。

### 3.2 目录结构说明
- `main.py`: 应用程序入口。负责：性能分析、日志、OpenGL 默认格式与预热、本地 API 启动、数据库初始化与迁移、`GraphManager` 初始化、全局 QSS、主窗口创建与显示。
- `config.py`: 路径与配置（`settings.ini`、QSettings），公共/私有数据库路径、资源路径、窗口状态等。
- `core/`: 核心业务逻辑
  - `crawler/`: 爬虫管理（CrawlerManager）、多源抓取与清洗。
  - `database/`: 连接管理（sqlite3 `get_connection`）、init/migrations、query/insert/update/delete、model。无 ORM，无 Repository 中间层。
  - `graph/`: 关系图：`graph_manager.py`（单例，维护总图、增量信号）、`graph_session.py`（会话与过滤）、`force_directed_view_widget.py`（与 C++ OpenGL 视图协作）、`graph_filter.py`、`graph.py`（图生成）、`async_image_loader.py`、`force_view_settings_panel.py`。
  - `recommendation/`: 推荐逻辑（如随机推荐）。
  - `schema/`, `cv/`, `utils/`: 数据模型、图像、日志/性能等工具。
- `controller/`: 全局信号（`GlobalSignalBus`）、快捷键（`ShortcutRegistry` / `ShortcutBindings`）、状态栏（`StatusManager`）、任务与通知（`TaskManager` / `TaskListWindow`）。
- `ui/`: 界面层
  - `main_window.py`: 主窗口、顶栏、侧栏（`Sidebar2`）、`QStackedWidget`、状态栏、与 Router 协同。
  - `navigation/router.py`: 路由单例，懒加载页面、历史栈、菜单与路由映射。
  - `pages/`: 各功能页（首页/仪表盘、管理、作品/女优/男优、统计、关系图、书架、暗黑界、设置及各类详情/编辑页）。
  - `widgets/`: 通用组件（书架 `ShelfWidget`/`ShelfVirtualizedView`、图片叠加 `ImageOverlayWidget`、封面/女优卡片、选择器等）。
  - `basic/`, `dialogs/`, `statistics/`: 基础控件、对话框、统计图表。
- `server/`: 本地 FastAPI 服务（`app.py`）、`bridge.py`（ServerBridge 单例，Qt 信号连接主线程）、launcher。
- `cpp_bindings/`: C++ 扩展
  - `forced_direct_view/`: 力导向图 OpenGL 渲染（ForceViewOpenGL、Simulation、GraphRenderer、PhysicsState）。
  - `color_wheel/`: 色轮等自定义控件。
- `extensions/`: 浏览器插件源码（如 `firefox_capture`）。
- `resources/`: 静态资源与数据存储（icons、config、sql、public/private 数据目录等）。
- `styles/`: QSS 样式（如 `main.qss`）。
- `manual_tests/`, `tests/`: 手动测试与单元测试。

### 3.2.1 当前启动与数据流
- **启动顺序**：`main.py` → 日志与性能分析 → 后台启动 FastAPI → 设置 OpenGL 格式并预热 → 数据库 init/migrations → `GraphManager.instance().initialize()`（后台建图）→ 加载 QSS → 创建 `MainWindow` → Router 延后 `push("dashboard")` 加载首页。
- **跨线程**：FastAPI 收到插件/爬虫结果后，通过 `server.bridge.bridge` 的 Qt 信号（如 `capture_received`）投递到主线程，由 `MainWindow` 等槽函数处理。
- **关系图**：`GraphManager` 维护 NetworkX 总图并发出 `graph_diff_signal`；`GraphViewSession` 按当前筛选生成子图/增量，驱动 C++ `ForceViewOpenGL` 与 `ImageOverlayWidget` 显示节点与封面。

### 3.3 开发规范 (Guidelines)
- **代码风格**：
  - 强制使用 Type Hints，减少运行时错误。
  - 核心逻辑必须编写单元测试 (`tests/` 或 `manual_tests/`)。
- **UI 设计原则**：
  - 优先使用 `QGraphicsView` 实现复杂交互。
  - 坚持 CJK 竖排美学，针对高分屏进行适配。
  - 样式与逻辑分离，统一管理颜色和字体配置。
- **数据层规范**：
  - **架构决策**：数据层采用 sqlite3 + `get_connection`，UI 直接调用 `core.database.query/insert/update/delete`，不引入 ORM 与 Repository 中间层。详见 `docs/data-layer.md`。
  - 统一连接策略：全部使用 `core.database.connection.get_connection`；写操作完成后由调用方负责 emit 对应的 `global_signals.*_changed` 信号。

### 3.4 单体 + 插件架构 (Monolith + Plugins)

采用**单体核心 + 可选插件**的架构：核心是完整可运行的单体应用，插件在此基础上提供增强能力。

#### 3.4.1 单体核心 (Core Monolith)

以下为应用必须的内核，不依赖插件即可运行：

| 模块 | 说明 |
|------|------|
| **启动与配置** | `main.py` 启动流程、`config.py` 路径与 QSettings、OpenGL 预热 |
| **数据层** | `core/database/`：连接、init/migrations、query/insert/update/delete、model（详见 `docs/data-layer.md`） |
| **基础 UI** | `main_window.py`、Router、侧栏、状态栏、基础控件（`ui/basic/`） |
| **核心页面** | 首页/仪表盘、作品管理、女优/男优列表与详情、基本设置、标签管理 |
| **控制器** | `GlobalSignalBus`、`ShortcutRegistry`、`StatusManager`、`TaskManager` |
| **本地存储** | 数据与资源均存于 `resources/`，不上传云端 |

#### 3.4.2 插件 (Plugins)

以下为可选扩展，可按需加载、卸载或由第三方提供：

| 插件 | 功能 | 对应目录/组件 |
|------|------|---------------|
| **采集插件** | Firefox 插件 + 本地 Server + Inbox 工作流、爬虫抓取 | `server/`、`extensions/`、`core/crawler/` |
| **关系图插件** | ForceView 力导向图、共演/导演关联展示 | `core/graph/`、`cpp_bindings/forced_direct_view/` |
| **推荐插件** | 随机推荐、智能推荐 | `core/recommendation/` |
| **书架插件** | 拟物化书架、卡片流、虚拟化视图 | `ShelfWidget`、`ShelfVirtualizedView` |
| **统计插件** | 贤者报告、偏好分析、热力图等 | `ui/statistics/` |
| **伪装插件** | Excel/代码编辑器一键伪装 | （Phase 3 规划） |

#### 3.4.3 插件与核心的交互

- 核心提供**事件总线**（`GlobalSignalBus`）和**数据访问接口**（`core.database.query/insert/update/delete`），插件通过订阅信号或调用接口与核心通信。
- 插件通过**注册机制**向 Router 注册页面、向侧栏注册菜单项。
- 插件独立打包，核心启动时从配置目录扫描并加载已启用的插件。

---

## 4. 路线图与里程碑 (Roadmap)

### Phase 1: 策展人更新 (The Curator Update) - *Current Focus*
- [ ] **无限卡片流首页**：当前首页为 `CoverBrowser` + 随机推荐，仪表盘为 `DashboardPage`；书架直接使用 `ShelfWidget` / `ShelfVirtualizedView`，可继续向高性能水平滚动卡片流演进。
- [ ] **CJK 竖排 UI**：在卡片详情和部分标题中实现竖排排版。
- [ ] **Inbox 模式重构**：改造手动录入流程，引入“待处理”队列；插件抓取数据经 ServerBridge 由主窗口处理。

### Phase 2: 情报网更新 (The Intelligence Update)
- [ ] **浏览器插件双向通信**：网页端实时显示本地收藏状态（已看/未看）。
- [ ] **关注订阅系统**：实现女优/片商的新作预告推送。
- [ ] **多源数据融合**：完善 ID 解析与去重逻辑，提升元数据准确性。

### Phase 3: 隐形人更新 (The Invisible Update)
- [ ] **Excel 伪装模式**：实现一键 UI 切换功能。
- [ ] **数据加密**：为私有数据库 (`private.db`) 增加加密支持。
- [ ] **贤者报告**：开发基于用户行为的数据分析与可视化模块。

### 长期目标
- **i18n 支持**：全面支持简中、繁中、日文、韩文。
- **社区生态**：开放插件市场，分享爬虫规则和分析模型。


---

## 5. 待优化与已知事项 (Backlog)

- **数据与首页**：番号补全按需加载或分页；仪表盘展示数据库规模（作品数、女优数、代表作数、近 30 天增量）；最近看过的 5～10 部、最近入库作品、最近新增女优；待处理事项汇总（如无封面作品数、未绑定女优数、无代表作女优数）；随机推荐一部；首页可配置。
- **关系图**：C++ 侧 `updateEdgeLineBuffers` 可考虑拆分以降低耦合（详见实现）。
- **架构演进**：详见 `docs/architecture-refactor-proposal.md`；下图为**拟议**单体 + 插件架构（目标态）。

```

┌─────────────────────────────────────────────────────────────────────────────┐
│  单体核心 (Monolith Core) — 完整可运行的基础应用                               │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │  Bootstrap (main.py)                                                   │  │
│  │  配置加载、DB 初始化、 migrations、事件总线、插件加载器                  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │ 数据层        │ │ 主窗口/路由   │ │ 核心页面     │ │ 控制器        │       │
│  │ DB/Repo/Model│ │ MainWindow   │ │ 作品/女优/   │ │ 信号/快捷键   │       │
│  │ migrations   │ │ Router/侧栏  │ │ 详情/设置    │ │ 状态栏/任务   │       │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘       │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │ 事件总线 / 数据接口 / 页面注册
┌─────────────────────────────────────▼───────────────────────────────────────┐
│  插件层 (Plugins) — 可选加载                                                    │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐            │
│  │ 采集插件     │ │ 关系图插件   │ │ 推荐插件     │ │ 书架插件     │            │
│  │ Server+     │ │ GraphMgr+   │ │ 随机/智能    │ │ ShelfWidget │            │
│  │ Crawler+    │ │ ForceView   │ │ 推荐        │ │ 虚拟化视图   │            │
│  │ Inbox       │ │             │ │             │ │             │            │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘            │
│  ┌─────────────┐ ┌─────────────┐                                             │
│  │ 统计插件     │ │ 伪装插件     │  (按配置启用/禁用)                          │
│  │ 贤者报告     │ │ Excel/编辑器 │                                             │
│  └─────────────┘ └─────────────┘                                             │
└──────────────────────────────────────────────────────────────────────────────┘



符合直觉简单，比如拟物化的东西，无声的推荐，在两边放相关的东西，
人分为被动搜索，与主动整理，注重形式感。
关联怎么呈现，女优的信息怎么呈现，点击女优是为了过滤，还是查看女优的相关的信息，现在已经能实现像dvd的盒子一样了，
还有很多的提示，比如本地有片时，点击后如果本地没片跳转网页，本地有片优先本地，左侧信息栏最好是3D化，就像游戏一样做进去。
dvd回去的过程也要自然
