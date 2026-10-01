# DarkEye C++ 架构

> 本文描述仓库当前的 C++ / Qt 实现；Python 版目录、运行时和绑定层均不属于本项目架构。

## 技术栈

| 类别 | 当前实现 |
| --- | --- |
| 语言与构建 | C++20、CMake 3.25+、Ninja、MSVC 2022、vcpkg manifest |
| GUI | Qt 6.10：Widgets、Quick/Quick3D、QML、QRhi、Qt SQL |
| 数据 | SQLite 公共库与私有库，基于 Qt SQL 访问 |
| 本地通信 | Qt HttpServer，默认仅监听 `127.0.0.1:56789` |
| 外部采集 | 可选 Collector 进程与 HTTP JSON 客户端，默认端口 56790 |
| 关系图 | 自有图存储/会话、力导向仿真、QRhi 渲染、MSDF 字体图集 |
| 发布 | CMake install、Qt 部署脚本、vcpkg 运行时依赖收集 |

## 分层与依赖

```text
apps/desktop (Darkeye.exe)
        │
        ├── libs/ui ──────────────── 页面、编辑器、对话框和导航
        ├── libs/darkeye_ui ──────── 主题、布局、通用视觉组件
        │
        ├── libs/services ────────── 采集持久化、同步、翻译、更新、视频库维护
        ├── libs/graph_view ──────── 关系图视图、仿真和 QRhi 渲染
        │        └── libs/graph ──── 图数据、仓储和图会话
        │
        ├── libs/database ────────── SQLite、仓储、备份、维护和 WebDAV
        ├── libs/settings ────────── 路径与持久化设置
        └── libs/core ────────────── 领域模型、日志、采集、HTTP API、通用工具
```

`libs/darkeye_ui` 是可复用的设计系统；`libs/ui` 负责 DarkEye 的业务页面。
`libs/graph` 不依赖渲染器，`libs/graph_view` 将图会话适配到 Qt 图形视图，便于单独测试
图数据和渲染逻辑。

## 应用启动

`apps/desktop/main.cpp` 创建 `darkeye::Application`。应用构造和运行流程如下：

1. `settings::Paths` 创建运行所需目录，`settings::ensureDefaults` 补齐默认设置。
2. `DatabaseManager` 打开并初始化公共、私有 SQLite 数据库。
3. `ThemeService` 读取并应用主题；随后创建 `MainWindow`、`LocalApiServer` 和可选的 `ManagedCollector`。
4. 接受首次使用协议后，显示主窗口并预热 `ForceViewRhiWidget` 的图形渲染器。
5. Qt 事件循环启动后，本地 API 监听 `127.0.0.1:56789`；应用按设置启动 Collector、检查更新，以及可选的本地 `llama-server`。

程序退出时会停止本地 API、Collector 和受管理的 `llama-server`，再关闭日志服务。

## 桌面界面与功能入口

`MainWindow` 使用侧栏和 `QStackedWidget` 管理页面，并按需创建具体页面。当前主入口包括：

| 入口 | 主要页面/职责 |
| --- | --- |
| 首页、仪表盘 | 首页内容和数据库概览。 |
| 作品 | `WorkPage`：作品浏览、条件检索、详情与编辑跳转。 |
| 女演员、男演员 | 列表、详情和编辑页面。 |
| 管理 | 作品录入、标签/制作商/厂牌/系列管理、批处理、汇总查询、回收站。 |
| 统计 | 私人数据记录与统计图表。 |
| 关系图 | `ForceDirectPage`：作品/人物关联浏览。 |
| 书架、剧照 | 拟物书架、作品详情和剧照浏览。 |
| 通知 | `InboxPage`：采集队列状态与处理。 |
| 设置 | 主题、视频、快捷键、采集、NFO、翻译、数据库与备份。 |

页面之间通过 Qt 信号传递“打开详情”“编辑”“数据已修改”等事件。主窗口集中处理导航、
历史记录、主题切换及跨页面刷新，页面不直接依赖其他页面的实现。

## 数据、采集与本地 API

```text
浏览器扩展 / 本机工具
          │ HTTP（127.0.0.1:56789）
          ▼
LocalApiServer ── Qt 信号 ──► MainWindow / CrawlerScheduler
                                      │
                                      ▼
                       CollectorClient（可选，127.0.0.1:56790）
                                      │ JSON
                                      ▼
                       CrawlerPersistenceService ──► SQLite 与图片资源
```

`LocalApiServer` 只转发本地采集事件，不在 HTTP 处理器内执行长时间任务。`CrawlerScheduler`
负责排队、暂停、取消、失败状态与完成回调；`CollectorClient` 通过网络请求与外部信息补充器通信。
Collector 是可选的独立程序，仓库不包含其实现或构建方式。

数据库由 `DatabaseManager` 管理公共和私有连接。领域仓储覆盖作品、人物、统计、引用和私有数据；
数据库模块同时提供事务、架构脚本、快照、维护、CSV 和 WebDAV 备份服务。运行数据路径全部由
`settings::Paths` 解析，默认相对于可执行程序目录。

## 关系图

关系图由两层组成：

- `libs/graph`：读取数据、维护图存储、仓储和视图会话，负责全图、中心节点邻域和筛选条件。
- `libs/graph_view`：力导向物理、四叉树、节点/边渲染、图片覆盖和 QRhi 小部件。

`GraphViewWidget` 接收 `GraphManager` 与 `GraphViewSession` 的数据，可展示全图、中心节点邻域、
收藏筛选或测试图。渲染层使用 `msdfgen` 与 FreeType 生成文字图集，并由 Qt 的图形运行时部署所需 DLL。

## 发布边界

CMake 将桌面程序和 `resources/` 安装到输出目录；`DARKEYE_INSTALL_DATA` 决定是否附带 `data/`。
Windows 下构建后会使用 `windeployqt` 部署开发运行所需的 Qt 文件；`cmake --install` 还会收集
msdfgen、FreeType 等运行时依赖。具体命令和预设见[开发准备](development.md)。
