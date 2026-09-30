# Python 代码转写 C++ 建议（性能视角）

## 结论摘要

本项目里最值得转写 C++ 的，不是爬虫或数据库 CRUD，而是**图数据侧的 Python 计算链路**。  
当前图渲染和力导模拟已经是 C++（`PyForceView`），真正剩余瓶颈主要在：

1. `networkx` 过滤与 diff 计算（Python）
2. `networkx -> C++` 参数扁平化与运行时 diff 序列化（Python）
3. 相似图构建中的稠密矩阵与 O(N^2) 逻辑（Python）

## 扫描范围

- 已查看主路径：`core/`, `ui/`, `darkeye_ui/`, `utils/`, `main.py`
- 明确作为非主路径处理：`manual_tests/`, `junk/`

## 已经是 C++ 的部分（不建议重复转写）

- 力导图 OpenGL/仿真：`core/graph/force_directed_view_widget.py:20`（`PyForceView`）
- 颜色轮控件：`darkeye_ui/components/color_picker.py:7`（`PyColorWheel`）

---

## 建议转写清单（按优先级）

## P0：高优先，收益最大

### 1) 图过滤与增量 diff 引擎

- 位置：
`core/graph/graph_session.py:42`, `core/graph/graph_session.py:47`, `core/graph/graph_session.py:55`, `core/graph/graph_session.py:63`, `core/graph/graph_session.py:102`, `core/graph/graph_session.py:190`, `core/graph/graph_session.py:199`, `core/graph/graph_session.py:212`
- 现状：
每次过滤/更新都会在 Python 层遍历节点和边，`nx.ego_graph` + 全量对比 `sub_G`，图变大后会明显拖慢 UI。
- C++ 转写建议：
将 `GraphViewSession` 的核心逻辑（Ego 过滤、邻域扩张、diff 计算）下沉到 C++，维护邻接表与稳定 id 映射，只向 Python 回传最小增量。
- 预期收益：
中大型图（几千到上万节点）交互时，过滤/展开延迟可明显下降（通常是量级改善）。

### 2) `networkx -> C++` 扁平化与运行时 diff 序列化

- 位置：
`core/graph/force_directed_view_widget.py:397`, `core/graph/force_directed_view_widget.py:413`, `core/graph/force_directed_view_widget.py:434`, `core/graph/force_directed_view_widget.py:457`, `core/graph/force_directed_view_widget.py:487`, `core/graph/force_directed_view_widget.py:497`, `core/graph/force_directed_view_widget.py:563`
- 现状：
每次装载图都在 Python 循环构造 `edges/ids/labels/radii/colors`，diff 也在 Python 逐条 dict 组装。
- C++ 转写建议：
提供 C++ `GraphMarshal` 层，直接接收紧凑数组（优先 int id + 连续内存），减少 Python 对象创建和 Shiboken 边界开销。
- 预期收益：
首屏载入与大批量更新时的卡顿下降，CPU 占用更平滑。

## P1：中优先，在图规模变大时收益明显

### 3) 相似图构建（标签向量 + 相似度 + 社区）

- 位置：
`core/graph/graph.py:85`, `core/graph/graph.py:136`, `core/graph/graph.py:149`, `core/graph/graph.py:157`, `core/graph/graph.py:178`, `core/graph/graph.py:190`
- 现状：
稠密矩阵 + 全量相似度矩阵 + 双层循环阈值筛边，复杂度高，数据一大就变慢。
- C++ 转写建议：
用 C++（Eigen/OpenMP）做向量化与并行，优先改成稀疏计算；社区检测可接 C++ 图算法库（按依赖策略选型）。
- 预期收益：
收藏规模上升时，构图耗时下降明显。

### 4) 文本关系增强/增量更新中的批处理逻辑

- 位置：
`core/graph/graph_manager.py:160`, `core/graph/graph_manager.py:171`, `core/graph/graph_manager.py:188`, `core/graph/graph_manager.py:194`, `core/graph/graph_manager.py:239`, `core/graph/graph_manager.py:281`, `core/graph/graph_manager.py:289`, `core/graph/graph_manager.py:292`
- 现状：
逐条解析 notes + 逐条关系更新，混合 Python 循环与查询，数据量上来会放大延迟。
- C++ 转写建议：
把 wikilink 扫描与关系构建放到 C++ 批处理接口；Python 只负责调度和最终落库。
- 预期收益：
初始化图与“最近变更”处理更稳。

## P2：可选，收益场景化

### 5) 本地视频库扫描

- 位置：
`utils/utils.py:589`, `utils/utils.py:600`, `utils/utils.py:617`, `utils/utils.py:635`, `utils/utils.py:640`
- 现状：
`os.walk` + Python 字符串匹配，目录很大时慢。
- C++ 转写建议：
`std::filesystem` + 并行扫描封装为扩展模块；同时建议配合索引缓存（SQLite）避免重复全盘扫描。
- 预期收益：
首次全盘扫描会更快；但这块本质 I/O 受限，收益不如图计算链路。

### 6) 瀑布流布局计算

- 位置：
`darkeye_ui/layouts/waterfall_layout.py:50`, `darkeye_ui/layouts/waterfall_layout.py:63`, `darkeye_ui/layouts/waterfall_layout.py:64`
- 现状：
`setGeometry` 每次都遍历全部 item，卡片很多时会触发 UI 抖动。
- C++ 转写建议：
做 Qt C++ 自定义 Layout，支持增量布局缓存。
- 预期收益：
超长列表/频繁 resize 的顺滑度提升。

---

## 不建议优先 C++ 化的区域

- 纯数据库查询层（`core/database/query/*`）：主要瓶颈通常在 SQL 与索引，不在 Python 执行本身。
- 爬虫（`core/crawler/*`）：主要是网络 I/O 和站点解析稳定性，C++ 化收益低于异步/重试/缓存优化。

## 推荐落地顺序

1. 先做 P0（图过滤 diff + 扁平化桥接）
2. 再做 P1（相似图构建）
3. 最后按需求做 P2（视频扫描/布局）

## 额外建议（先于 C++ 的低成本收益）

1. 对图链路加基准：`load_graph`、`fast_calc_diff`、`apply_diff_runtime` 的耗时埋点
2. 先固定节点 id 为整数映射，减少字符串跨语言传输
3. 对相似图构建先尝试稀疏化与批处理，确认瓶颈后再 C++ 化
