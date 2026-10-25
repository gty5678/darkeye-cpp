# 独立 DVD Demo：HDR、IBL、AO 和阴影

> 当前项目使用 Qt 6.12.0；本文原有测试与性能数据保留实测时的 Qt 6.10.3 版本标注。

2026-10-16。仅新增独立 demo 的光照能力，主程序不接入。使用仓库已有 `resources/hdr/lebombo_1k.hdr`，没有新增网络资源或 Qt Quick 运行时依赖。

## 画面与操作

`darkeye_dvd_scene_demo` 默认开启 HDR 背景、IBL、AO、方向光阴影和色调映射。侧栏各项可独立切换；曝光为 10%–400%。新增“环境旋转”滑条和角度框（0–360°）：背景和 IBL 光照同步绕竖轴旋转，360° 回到初始方向。关闭 HDR 背景后，明暗主题的纯色背景生效。



同一 GLB、同一封面、同一相机姿态，1280×720、DPR 1、4x MSAA；合成数据仍为八张封面循环复用。历史测试截图与采样文件已清理，本文保留的性能摘要为历史结果，不代表当前默认画质。

```powershell
./build/windows-msvc-release-data/darkeye_dvd_scene_demo.exe
./build/render3d-release/darkeye_dvd_scene_demo.exe `
  --screenshot build/dvd-hdr-open.png --screenshot-closed build/dvd-hdr-closed.png
./build/render3d-release/darkeye_dvd_scene_demo.exe --no-ao --screenshot build/dvd-no-ao.png
./build/render3d-release/darkeye_dvd_scene_demo.exe --no-shadows --screenshot build/dvd-no-shadows.png
./build/render3d-release/darkeye_dvd_scene_demo.exe --no-ibl --screenshot build/dvd-no-ibl.png
```

另外支持 `--no-hdr-background`、`--no-tone-mapping`。命令行会同步侧栏开关状态。

## 实现

- **HDR**：CPU 解析 Radiance RGBE（scanline RLE / 普通像素，`-Y +X`），保留大于 1 的线性亮度。图片、图片尺寸和 scanline 均有边界检查；错误格式明确失败。原图以 RGBA16F 上传，天空通过相机射线采样等距柱状环境图。
- **IBL**：CPU 预计算余弦卷积漫反射 irradiance、八级 GGX 粗糙度预滤波反射和 split-sum BRDF LUT。128 个确定性样本；specular 128×64、diffuse 32×16、BRDF 64×64。按粗糙度选择预滤波层级，不把普通平均 mipmap 当作 GGX 反射。环境图不随相机平移，只随方向改变采样。
- **AO**：法线/线性深度预通道 + 24 样本半球 SSAO，半分辨率，最长边最多 960。半径以世界单位计量；默认 0.025m、强度 1。只影响环境/间接光，不压暗直射光，也不修改封面原图。
- **阴影**：2048² R32F 方向光阴影图，按驻留场景边界拟合正交光源相机；包含相机外但仍驻留的投影物。使用 3×3 PCF、斜率相关 color-depth bias，避免薄壳表面条纹。深度/alpha-mask 规则与可见材质保持一致，物体移动时实时更新投影。
- **显示**：照明/透明混合保留在线性 RGBA16F 中，曝光后可选 ACES 拟合色调映射，再编码为显示 sRGB。关闭色调映射仍可调曝光。

公开入口为 `render3d/Lighting.h` 和 `RhiSceneWidget::setEnvironment/setLighting/lighting`。CPU 的 EnvironmentMap 保留在 Renderer 外部配置中；GPU 的环境图、AO/阴影目标、管线和 SRB 由当前 QRhi 的 LightingResources 持有。跨窗口/QRhi 重建时重新创建 GPU 资源，CPU 预处理结果可复用。关闭 AO/阴影会释放其目标。

缓存预算中的“封面预算”仍只约束源封面纹理；环境图与渲染目标单独计入统计面板的“纹理/目标估算”，不冒充驱动显存。

## 验证范围

新增 `darkeye_render3d_lighting_1` / `_1.5`，在真实 D3D11 framebuffer 验证：

- RGBE 解码保留 HDR 数值，常量环境卷积能量不变；损坏/truncated scanline 明确失败。
- HDR 背景颜色、曝光变化、IBL 对金属反射的影响。
- 遮挡物移动后投影随之移动/消失，接触区 AO 压暗而空旷平面基本不变。
- HDR/IBL 的跨顶层窗口重建、环境替换/清除，AO/阴影的 resize、MSAA 与关闭后资源回落。

原有窗口生命周期、颜色空间、透明混合、缓存、鼠标交互、DVD 返回对齐与 1000 件 smoke 继续执行。历史原始测试日志已清理，验证结果摘要保留如下。

SSAO 只利用当前画面可见深度；方向光为单张 shadow map。HDR 背景是环境照片，不是可接收投影的实体地面。当前 DVD 有自阴影和相互投影；不宣称与旧 Qt Quick 3D 的每个像素相同。

## 全效果性能记录

D3D11 / Qt 6.10.3 / 1280×720 / DPR 1 / 4x MSAA，HDR、IBL、AO、阴影、色调映射全部开启。每组 16ms PreciseTimer 推进 30 次相机输入；八张共享封面、最多 60 件驻留，CPU 预处理发生在启动阶段，不计入帧提交时间。

| DVD 总数 | 帧提交 Hz | P95 交互步 ms | P95 CPU 提交 ms | Draw Calls | 纹理/目标估算 MiB |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 60.45 | 16.77 | 0.16 | 21 | 89.06 |
| 10 | 60.43 | 16.68 | 0.25 | 183 | 90.52 |
| 100 | 60.47 | 16.87 | 0.76 | 1019 | 90.52 |
| 500 | 60.39 | 16.95 | 1.00 | 1019 | 90.52 |
| 1000 | 60.50 | 16.59 | 1.10 | 1019 | 90.52 |

Draw Calls 包含法线/深度预通道、阴影、SSAO、HDR 背景、可见模型和显示 pass。帧提交频率不是屏幕呈现 FPS；GPU 执行时间和驱动显存仍未采集。本表不能推断 1000 张独立封面的冷加载成本，也不能作为旧版画质等价的性能结论。

原始性能采样 JSON 已随历史测试产物清理，此处保留测量结果摘要。

最终验证：独立 Release 全套 11/11（36.84s）、Debug 全套 11/11（41.59s）通过；补充曝光/创建失败检查后，两个配置的光照 DPR 1/1.5 与 DVD smoke 再次 3/3 通过。根目录 release-data 的两个 demo 已重新编译，1000 件 HDR DVD smoke 通过。


环境旋转增量验证：Debug、Release 的光照 DPR 1/1.5 与 DVD smoke 各 3/3 通过。新增红/蓝半球环境 GPU 读回，检查 180° 背景/金属反射同步变化、360° 恢复初始方向，旋转不重复上传几何。
