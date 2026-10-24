# QRhi 3D 迁移盘点（独立 DVD demo）

2026-10-16。按用户要求只完成独立程序，生产页面保持原路径。
旧版五组截图和交互状态回放已留存，见 [执行记录](qrhi3d-progress.md) 与 [性能记录](qrhi3d-performance.md)。
GPU 时间/驱动显存、物理跨屏与主程序混合图形回归仍是集成阶段验收项。

| 功能 | 当前文件 / 类 | 未来归属 | 阶段 / 验证 |
| --- | --- | --- | --- |
| 页面、筛选、排序、显示窗口 | `libs/ui/pages/ShelfPage.*` | 页面 + DvdSceneAdapter / DvdShelfController | P3；比较作品顺序、筛选、分页刷新 |
| 数据绑定、封面缩略图和元数据 | `libs/ui/components/DvdShelfView.*`，`DvdShelfView` | DvdSceneAdapter / CoverTextureCache | P3；数据库变化、缺失图片与非 ASCII 路径 |
| DVD 几何、UV、封面和材质 | `resources/qml/dvd/Dvd.qml`，`resources/meshes/*.mesh` | GltfLoader / DvdCaseFactory | P2；离线导出真实网格为 GLB，保留 provenance 和 UV 测试 |
| 环境、相机、布局、虚拟化 | `resources/qml/dvd/dvd_scene.qml` | Camera / DvdShelfController | P3；1、10、100、500、1000 个合成对象 |
| 左键选择、右键旋转、滚轮横移/关盒、悬停、拾取 | `dvd_scene.qml` 的输入处理、`DvdShelfView` bridge | 相机与 DVD 交互控制器 / Raycaster | P3；逐项对照当前行为 |
| 选中移动、开合、盘面旋转、返回 | `dvd_scene.qml`、`Dvd.qml` | Animator + DVD 状态机 | P3；中断、快速切换、不漂移 |
| 爱心、编辑、删除、作品元数据、剧照与图谱覆盖层 | `DvdShelfView.*`、`dvd_scene.qml`、`FanartStripWidget` | 现有页面 + DVD 事件接口 | P3；稳定作品 ID、回调与位置投影 |
| 主题和整体页面 | `ShelfPage`、`DvdShelfView`、场景环境 | libs/ui / dvd_scene | P4；明暗主题、窗口尺寸、DPI |
| 其他 Quick 使用 | `libs/ui/pages/HomePage.cpp` 的 `QQuickWidget`、`resources/qml/shop/shop_viewport.qml` 和 `Darkeye_shop.qml` | 保留现有实现 | P5 前逐项审查，不能全局删除 Quick |
| GPU/Widgets 预热 | `resources/qml/graphics_prewarm_scene.qml`、现有启动路径 | 后续按实际需要调整 | 当前保持原样 |
| 截图/导出 | 本轮未确认正式 DVD 页有对外导出契约 | 待确认 | 不凭 demo 截图能力宣称正式导出等价 |

## 资产和依赖差异

- 原仓库没有 `.glb` / `.gltf`。现有 DVD 是 Qt Quick 3D 预转换的
  `back.mesh`、`spine.mesh`、`front.mesh`、`cD.mesh`，不能直接交给 glTF parser。
  本轮通过可选离线工具导出 `apps/render3d_demo/assets/dvd-case.glb`，V 转为 1-v，Qt ARGB 转为线性 glTF 因子。解析采用 cgltf v1.15/MIT，选择理由见其 third_party/README；原模型独立上游授权仍待确认。
- `Dvd.qml` 使用 `PrincipledMaterial`；封面通过 `textureSource` 注入，默认资源为
  `qrc:/assets/maps/0.png`。`dvd_scene.qml` 使用 `resources/hdr/lebombo_1k.hdr` 环境。
- `libs/ui/CMakeLists.txt` 与 `cmake/Dependencies.cmake` 仍引用 Quick 系列模块；
  图谱 `libs/graph_view` 的 QRhi 路径独立保留。本轮不触及这些使用点。
- 新库仅链接 Gui / Widgets（公开）和 GuiPrivate（私有），不依赖业务、SQL、QML。
  demo 独立构建路径见 `apps/render3d_demo/README.md`。

复查命令（仓库根目录）：

```powershell
rg -n 'QQuickWidget|QQuickView|QQml|Quick3D|View3D|PrincipledMaterial|PerspectiveCamera' libs apps resources
rg --files resources | rg '\.(qml|glb|gltf|mesh|hdr)$'
rg -n 'Qt6::(Qml|Quick|Quick3D|QuickWidgets)|qt_add_shaders|GuiPrivate' libs apps cmake
```

旧页面继续作为生产路径。当前不需要回滚开关，因为没有替换任何业务页面。

