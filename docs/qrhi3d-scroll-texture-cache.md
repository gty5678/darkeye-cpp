# DVD 横向滚动：纹理预算反复重建修复

2026-10-24，Release / Qt 6.10.3 / D3D11。

## 原因

书架滚动时，封面会在预加载尺寸与屏幕所需尺寸之间切换。旧的 GPU 纹理预算分配器每帧按全部源纹理的总尺寸计算一个缩放比例，再把这个比例应用到所有驻留纹理。

只要一张封面的源尺寸改变，其余封面的目标像素尺寸也可能改变。渲染器因此在 GUI 线程重新缩放整批图片，销毁 GPU 纹理和相关绑定，再上传图片、生成 mipmap。此前只用共享占位封面测试，未覆盖这一预算压力下的重复上传。

## 修改

- 保留源尺寸没有改变的驻留纹理分配。
- 新纹理按当前预算计算初始尺寸。
- 超预算时，只缩小满足回收需要的少数纹理；优先处理相对源图分辨率较高的纹理，以平衡清晰度。
- 明确修改预算时重新分配；源图总量低于预算的 75% 时恢复完整分辨率。
- 新增累计纹理上传次数、上传字节数；统计面板显示最近半秒的上传 MiB/s。

没有关闭 HDR、IBL、AO、阴影或 4× MSAA。纹理仍受同样的 128 MiB 源纹理预算约束，预算不包含 HDR、阴影及颜色/深度目标。

## 对照测试

使用生产 `DvdShelfView` 的模型、材质和灯光，1000 件 DVD，最多 60 件驻留，96 张独立生成的 1024×2048 JPEG 轮换。窗口请求 1280×800 逻辑像素，系统 DPR 3，实际渲染目标 3840×2337。预热后持续横向移动约 2.2 秒。

| 指标，全效果 | 修改前 | 修改后 |
| --- | ---: | ---: |
| 帧提交频率 | 15.84 FPS | 58.90 FPS |
| P95 CPU 帧准备/录制 | 99.54 ms | 3.84 ms |
| 测量期间纹理上传次数 | 1093 | 61 |
| 测量期间上传量（不含 GPU 生成的 mipmap） | 1737.23 MiB | 51.30 MiB |

分别关闭 IBL、AO、阴影，或改为单采样、降低目标分辨率，修改前仍只有约 12–16 FPS，修改后各组约 59–60 FPS。共享占位封面的原实现已经能达到约 59–60 FPS。结果支持本次瓶颈位于独立封面引起的 CPU 缩放和上传重建。

以上为固定合成封面的短程回放和帧提交统计，不是用户原库或屏幕实际呈现 FPS，也未直接测量 GPU 执行耗时。

## 回归入口

- `RhiSceneWidgetTest::coverLodChangesDoNotResizeTheEntireTextureCache`：预算压力下改变一张源纹理尺寸，最多允许上传三张图片；验证纯相机移动不上传纹理，以及提高/降低预算后的实际预算约束。
- `ShelfPageTest::scrollPerformanceProbe`：默认跳过。设置 `DARKEYE_SHELF_PERF_OUTPUT` 为 JSON 输出路径可运行对照；同时设置 `DARKEYE_SHELF_PERF_UNIQUE_COVERS=1` 启用独立高清封面，否则使用占位封面。Windows 下将 `TEMP`、`TMP` 指向可写的测试目录。

本地原始测量位于 `build/shelf-regression-run/scroll-covers-before.json`、`scroll-covers-after.json`；这些构建产物不作为仓库源文件提交。

## 验证结果

Release 主程序构建通过。渲染 DPR 1 全套 17 项通过，光照 DPR 1/1.5、书架集成通过；新增纹理预算与统计面板用例在 DPR 1.5 专项通过。

DVD DPR 1/1.5 全套各有 19 项通过、2 项失败，失败为 `wheelRefreshesHoverWithoutMouseMovement` 和 `hoverStaysPulledOutUntilPointerLeaves`。将 Renderer.cpp 换回本次优化前的 HEAD 实现后，单独重跑两项仍同样失败；随后已恢复优化后的实现。不能将本次 DVD 全套回归记为全部通过。基线复查日志为 `build/shelf-regression-run/scroll-hover-baseline.txt`。
