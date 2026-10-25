# 独立 DVD Demo 材质制作

> 当前项目使用 Qt 6.12.0；本文原有测试与性能数据保留实测时的 Qt 6.10.3 版本标注。

当前范围仅为独立 demo，不接入主程序。盒子不必不断增加材质种类，先把不同表面的粗糙度和法线做好。以下参数是美术起点，需结合实际模型和 HDR 环境调整。

## 建议拆分

| 表面 / 材质名建议 | 金属度 | 粗糙度起点 | 重点 |
| --- | --- | --- | --- |
| 不透明塑料盒壳 `shell` | 0 | 0.25–0.45 | 微弱磨砂法线、轻微粗糙度变化；黑塑料不要使用完全纯黑 |
| 封面纸 `cover` | 0 | 0.55–0.8 | 封面颜色图；细纸纹法线可选，避免明显凹凸 |
| 光盘印刷面 `disc-label` | 0 | 0.35–0.6 | 印刷颜色、印刷层粗糙度 |
| 光盘金属面 `disc-metal` | 1 | 0.08–0.25 | 中性铝色、轻微划痕法线和粗糙度；通过 IBL 产生反射 |

普通 DVD 的纸和塑料不需要自发光。盘面彩虹衍射也不是自发光；当前渲染器没有薄膜干涉模型，不能通过降低粗糙度得到真实彩虹效果。

透明盒壳/外膜使用 Transmission，Alpha 保持 1。已加入屏幕空间透射、IOR、厚度与体积吸收；实现范围和预览方式见下文。clearcoat 仍未实现。

## 贴图与 Blender 导出

建议优先准备 Base Color、Roughness、Normal。AO 用于铰链、卡扣等细小凹槽；避免把强烈方向阴影烘进封面颜色图。实时 SSAO 会与材质 AO 叠乘，AO 太重会导致凹槽发黑。

| glTF 通道 | 内容 | 图像颜色空间 |
| --- | --- | --- |
| Base Color RGBA | 颜色与透明度 | sRGB |
| Metallic-Roughness G / B | G 粗糙度、B 金属度 | Non-Color / 线性 |
| Normal RGB | 切线空间法线（+Y / OpenGL） | Non-Color / 线性 |
| Occlusion R | R 通道遮蔽，白色不遮蔽 | Non-Color / 线性 |
| Emissive RGB | 自发光颜色，另乘 factor 和 strength | sRGB |
| Transmission R | 透射比例，乘 transmissionFactor | Non-Color / 线性 |
| Thickness G | 体积厚度，乘 thicknessFactor | Non-Color / 线性 |

AO 可以与 MR 共用 ORM 图（R=AO、G=Roughness、B=Metallic）。这些通道由 [glTF 2.0 规范](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)规定。

在 Blender 使用 Principled BSDF，颜色图连接 Base Color，数据图设为 Non-Color，法线图通过 Normal Map 节点连接 Normal。程序纹理和复杂节点先烘焙成图片；导出 GLB、UV、法线和切线，贴图使用 PNG/JPEG。Blender 的节点识别与导出规则见[官方 glTF 文档](https://docs.blender.org/manual/en/5.4/addons/scene_gltf2.html)。

当前导入器只支持 TEXCOORD_0，不支持纹理变换、Draco、KTX2、骨骼或形变动画；不支持的 required extension 会报错。缺少切线时渲染器由位置与 UV 导数构建切线框架，但推荐导出切线以保持烘焙法线一致。

## 与 DVD demo 的约定

- `back`、`spine`、`front`、`disc` 节点名和铰链原点保持现有约定；节点层级决定开盒动作。
- 需要运行时换封面的材质命名为 **`cover`**，Base Color 因子设白色，避免封面整体被染色。
- 换封面只替换 Base Color 贴图，保留粗糙度、金属度、法线、AO、自发光和透明模式；不同 DVD 的封面材质独立，图片资源共享。
- 当前内置 GLB 仍是原模型导出资产，没有替它凭空生成纸纹/划痕图。新增渲染功能要在模型提供对应贴图后才会表现出来。

## 已补齐的渲染支持与验证

增加法线贴图及强度、材质 AO 及强度、自发光贴图/颜色及 `KHR_materials_emissive_strength`。导入 glTF TANGENT，处理镜像模型的切线手性；法线影响 PBR 光照，AO 只影响间接光，自发光不受 AO 压暗。所有新贴图纳入现有纹理预算和绑定缓存。

导入测试覆盖图像颜色空间、共享 ORM、切线与扩展参数；GPU 像素测试覆盖导数/导出切线、镜像、normalScale、AO 强度和自发光颜色。DVD 测试覆盖换封面后保留这些参数及实例隔离。

2026-10-16，Qt 6.10.3 / D3D11 / NVIDIA Quadro P600：Release、Debug 各 11/11 CTest 通过，包含 DPR 1/1.5 和双面背面法线回归。VS 使用的 `windows-msvc-release-data` 两个 demo 已重新构建；其 DVD demo 的 1000 件聚焦/开盒/归位 smoke 通过。原内置 GLB 哈希未变。当时通过默认模型截图确认原模型无回归，不代表已为原模型创作新贴图；历史测试截图已清理。

## 透明塑料与外膜增量

按用户确认的透明方向，独立 demo 内置模型默认应用“透明塑料盒壳”预览，仅在内存中覆盖名为 `shell` 的材质。侧栏可以切回“模型原材质”或选择“薄片透射预览”。薄片预览不会新增外膜几何；2026-10-20 新增的“透明盒壳＋外膜预览”会生成示意薄面，见下文。正式外膜建议在 Blender 中建立独立薄面，并赋予自己的材质。原始 GLB 文件保持不变。切换预览会重置开盒姿态。

| 参数 | 透明盒壳起点 | 透明外膜起点 |
| --- | --- | --- |
| Metallic / Alpha | 0 / 1 | 0 / 1 |
| Transmission Weight | 1 | 1 |
| IOR | 1.49（美术起点，按实际塑料调整） | 1.49 |
| Roughness | 0.12 | 0.07 |
| Thickness | 0.0008m，假设模型单位为米 | 0，按无限薄片处理 |
| Attenuation Color / Distance | 轻微冷色 / 0.1m，可改中性白 | 白色 / 无限 |

不要用 Alpha=0 来表达透明塑料，这会丢掉表面反射。盒壳应有正确朝外法线、封闭几何和独立铰链，volume 材质推荐单面渲染，避免入口与出口重复应用一次体积。外膜使用单层表面，避免与封面共面产生闪烁。纸质封面的背面若应为白纸，需要单独背面材质；原模型的双面封面会让印刷图从背面可见。

导入支持 `KHR_materials_transmission`（R 通道）、`KHR_materials_ior`、`KHR_materials_volume`（厚度 G 通道、吸收颜色/距离），包括 required extension；材质数据图均线性。遵循 [Khronos 透射规范](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_transmission/README.md)将 Alpha 覆盖与透射分开，厚度/吸收参数见 [volume 规范](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_volume/README.md)。

Blender 导出后，可用侧栏“载入 DVD GLB…”打开，或运行：

```powershell
.\build\windows-msvc-release-data\darkeye_dvd_scene_demo.exe --model "D:\Models\dvd.glb"
```

外部模型保留 Blender 原材质，不自动覆盖 shell。仍需具备 `back/spine/front/disc` 节点。命令行 `--shell-material original|plastic|thin` 可显式选择预览。

实现先绘制不透明物体和 HDR 背景到独立 RGBA16F 快照，再让透射表面采样；避免读写同一个附件。IOR 调整介质 F0，透射替换漫反射部分而保留反射；厚度和折射方向决定屏幕采样偏移，按传播距离应用 Beer–Lambert 吸收。零厚度没有宏观折射，粗糙透射使用有界九点模糊近似。关闭透射或移除模型会释放快照，resize、MSAA 和 QRhi 重建时重新创建。

以上为 2026-10-16 的单层实现记录；当时快照不包含其他透明物体。2026-10-20 已升级为下述多层合成。透明塑料仍不作为不透明阴影/SSAO 遮挡物，未计算有色透射阴影；历史基础性能数据不能直接套用。

透明增量验证：同机 Release / Debug 各 11/11 CTest 通过（45.84s / 50.95s）；覆盖透射 R/厚度 G 通道、零厚度、IOR 保留反射、折射采样偏移、吸收、快照释放/恢复、resize、4x MSAA 与跨窗口 QRhi 重建。侧栏滚动调整后，两配置另各通过一次 DVD smoke。VS release-data 已重新构建，透明塑料 1000 件、原材质/薄片各 10 件，以及带三个 required extension 的外部 GLB 1000 件 smoke 通过。外部 GLB 验证资产由原模型在 build 目录派生，不是声称已验证用户尚未提供的 Blender 文件。

历史测试截图与原始采样文件已清理，此处保留验证结果摘要。

## 多层透射与外膜预览（2026-10-20）

透明表面按相机空间包围盒中心深度从后往前合成，前层可采样已合成的后层，包括 Alpha Blend 表面。初始场景包含不透明封面与 HDR 天空；每批交替写入两张 RGBA16F 场景目标，先复制前一批的线性颜色，再绘制本批。不会采样当前正在写入的目标，曝光、ACES 和 sRGB 编码只在最终显示 pass 执行一次。

每批重新绘制不透明几何以恢复深度，保证封面等前景能遮挡透明层。这样不依赖 MSAA 颜色和 renderbuffer 深度跨 pass 保留；Qt 对这类保留存在后端约束，见 [QRhiTextureRenderTarget 文档](https://doc.qt.io/qt-6/qrhitexturerendertarget.html)。

投影范围不相交的透明表面可共用同一批；透射采样范围计入世界厚度偏移及模糊半径。默认最多 8 次合成，侧栏“透射合成上限”可设 1–16，命令行 `--transmission-passes` 同步支持。超过上限时将最远的若干批合并为第一批，保留近景的独立合成；合并表面仍绘制，但其相互透射降级。侧栏和 benchmark JSON 报告实际 `transmissionPasses`、`transmissionFallbackItems`；后者大于零表示发生此降级。目标纹理数量不随层数增长，绘制和全屏复制开销会随批数增加。

侧栏新增“透明盒壳＋外膜预览”：保留盒壳透射，在 back/front 的封面表面外偏移 0.25mm，生成独立示意薄面，并赋予弱法线细纹、IOR 1.49、粗糙度 0.1、零厚度透射材质。薄面随原节点开合，原 GLB 和用户模型文件都不改写。这是程序生成的展示材质，不等同于用户在 Blender 制作的完整包膜或真实褶皱几何。

```powershell
.\build\windows-msvc-release-data\darkeye_dvd_scene_demo.exe --count 10 --shell-material film --transmission-passes 8
```

载入 Blender GLB 后仍默认使用原材质。独立外膜使用单独 primitive 和透射材质即可参加多层合成，不要求名为 `demo-film`。只有上述预览预设依赖 `cover` / `shell` 命名。

适用范围是可按物体深度排序的前后透明层，不是逐像素深度剥离或路径追踪。互相穿插、深度顺序随像素变化的透明网格、同一 primitive 内交错的多层表面仍可能排序错误。出屏几何、全内反射、焦散、乳白塑料体散射、有色透射阴影与 clearcoat 尚未实现。正式资产仍应拆分纸、盒壳、外膜，避免共面和重复的双面体积。

验证：Release / Debug 的完整 11 项 CTest 分别通过（48.54s / 53.18s）；最终近景优先合批策略和曝光测试另通过 DPR 1、1.5 与 DVD smoke 各 3 项。像素检查包含两/三层线性颜色累积、ACES/曝光只应用一次、Alpha Blend 后景、前景不透明遮挡、单批/双批上限回退、优先保留近层、非重叠合批，以及 4x MSAA 和 resize。此前悬浮保持/归位回归仍通过。VS release-data 两个 demo 已重建，透明盒壳和带外膜预览各通过 1000 件开盒/归位 smoke。原始 GLB SHA256 保持 `8ba289c050fa73ab0653547f65df171ace6a7fcac2e89b1a8274b0a9af0b6523`。

Qt 6.10.3 / D3D11 / Quadro P600、1280×720、DPR 1、4x MSAA，HDR/IBL/AO/阴影全部开启的短时外膜采样如下；最多 60 件驻留、八张共享封面、合成上限 8。CPU 时间是提交耗时，提交频率不是屏幕呈现 FPS，GPU 执行时间和驱动显存均未测量。

| DVD 总数 | 合成次数 | 回退表面 | CPU 提交 P95 ms | 帧提交 Hz |
| --- | --- | --- | --- | --- |
| 1 | 5 | 0 | 0.169 | 60.5 |
| 10 | 8 | 42 | 0.410 | 60.4 |
| 100 | 8 | 251 | 1.558 | 39.9 |
| 500 | 8 | 251 | 1.375 | 56.6 |
| 1000 | 8 | 251 | 1.692 | 56.2 |

结果说明大书架仍会触发远景降级，不能声称所有 DVD 均完整多层折射，也不能以短时采样保证稳定 60 FPS。1000 件样本的纹理/目标估算为 125.76MiB，与真实驱动显存不同。历史测试截图与原始采样文件已清理，此处保留验证结果摘要。
