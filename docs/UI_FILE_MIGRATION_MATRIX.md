# Python UI → C++ UI 逐文件迁移矩阵

> Python 基线：`D:\00_important\10-Project\darkeye`  
> C++ 目标：`C:\Users\yin\Desktop\darkeye-cpp\libs\ui`  
> 核对日期：2026-09-15

本表按 Python 文件逐项验收。状态含义：

- **完成**：核心布局、交互、数据语义和错误路径已有 C++ 实现及验证。
- **部分**：已有可运行实现，但仍缺 Python 行为或视觉细节。
- **未开始**：C++ 尚无对应实现，或当前仍由 `PlaceholderPage` 代替。
- **合并**：Python 的薄封装或重复实现已并入一个共享 C++ 类型。
- **不迁移**：仅包导出、demo 或交互测试，不属于运行时产品功能。

## `ui/base`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `ui/base/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `BaseMoveableTableModel.py` | `components/TokenViews.*`、各 Repository | 部分 | 行移动、持久化顺序合同 |
| `BaseMoveableTableView.py` | `components/TokenViews.*` | 部分 | 拖放反馈、键盘行为 |
| `MakerComboDelegate.py` | `components/MakerSelector.*` | 部分 | 表格 delegate 编辑语义 |
| `SearchLineBase.py` | `components/CompleterLineEdit.*` | 合并 | 搜索防抖已由页面实现 |
| `SqliteEditableTableModel.py` | Repository + 管理组件 | 合并 | 各管理页分别验收编辑/回滚 |
| `SqliteQueryTableModel.py` | Repository + Qt item model | 合并 | 分页与刷新逐页验收 |
| `WorkCompletenessQueryTableModel.py` | — | 未开始 | 15 维完整度模型 |

## `ui/basic`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `ui/basic/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `EditableTableView.py` | `components/TokenViews.*` | 部分 | 新增/删除/提交快捷操作 |
| `Effect.py` | 主题 QSS / 各组件 paint | 合并 | 阴影视觉基线 |
| `HorizontalScrollArea.py` | 页面内 `QScrollArea` | 合并 | 横向滚动条样式 |
| `ModelSearch.py` | Repository 搜索接口 | 合并 | 逐模型过滤语义 |
| `MovableTableView.py` | `components/TokenViews.*` | 部分 | 行顺序保存 |
| `path/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `path/MultiplePathManagement.py` | `components/PathManagement.*` | 完成 | 已核对增删、编辑、目录选择代理、表格布局与路径读写 |
| `path/SinglePathManagement.py` | `components/PathManagement.*` | 完成 | 已核对目录选择、初始目录判断与路径读写 |
| `RotateButton.py` | `components/InteractionEffects.*` | 完成 | — |
| `ShakeButton.py` | `components/InteractionEffects.*` | 完成 | — |
| `StateToggleButton.py` | `components/StateToggleButton.*` | 完成 | — |
| `TagTypeMoveableTableView.py` | `components/TagManagementWidget.*` | 完成 | — |

## `ui/dialogs`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `ui/dialogs/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `AddActorDialog.py` | `dialogs/AddActorDialog.*` | 完成 | 已核对必填、写库、提示和外部搜索 |
| `AddActressDialog.py` | `dialogs/AddActressDialog.*` | 完成 | 已核对 trim、必填、写库、提示和外部搜索 |
| `AddMakeLoveDialog.py` | `dialogs/AddMakeLoveDialog.*` | 完成 | 已核对默认时间、评分、字段和写库 |
| `AddMasturbationDialog.py` | `dialogs/AddMasturbationDialog.*` | 完成 | 已核对番号、工具补全、评分和写库 |
| `AddQuickWork.py` | `components/WorkEditorWidget.*` | 部分 | 快捷添加独立入口、采集联动 |
| `AddSexualArousalDialog.py` | `dialogs/AddSexualArousalDialog.*` | 完成 | 已核对当天 06:00、备注和写库 |
| `open.py` | `MainWindow::registerApplicationActions` | 完成 | 私人记录与快捷作品入口均已接线；人物新增原文件不经 `open.py` |
| `TagTypeModifyDialog.py` | `components/TagManagementWidget.*` | 合并 | 已并入管理页标签类型页签 |
| `TermsDialog.py` | `dialogs/TermsDialog.*`、`apps/desktop/Application.cpp` | 完成 | 已接入首次启动同意/拒绝与持久化 |

## 主窗口、导航与 MyADS

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `ui/__init__.py` | — | 不迁移 | 仅 Python 包标记 |
| `ui/main_window.py` | `apps/desktop/MainWindow.*` | 部分 | 快捷键、帮助与首次条款已接入；资源监控、后台服务及截图动作待迁移 |
| `navigation/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `navigation/router.py` | `apps/desktop/MainWindow.*` | 部分 | 参数化路由、页面刷新钩子 |
| `myads/__init__.py` | `layouts/myads/*` | 合并 | — |
| `myads/drag_drop_overlay.py` | `layouts/myads/WorkspaceWidget.*` | 完成 | — |
| `myads/layout_tree.py` | `layouts/myads/LayoutTree.*` | 完成 | — |
| `myads/pane_widget.py` | `layouts/myads/PaneWidget.*` | 完成 | — |
| `myads/split_preview.py` | `layouts/myads/WorkspaceWidget.*` | 完成 | — |
| `myads/tab_drag_handler.py` | `layouts/myads/PaneWidget.*`、`WorkspaceWidget.*` | 完成 | — |
| `myads/theme.py` | `layouts/myads/DockTheme.*` | 完成 | — |
| `myads/workspace_manager.py` | `layouts/myads/WorkspaceWidget.*` | 完成 | — |
| `myads/styles/__init__.py` | — | 不迁移 | 仅 Python 包标记 |
| `myads/styles/myads.qss` | `layouts/myads/DockTheme.*` | 完成 | — |
| `myads/README.md` | `layouts/README.md` | 完成 | — |
| `myads/tests/*` | `tests/unit/MyAdsTest.cpp`、`tests/demos/myads_demo` | 完成 | Python demo/测试不进入产品库 |

## `ui/pages`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `ui/pages/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `ActorPage.py` | `pages/PersonPage.*` | 完成 | — |
| `ActressPage.py` | `pages/PersonPage.*` | 部分 | 女优采集入口 |
| `AvPage.py` | — | 未开始 | 当前路由为占位页 |
| `CoverBrowser.py` | — | 未开始 | 剧照浏览、翻页、删除/下载 |
| `DashboardPage.py` | `pages/DashboardPage.*` | 完成 | 精确保留 Python 当前统计区与两组占位列表；当前作为 3D 商店迁移完成前的正常业务首页 |
| `ForceDirectPage.py` | `libs/graph` + 待建页面 | 部分 | 图页面装配、数据同步、交互 |
| `HomePage.py` | — | 未开始 | Python 3D 商店尚未迁移；原 C++ 组件展厅已退出正式路由，组件展示由独立 demo 承担 |
| `InboxPage.py` | — | 未开始 | 当前路由为占位页 |
| `ManagementPage.py` | `pages/ManagementPage.*` | 部分 | 缺采集/翻译相关管理操作 |
| `ModifyActorPage.py` | `dialogs/PersonEditorDialog.*` | 完成 | — |
| `ModifyActressPage.py` | `dialogs/PersonEditorDialog.*` | 部分 | 女优采集与远程资料合并 |
| `SettingPage.py` | `pages/SettingsPage.*` | 完成 | 八段 ModernScrollMenu 结构已接入；子页状态分别见下表 |
| `ShelfPage.py` | — | 未开始 | 当前路由为占位页 |
| `SingleActressPage.py` | `pages/PersonDetailPage.*`、`components/ActressWorkTimeline.*` | 部分 | 作品时间线与人物头部已接入；页面间距仍需继续做逐像素视觉核对 |
| `SingleWorkPage.py` | `pages/WorkDetailPage.*` | 部分 | fanart 浏览、本地视频扫描 |
| `StatisticsPage.py` | `pages/StatisticsPage.*` | 部分 | 信息面板已接入；PlotTabPage 仍待逐图迁移 |
| `WorkPage.py` | `pages/WorkPage.*` | 完成 | — |
| `WorkspaceDemoPage.py` | `tests/demos/myads_demo` | 不迁移 | 产品外 demo |

## `ui/pages/management`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `AddWorkTabPage3.py` | `components/WorkEditorWidget.*`、`FanartStripWidget.*` | 部分 | 采集、多源字段选择、工作区布局恢复 |
| `LabelManagementPage.py` | `components/ReferenceManagementWidget.*` | 完成 | — |
| `ManagementTable.py` | 多个管理组件 | 合并 | — |
| `RecycleBinPage.py` | `components/WorkBatchStateWidget.*` | 完成 | — |
| `SearchTable.py` | Repository 搜索 + 管理组件 | 合并 | — |
| `SeriesManagementPage.py` | `components/ReferenceManagementWidget.*` | 完成 | — |
| `StudioManagementPage.py` | `components/ReferenceManagementWidget.*`、`MakerPrefixManagementWidget.*` | 完成 | — |
| `TagManagement.py` | `components/TagManagementWidget.*` | 完成 | — |
| `UpdateManyTabPage.py` | `components/WorkMaintenanceWidget.*` | 部分 | 批量翻译、女优补全、采集 |
| `WorkSoftDeletePage.py` | `components/WorkBatchStateWidget.*` | 完成 | — |

## `ui/pages/settings`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `settings/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `about.py` | `pages/SettingsPage.*::AboutSettingsPage`、`utils/UpdateUtils.*` | 部分 | 版本比较和 latest.json 解析已迁移；在线请求重试和本地安装包更新待服务接线 |
| `common.py` | `pages/SettingsPage.*`、`ThemeService.*` | 部分 | 主题、主色、绿色模式控件与持久化已迁移；绿色模式业务消费者待接入 |
| `crawler.py` | — | 未开始 | Collector/浏览器/接口设置 |
| `database.py` | 数据库服务 + 待建设置页 | 部分 | 路径、备份、恢复 UI |
| `nfo.py` | — | 未开始 | NFO 导入设置 |
| `shortcut.py` | `pages/SettingsPage.*` | 部分 | 8 行编辑、即时应用、JSON 持久化和单项恢复已迁移；搜索/两类截图动作本体待迁移 |
| `translation.py` | — | 未开始 | Google/LLM/llama.cpp 设置 |
| `video.py` | `pages/SettingsPage.*`、`components/PathManagement.*`、`utils/MediaUtils.*`、`services/VideoLibraryService.*` | 部分 | 播放器、多目录、缺失番号扫描及 video_url 事务覆盖已迁移；缺失番号批量采集对话框待接入 |

## `ui/pages/statistic` 与 `ui/statistics`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `statistic/PersonalDataPage.py` | `pages/PersonalDataPage.*` | 完成 | 七项概览、三档最常记录女优、去化周期和热力图已接入 |
| `statistic/PlotTabPage.py` | — | 未开始 | 全部统计图及时间范围 |
| `statistics/__init__.py` | — | 不迁移 | 仅 Python 包导出 |
| `statistics/MplCanvas.py` | `components/Charts.*` + 待扩展图表 | 部分 | 现仅热力图/雷达图 |
| `statistics/SwitchHeapMap.py` | `PersonalDataPage.*`、`components/Charts.*` | 部分 | 年份和三类记录已切换；年份按钮列暂用下拉框 |

## `ui/widgets`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `ui/widgets/__init__.py` | `components/Components.h` 等 | 合并 | — |
| `actress_nav_page.py` | — | 未开始 | 自定义女优导航 JSON |
| `ActressWorkTimeline.py` | `components/ActressWorkTimeline.*` | 已迁移 | 日期轴、同日错层、未知日期区、缩放/滚动、中键拖拽、悬浮封面卡、提示和作品跳转均已接入 |
| `crawler_nav_page.py` | — | 未开始 | 自定义采集导航 JSON |
| `CrawlerToolBox.py` | `components/CrawlerFieldSelector.*` | 部分 | 工具箱整体和采集连接 |
| `SingleActressInfo.py` | `components/PersonInfoPanel.*`、`components/ClickableLabel.*` | 部分 | Python 同款姓名行、头像资料区、日期格式和五维身材雷达图已接入；边框与细部间距继续核对 |
| `StatsOverviewCards.py` | `components/StatsOverviewCards.*` | 完成 | 七项查询与刷新已接入 |
| `work_completeness_leds.py` | `components/WorkCompletenessIndicators.*` | 完成 | 15 维灯条、bit delegate、提示与未知状态均已迁移 |
| `work_summary_edit_delegate.py` | `components/WorkEditorWidget.*` | 部分 | 表格内摘要编辑 delegate |

## `ui/widgets/image`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `image/__init__.py` | — | 不迁移 | 仅 Python 包标记 |
| `ActorAvatar.py` | `components/Avatar.*` | 完成 | — |
| `ActorCard.py` | `components/PersonCard.*` | 完成 | — |
| `ActressAvatar.py` | `components/Avatar.*` | 完成 | — |
| `ActressAvatarDropWidget.py` | `components/ImageDropWidget.*` | 完成 | — |
| `ActressCard.py` | `components/PersonCard.*` | 完成 | — |
| `CoverCard.py` | `components/WorkCard.*` | 完成 | — |
| `CoverCard2.py` | `components/WorkCard.*` | 完成 | — |
| `CoverDropWidget.py` | `components/ImageDropWidget.*` | 完成 | — |
| `CoverImage.py` | `components/AsyncImageLabel.*` | 完成 | — |
| `FanartStripWidget.py` | `components/FanartStripWidget.*` | 完成 | — |

## `ui/widgets/selectors`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `selectors/__init__.py` | — | 不迁移 | 仅 Python 包标记 |
| `ActorSelector.py` | `components/IdCheckList.*` | 完成 | — |
| `ActressSelector.py` | `components/IdCheckList.*` | 完成 | — |
| `label_selector.py` | `components/MakerSelector.*` 共享实现 | 完成 | 命名待拆分以提升可读性 |
| `maker_selector.py` | `components/MakerSelector.*` | 完成 | — |
| `series_selector.py` | `components/MakerSelector.*` 共享实现 | 完成 | 命名待拆分以提升可读性 |
| `TagSelector5.py` | `components/WorkTagSelector.*` | 完成 | — |

## `ui/widgets/text`

| Python 文件 | C++ 落点 | 状态 | 剩余核对 |
|---|---|---|---|
| `text/__init__.py` | — | 不迁移 | 仅 Python 包标记 |
| `ClickableLabel.py` | `components/ClickableLabel.*` | 完成 | 复制、Toast、固定尺寸及右键跳转信号已覆盖 |
| `VerticalTagLabel2.py` | `components/VerticalText.*` | 完成 | — |
| `WikiHighlighter.py` | `components/WikiHighlighter.*` | 完成 | Wiki 链接、三级标题、粗体和斜体格式已覆盖 |
| `WikiTextEdit.py` | `components/WikiTextEdit.*` | 完成 | 链接识别、包含式补全、异步词库、封面预览及参数化跳转信号已覆盖 |
| `test_WikiTextEdit_interactive.py` | `tests/unit/TextWidgetTest.cpp` | 完成 | 改用离屏自动化兼容测试验收 |

## `darkeye_ui` 公共库说明

Python `darkeye_ui/` 的运行时公共导出已独立迁入 `libs/darkeye_ui/`：

- `libs/darkeye_ui/components`：按钮、输入、通知、图表、图片及 token 控件。
- `libs/darkeye_ui/layouts`：Flow、VerticalFlow、VerticalText、Waterfall。
- `libs/darkeye_ui/theme`：主题令牌、主题切换、图标。
- `libs/darkeye_ui/base`：首次显示时初始化的 `LazyWidget`。

`libs/ui` 仅保留页面、对话框、MyADS，以及作品、人物、标签、路径、采集、
统计等 Darkeye 业务组件。基础库通过 `darkeye::darkeye_ui` 暴露，业务 UI 通过
`darkeye::ui` 暴露；独立边界测试只链接前者，防止重新产生反向依赖。
应用级 `MainWindow` 位于 `apps/desktop`，由桌面壳层目标 `darkeye::desktop_ui`
承载，不进入可复用业务 UI 库。

公共导出名称的初轮覆盖已完成，但视觉验收仍以组件展厅的七主题快照和 Python
基线截图为准；文件合并不代表可以跳过每个公开类的属性、信号与边界行为核对。

## 当前执行顺序

1. 完成所有小型对话框与首次启动条款。
2. 完成私人记录统计页，使三个记录对话框有产品入口和刷新闭环。
3. 完成设置页八个子页及路径控件。
4. 完成通知/Inbox 与采集相关页面。
5. 完成关系图页面装配。
6. 完成书架与首页 3D 视图。
7. 完成暗黑界、完整统计图、Wiki 编辑和剩余专用组件。

每一项完成后必须同时更新本表与 `MIGRATION_STATUS.md`，并至少通过对应的 Qt Test；
涉及视觉的页面还需加入 `darkeye_ui_snapshot`。
