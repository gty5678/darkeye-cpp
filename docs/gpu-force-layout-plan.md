# QRhi GPU 力导向仿真迁移

本文记录首版迁移方案与测量。后续的排序、读回、实例化和文字优化见 [第二轮性能优化](gpu-force-layout-optimization.md)。

## 目标与基线

将建树、斥力、碰撞、弹簧、中心力和积分移到 QRhi compute。现有 CPU 后端保留，作为数值参考与不支持 compute 时的回退。现有 CPU Barnes–Hut 已通过 OpenMP 并行遍历，建树串行；比较对象是这一实现。

## 实现顺序

1. 新增独立的 GPU 仿真引擎和 compute shader。位置/速度双缓冲常驻 GPU；GPU 计算边界、Morton 编码、排序和层次树聚合。采用空间排序二叉树实现 Barnes–Hut，避免串行 CPU 建树及浮点原子加法。碰撞查询复用树的 AABB，保留精确叶子碰撞。
2. CPU 在拓扑变化时构建 CSR 邻接表；GPU 每节点汇总边力，计算中心力、阻尼、位移限幅和拖拽约束。保留现有参数和冷却规则。
3. 接入 QRhiWidget 帧循环，默认自动选择 GPU；支持 cpu/gpu/auto 和能力/资源失败回退。异步读回供现有 CPU 顶点生成、拾取和内容范围查询使用；不使用逐帧 finish()。处理资源重建、异步结果过期、暂停、恢复、重启和图增删。
4. 设置面板显示实际后端并允许切换；演示程序支持后端及规模参数。
5. 实际 QRhi 计算测试：精确模式与 CPU 对照、近似误差、重合点、拖拽、空图/单节点、拓扑变化及资源生命周期。运行已有物理和页面渲染回归，并比较 CPU/GPU 在相同图上的耗时。

## 验收与限制

- GPU 路径不调用 CPU 力计算或 CPU 建树；树遍历有界，所有节点包括重复 Morton 编码均保留。
- 无数据竞争：排序阶段独占比较对；聚合层间分 pass；力计算只写所属节点，积分使用独立输出缓冲。
- 对照使用精确开角模式验证公式，再验证默认开角近似与稳定性；二叉层次树与原四叉树无需逐点完全一致。
- 异步结果必须匹配图/仿真代次，不得复活暂停或覆盖新拖拽、图变更。
- 初版保留现有 CPU 绘制准备，必须报告读回成本和端到端限制；后续 GPU 顶点生成属于独立渲染优化，不作为本次计算迁移的前提。
- 不预设 GPU 一定快于并行 CPU。记录真实设备、构建模式、图规模、单步计算和端到端更新表现。

## 当前状态

- 已完成引擎、帧循环、设置面板、演示参数和测试；主程序 Debug 构建通过。
- 默认 `auto` 在 QRhi 初始化后选择 GPU，设备缺少 compute/非 uniform buffer 读回能力或资源初始化失败时使用 CPU。QRhi 尚未创建时，隐藏页面仍可沿用现有 CPU 预热。
- 初始预热每次提交最多四个 GPU 步，只读回一次。冷却按物理步递进，早停在批次末检查；重启和拖拽关闭预热，恢复单步交互。
- 位移限幅前的绝对速度在 GPU 上计算并随快照读回，保留 CPU 收敛统计。拖拽节点保留 CPU 的固定位置和速度处理规则。
- 无栈二叉树遍历，按 Morton 顺序分配相邻线程；无需线程私有遍历栈。GPU bitonic 排序工作量为 O(N log² N)，建树聚合工作量 O(N)。Barnes–Hut 和碰撞查询的最坏情况仍可退化，不能保证任意图上的收益。

## 验证记录（2026-10-26）

环境：Windows 11、Qt 6.12.0、MSVC 2022、AMD Ryzen 5 7500F、NVIDIA Quadro P600、D3D11。CPU 构建启用 OpenMP（`-openmp`），CPU/GPU 开角均为 0.6。

- Debug：`darkeye_gpu_simulation_tests`、`darkeye_graph_view_physics_tests`、`darkeye_graph_view_demo_smoke`、`darkeye_graph_page_rendering_tests` 全部通过。GPU 套件 15 项通过，默认跳过性能测试。
- Release：GPU 套件含性能测试 16 项通过。覆盖精确计算与 CPU 对照（含 1501 节点）、重复空间编码、GPU 常驻状态、拖拽、四步批处理、冷却边界、限幅前收敛统计、长期稳定性、空图拒绝、暂停恢复、切换后端及拓扑变更。
- 在 2048 节点的测试分布上，默认开角斥力相对 RMS 误差为 2.607%，相对于同一 GPU 引擎的精确模式；这是该测试分布的误差，不是任意图的误差上限。
- 已实际验证 Windows/D3D11。GLSL、HLSL、MSL 着色器打包通过；其他系统/后端尚未实机验证。

性能使用固定种子 42 的合成图、N−1 条链式边，每轮重新初始化相同输入，先预热一步，测十步，共五轮取每步均值的中位数。密集分布为 ±150 的正方形；应用尺度分布为 ±(25√N+50)。GPU 时间包含命令记录、提交、完成及位置/速度/收敛统计读回，不含页面顶点生成和实际显示帧调度。

| 分布 | 节点数 | CPU ms/步 | GPU ms/步 |
| --- | ---: | ---: | ---: |
| 密集 | 400 | 0.614 | 0.553 |
| 密集 | 2000 | 1.582 | 1.119 |
| 密集 | 10000 | 5.078 | 3.711 |
| 应用尺度 | 400 | 0.521 | 0.499 |
| 应用尺度 | 2000 | 0.783 | 0.679 |
| 应用尺度 | 10000 | 4.685 | 1.915 |

这些是合成图上的本机测量，不是用户实际数据库图的性能保证，也不等同于 FPS 提升。小图差异很小；现有 CPU 顶点生成和文字处理仍会影响整体表现。

## 使用与复现

- 图设置面板的 Layout Backend 可选自动、GPU、CPU，Active Layout 显示实际后端。GPU Tick 显示提交至异步读回的单步平均延迟（预热批次均摊），包含帧调度；不能与离屏性能测试的原始执行时间混为一谈。
- 演示程序：`darkeye_graph_view_demo --backend gpu --nodes 10000`，也支持 `cpu` 与 `auto`。
- 构建测试目标：`cmake --build --preset debug-tests --target darkeye_gpu_simulation_tests darkeye_graph_page_rendering_tests darkeye_graph_view_physics_tests darkeye_graph_view_demo`。
- 运行回归：`ctest --preset debug-tests -R "^darkeye_(gpu_simulation_tests|graph_view_physics_tests|graph_view_demo_smoke|graph_page_rendering_tests)$" --output-on-failure`。
- Release 性能测试：设置 `DARKEYE_GPU_LAYOUT_BENCHMARK=1`，运行 `ctest --preset release-tests -R "^darkeye_gpu_simulation_tests$" --output-on-failure`。
- GPU 套件明细写入各构建目录下的 `tests/gpu-simulation-results.txt`。
