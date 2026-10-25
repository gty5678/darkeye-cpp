# QRhi DVD 渲染迁移记录

> 当前项目使用 Qt 6.12.0；本文原有测试与性能数据保留实测时的 Qt 6.10.3 版本标注。

2026-10-23：用户已授权接入主程序，当前书架页改用原生 QRhi 场景。接入范围与验证见 [书架接入记录](qrhi3d-shelf-integration.md)。以下独立 demo 限制是早期执行范围。

2026-10-16，Qt 6.10.3 / MSVC x64 / Windows D3D11。
执行来源：[实施计划](darkeye-qrhi3d-implementation-plan.md)。用户确认本轮“先完成独立 DVD demo，暂不接入主程序”。

## 交付与范围

已实现 `darkeye_dvd_scene_demo` 完整独立书架，以及 `darkeye_render3d_demo` 网格/GLB/生命周期诊断程序。源码与调试入口见 [README](../apps/render3d_demo/README.md)。

VS 的 `windows-msvc-release-data` 配置已开启 demo，启动项选 `darkeye_dvd_scene_demo.exe`。当前可执行文件位于：

```text
build/windows-msvc-release-data/darkeye_dvd_scene_demo.exe
build/render3d-debug/darkeye_dvd_scene_demo.exe
build/render3d-release/darkeye_dvd_scene_demo.exe
```

主程序链接规则不含新库；独立 DVD 程序只链接 dvd_scene/render3d 与 Qt Widgets/Gui/Core。原 DVD QML、数据库绑定、图谱和其他 Quick 页面继续保留。未执行 P3-02 的生产数据库对接、P3-07、P5。

## 需求与证据核对

| 计划要求 | 当前实现/验证 |
| --- | --- |
| P0 源码、交互、资源盘点 | [盘点](qrhi-3d-inventory.md)，原文件哈希见模型 provenance |
| P0 五种数量的旧版基线 | 已验证 1、10、100、500、1000 件；历史测试截图与合成数据已清理 |
| P0 交互记录 | 已验证 48 帧确定性状态回放；历史测试 GIF 已清理，可由 recorder 重建 |
| P1 QRhi、深度、shader、生命周期 | 两个 demo；DPR 1/1.5 D3D11 真实 GPU framebuffer 回归，安全销毁/重建、跨窗口 reparent、MSAA/格式、隐藏/最小化 |
| P2 CPU 场景和资产 | Scene 稳定 NodeId、父子变换与失效传播、共享 Mesh/Material/Texture/ModelAsset、Camera 投影/逆投影；对应 scene tests |
| P2 GLB/glTF | cgltf v1.15/MIT；外部/嵌入图片、Unicode、多个 primitive、矩阵/TRS、法线生成、UV、MR/baseColor 纹理及采样；损坏/缺失资源与 required extension 测试 |
| P2 真实 DVD 几何 | 从仓库四个 Qt mesh 导出 GLB，5 节点/9 primitives；保留几何、法线、材质子集，V=1-v；测试 Qt ARGB 转线性色与模型层级 |
| P3 DVD 数据与布局 | 独立 DvdItemViewModel/JSON adapter；稳定排序、重复 ID 原子拒绝、更新与替换；最多 60 个驻留书架实例，保留交互中对象 |
| P3 选择、拾取与交互 | 最近盒体命中（旋转/非均匀缩放）；真实鼠标点击/双击/右键旋转/中键平移/Ctrl+滚轮缩放/普通滚轮横移/Esc 测试；稳定 ID 信号 |
| P3 动画与封面 | 两阶段聚焦、开盒、先关盒后返回、切换/中断恢复；纯动画中断 100 次、DVD 循环 20 次；背景两线程解码、占位、路径失效、异步替换和销毁 |
| P4 材质与颜色 | 基础 GGX Metallic-Roughness、环境光/方向光、opaque/mask/blend；RGBA16F 线性照明/透明混合后 sRGB 编码；GPU 颜色、纹理象限、深度和透明测试 |
| P4 缓存/裁剪/数量 | CPU 封面 LRU、256/512 多分辨率、选中优先；每 QRhi 共享几何/纹理、GPU 源纹理预算与降采样、淘汰旧绑定、视锥裁剪；64 KiB 预算及纹理 16→1 释放测试 |
| P4 主题/尺寸/空闲 | 明暗清屏、3840×2160 和最小画布、DPR 1/1.5；静止不持续提交帧，隐藏/最小化停止动画；空/1/10/100/500/1000 样本 |
| P4 同模型/纹理/姿态对照 | [对照及差异](qrhi3d-visual-comparison.md)，修正倒置 UV、Qt ARGB 和聚焦 pivot；HDR/IBL/AO、业务覆盖层等差异明确保留 |
| P4 实时统计/测量 | [性能报告](qrhi3d-performance.md)，旧/新 5 组原始 JSON；缓存估算与 GPU 执行时间分开，未测项目为 null |
| 独立构建与主程序隔离 | Debug、Release、根目录 release-data 编译和运行通过；链接审计，无新增生产链接 |

`instancing` 暂未实施：虚拟化最多 60 件，当前 1000 件样本约 297 Draw Calls、P95 CPU 提交低于 0.5ms，优先保留共享几何/裁剪，后续按实际独立封面负载决定批处理。

## 最终验证

| 配置 | 构建 | CTest/运行证据 |
| --- | --- | --- |
| 独立 Release | 通过 | 9/9 CTest，24.94s |
| 独立 Debug（/Z7） | 通过 | 9/9 CTest，29.45s |
| 根目录 release-data | 两个 demo 目标通过 | 1000 件 focus/open/return smoke |

每个配置 9 项 CTest 包括 CPU scene、GLTF、DPR1/1.5 renderer、DPR1/1.5 DVD 场景及三个 smoke；没有失败或跳过。历史原始测试日志已清理，此处保留当时的验证结果摘要。测试实际使用 D3D11 读回，不是 Null renderer。

构建依赖问题已修复：中文 MSVC showIncludes 探测可能被 CMake 二次解码，造成 Ninja 无头文件依赖。`CompilerOptions.cmake` 只修正该乱码前缀；clean build 后核对 main.cpp 257 条依赖，后续公共头文件变更能触发重编。`VSLANG=1033` 在仅安装中文编译器语言包时不能解决该问题。Debug 采用 Embedded `/Z7` 避开共享 PDB 服务错误。

本地根构建验证临时关闭 vcpkg manifest 安装以复用已安装依赖；测试结束已恢复缓存 `VCPKG_MANIFEST_INSTALL=ON`，没有更改依赖清单。

## 后续集成验收项

- 主程序业务数据/数据库适配、作品详情/爱心/编辑/删除/视频等覆盖层、DVD 与图谱/其他 QQuickWidget 混合图形回归，随后才允许 P5 替换和打包清理。
- 旧版 HDR/IBL/AO、normal map、阴影与光盘自转/视频业务关联不在本轮基础独立原型中；未宣称完全视觉等价。
- GPU 执行时间、驱动显存和长期趋势未采集；纹理/Buffer 估算不能代替驱动显存或泄漏验证。正式性能验收需同一画质和实际数据负载。
- 物理跨屏拖动/不同显示器 DPI、其他后端和长时间稳定性仍需硬件人工验证；自动化覆盖进程 DPR 与跨顶层窗口 reparent。
- 模型上游独立许可证未在仓库找到；派生资产记录来源哈希，不代表取得新的第三方授权。对外分发前核实原模型授权。

这些验收项在计划中保留未完成状态；本轮可运行独立 demo 不替代完整生产迁移验收。

## 返回对齐修复（2026-10-16）

创建与刷新书架实例现在统一使用 `DvdCaseFactory::shelfTransform`，避免刷新丢失模型 pivot 偏移（X=0.006707、Z=0.000293），使初始位置和返回终点一致。
返回顺序调整为：关闭盒子 → 在书架外恢复朝向并对齐原槽位（320ms）→ 沿 Z 方向插回（180ms）。关闭阶段保持盒体位置，隐藏后跳过的时间也按阶段截止时间推进。
回归覆盖闭盒、完全打开、打开中途返回、滚轮加速关盒，以及刷新/两轮聚焦开合返回后的原始位置恢复。修复前的两个回归用例均可复现失败。
验证结果：Debug、Release 的 DVD DPR 1 / 1.5 与独立程序 smoke 均通过（每配置 3/3 CTest）；release-data 重新编译后 1000 件 smoke 通过。历史原始测试日志已清理。

## HDR / IBL / AO / 阴影增量（2026-10-16）

按后续用户要求为独立 DVD demo 加入仓库 `lebombo_1k.hdr`、漫反射和镜面 IBL、SSAO、方向光阴影及色调映射。新默认画面和实现说明见 [光照增量](qrhi3d-lighting.md)。本文件前面的 9 项测试及基础光照测量是当时的阶段记录；新增测试后总数为 11。主程序仍不接入。

## 材质增量（2026-10-16）

补齐 glTF 法线贴图、切线、材质 AO 和自发光贴图/强度，统一纳入纹理预算。材质 AO 和 SSAO 叠乘，只影响间接光；自发光独立于 AO。新增导入和 GPU 像素验证，以及运行时换封面保留表面参数的实例隔离测试。此前关于 normal map 尚未实现的文字为历史阶段记录。材质制作与当前限制见 [DVD 材质指南](qrhi3d-materials.md)，内置模型资产保持原样。

用户随后确认透明盒壳/外膜方向：增加 `KHR_materials_transmission/ior/volume`、透射/厚度图、IOR 反射、屏幕空间折射和吸收。独立 demo 增加透明盒壳/薄片预览及外部 GLB 载入；只在内存中调整内置模型材质，原 GLB 不变。薄片预览不创建外膜几何；单层快照不包含其他透明物体，没有焦散/有色透射阴影。详见材质指南增量部分。

## 悬浮抽出保持修复（2026-10-16）

用户反馈鼠标仍悬浮时 DVD 抽出后立即缩回。原因是悬浮动画结束的最后一帧，`refreshVisible()` 将无活动动画的非选中实例重置为 `home`。现在由交互控制器提供持久的书架姿态 `shelfPose()`，布局刷新和悬浮动画使用同一目标；悬浮保持 +0.03m，移开后才回到 home。聚焦/归位动画继续优先。

新增真实 Widget 事件回归：等待超过 200ms 动画时长、重复鼠标移动、数据刷新后仍保持抽出，Leave 后归位；聚焦后的 hover/Leave 不改变聚焦姿态。新测试在旧实现上失败，修复后通过。

Release / Debug 各通过 DVD 的 DPR 1、DPR 1.5 和 1000 件 smoke 共 3 项检查；VS release-data demo 已重建。历史原始测试日志已清理。修改仅限独立 DVD 场景。

## 多层透射与外膜预览（2026-10-20）

继续仅在独立 renderer/demo 中增加渲染功能。透明表面按深度排序，使用两张 RGBA16F 目标交替合成；前层能采样后层透射和 Alpha Blend。默认最多 8 批（可调 1–16），投影/采样范围不相交的表面合批，超限仍绘制并报告回退数。每批重建不透明深度，最终仅做一次曝光/ACES/sRGB；目标纹理数量不随层数增长。

侧栏及 CLI 新增“透明盒壳＋外膜预览”（`--shell-material film`），在原封面外生成示意薄面并使用弱法线细纹，不改写原 GLB。支持载入 Blender 制作的独立膜材质。仍不保证交叉/穿插透明网格的逐像素正确顺序，不支持焦散、体散射、clearcoat 或有色透射阴影。详情与参数见 [材质指南](qrhi3d-materials.md)。
