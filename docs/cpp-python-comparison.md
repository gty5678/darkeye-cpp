# C++ 与 Python 版本差异核对

日期：2026-10-12。

范围：C++ 当前工作区（HEAD `5066a18`，包含现有未提交的统计截图改动）；Python 可读取的工作区源码、Git HEAD `a266b03` 与暂存改动。Python 的 ui/controller/settings/server 等目录直接读取被拒绝，提升权限后仍失败，因此这些模块主要按已提交源码核对。权限导致的 Git `D` 状态不作为真实删除证据。

本次为源码静态核对，未运行两个程序做逐页截图、端到端操作或性能测量。以下“已存在”表示找到实现，不保证每个运行分支完全等价。没有修改业务代码。

## 确认的功能及行为差异

| 优先级 | 项目 | Python | C++ | 影响 |
|---|---|---|---|---|
| 高 | 身高统计的“撸过”范围 | 每位有记录的女优权重为 1；仅“撸过加权”按记录次数加权 | `actressHeights()` 在 scope=1 时也累计关联作品的权重 | 同一女优关联三部已记录作品时计三份，改变分布；应在 scope=1 输出权重 1 |
| 高 | 首页筛选与分页 | 番号、女优、标题、故事、笔记、导演、男优、片商、厂牌、系列筛选；4 个范围、13 个排序选项、上一页/下一页与数量显示 | `HomePage` 只有刷新按钮；随机查询 18 部作品 | 首页不能按条件浏览或翻页 |
| 高 | 翻译引擎 | Google、LLM 两种；LLM 配置不完整时 factory 回退 Google | 设置仅提供 LLM；翻译服务直接调用 OpenAI 兼容接口 | Google 配置不能等价迁移，也没有对应的回退行为 |
| 中 | 首页数据更新 | 监听 workDataChanged、likeWorkChanged；重新进入首页有贴图/数据刷新；再次点击同一路由调用 refresh | 作品变更刷新链未包含首页；HomePage 没有对应监听或 showEvent；同一路由提前返回 | 首页可能保留旧条目、旧封面，需手动刷新 |
| 低 | 直方图信息 | KDE 曲线与 5%/95% 加权分位标记；曲线采样 500 点 | 已补齐 5%/95% 加权分位虚线与标签；KDE 曲线仍为 160 点 | 分位标记已对齐，曲线采样细节仍不同 |
| 中 | 统计标题 | 拍摄年龄、身高、罩杯等标题包含统计范围 | 多项标题固定，未标明当前范围 | 保存的截图难以辨别公共库、收藏、撸过或加权范围 |
| 低 | 出道年龄精度 | `round((debut-birthday)/365.25, 1) - 0.5` | 不先 round，直接减 0.5 | 分箱边界、KDE 与分位计算可能不同 |
| 中 | 验证/积压警告 | MessageBoxService 显示警告，可请求窗口引起注意；Cloudflare 提示包含 URL | Toast 显示 6/8 秒；Cloudflare 提示仅站点与番号 | 后台工作时更容易漏掉需要处理的验证提示 |
| 低 | 默认启动页 | home | mutiwork（作品页） | 首次进入的页面不同 |
| 低 | 资源标题刷新 | 每秒一次，仅窗口可见且活跃时刷新；RSS 与 psutil CPU | 每 100ms 一次，不检查活跃状态；公共/私有工作集与平均 CPU | 标题内容、CPU 统计口径和更新开销不同；未测量实际性能 |
| 低 | 路径失效处理 | 路径不存在时重置设置为默认路径 | 保留配置，解析绝对/相对路径，不检查存在性 | 旧磁盘路径失效后的行为不同；属于策略差异 |
| 低 | 退出清理 | 主窗口 closeEvent 调用 clear_temp_folder | closeEvent 仅保存窗口状态；未找到对应 data/temp 整体清理 | 通用临时目录清理策略不同；QTemporaryDir 自清理不属于此缺项 |

## 关键源码位置

- 身高权重：C++ `libs/database/src/database/repositories/StatisticsRepository.cpp` 的 `actressHeights()`；Python `core/database/query/statistics.py` 的 `fetch_actress_height_with_weights()`。scope=1 的差异在输出权重处，scope=2 不应改为 1。
- 首页：C++ `libs/ui/pages/HomePage.cpp`；Python `ui/pages/HomePage.py`；更新与路由：C++ `apps/desktop/MainWindow.cpp` 的 `refreshWorksAfterCapture()`、`navigateTo()`，Python `ui/navigation/router.py`。
- 翻译：C++ `libs/ui/pages/SettingsPage.cpp`、`libs/services/src/services/LlmTranslationService.cpp`；Python `core/translation/factory.py`、`ui/pages/settings/translation.py`。
- 图表：C++ `libs/ui/pages/PlotTabPage.cpp`；Python `ui/statistics/MplCanvas.py` 的 `_hist_chart()` 和 `_paint_hist_kde()`。
- 出道年龄：C++ `StatisticsRepository::actressDebutAges()`；Python `fetch_actress_debut_age()`。
- 提示、启动页、标题刷新、退出：两个版本的 MainWindow；Python `controller/message_service.py`。
- 路径：C++ `libs/settings/Paths.cpp`；Python `settings/paths.py`。

## 已找到对应实现的模块

作品、女优/男优管理，标签/片商/厂牌/系列管理，批量维护、软删除/回收站、个人记录，采集队列及完整度提示，LLM/llama 服务，视频关联，NFO 导入，数据库备份/WebDAV，关系图谱，3D 书架，主题、快捷键、更新服务均有对应实现。

六个浏览器插件 HTTP 路由均已对应：health、check_existence、minnano-actress-capture、capture/one、crawler-backlog-warning、cloudflare-challenge-notify。这里只核对路由和主要处理逻辑，未验证所有请求/错误响应的完全兼容性。

两版当前统计画布均使用 QPainter；Python 虽保留 MplCanvas 文件名，本次读取的实现没有 Matplotlib 导航工具栏，不应把缺少该工具栏列为迁移遗漏。

词云、年回忆录在 Python 中也被禁用；Python 的 3D 身材统计按钮也禁用，因此不属于 C++ 独有缺失。Dashboard 的占位内容需要与 Python 单独核对，不能仅凭 C++ 占位字样判定迁移缺失。

当前 C++ 未提交改动已经接入统计图表 C 键截图，本次按这些改动存在进行对比。它比 Python MainWindow 当前的部分截图路由多了统计图表入口。

初始化 SQL：私有库脚本逐行一致；公共库脚本只找到一处逗号差异，C++ 为 actress_name 两个外键声明之间补上逗号。该差异是语法修正，不是新增字段或业务表。此结论仅涉及初始化脚本，不保证迁移代码或现有数据库完全一致。

## 建议推进顺序

1. 修正 scope=1 身高统计权重，并用“同一女优关联多部已记录作品”的固定数据验证。
2. 补齐首页筛选、范围、排序、分页及首页刷新链。
3. 若要求与 Python 翻译功能等价，加入 Google 引擎与明确的回退策略。
4. 分位线已补齐；继续补齐统计范围标题，统一年龄精度。
5. 确定启动页、提示方式、资源刷新与路径/临时目录策略是否继续保留 C++ 当前行为。

后续若需要宣称完全一致，仍需用相同数据库对列表、筛选结果、统计值、采集写入、备份恢复和截图逐项执行运行验证。
