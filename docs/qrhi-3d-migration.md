DarkEye 自研 QRhiWidget 3D 场景替代方案

文档定位：可执行技术设计与迁移任务清单（Implementation Plan / RFC）
建议仓库位置：docs/architecture/qrhi-3d-migration.md
项目：gty5678/darkeye-cpp
编写日期：2026-10-15
目标技术栈：C++20 / Qt 6.12.0 / Qt Widgets / QRhiWidget / QRhi / CMake
状态：待实施；文中所有新类、新目录均为建议设计，非现有代码已实现的声明。

0. 执行摘要

目标是在保持现有 DarkEye 业务层与 Qt Widgets UI 体系的前提下，用 QRhiWidget + 自研精简 3D Renderer 替换 DVD 陈列页面目前依赖的 QML / Qt Quick 3D。新引擎专注于 DVD 盒模型、封面纹理、基础 PBR、场景层级、镜头、交互拾取、过渡动画和大规模重复陈列，不尝试在初期复制完整的 Qt Quick 3D 功能。

架构原则：

QRhiWidget 仅是渲染显示和生命周期适配层，不是场景引擎本体。

通用 render3d 与 DVD 业务 dvd_scene 分离；不让 render3d 依赖 SQLite、作品数据结构或 Qt Widgets 页面。

CPU 场景/资产与 GPU 资源分离；GPU 资源绑定当前 QRhi*，支持 releaseResources() 后重建。

先做功能等价替换，再做优化；旧 QML 页面保留到新页面通过验收。

初期仅保证 Windows + Direct3D 11；Vulkan / Metal / OpenGL 的兼容性作为后续阶段性验证项。

优先实现简单、可测、可复用的 Forward Renderer；暂不引入 ECS、物理引擎、复杂 Render Graph 或完整编辑器。

最终交付物

libs/render3d：与业务解耦的 3D 场景、资产、GPU 渲染及交互基础库。

libs/dvd_scene：DVD 陈列控制器、封面管理、DVD Widget 和动画交互。

示例程序：独立 QRhiWidget 演示（网格/GLB/交互/压力测试）。

自动化测试及人工回归检查清单。

主应用切换新实现；确认不再需要后，移除相关 QML/Qt Quick 3D 依赖。

文档：架构、资源生命周期、材质与颜色约定、性能基线和问题排查指南。

1. 代码现状与迁移前盘点

1.1 已从公开仓库配置核实的情况

已确认事实

代码位置

对应行动

主技术栈包含 C++20 / Qt 6，顶层有 libs/、apps/、tests/ 和 docs/

仓库 README 与目录

按现有多库布局扩展

libs/graph_view 为独立目标，使用 Qt6::GuiPrivate、qt_add_shaders()

libs/graph_view/CMakeLists.txt

可参考 Shader/CMake/QRhi 构建方式，不直接复用图谱业务渲染实现

libs/ui 的 darkeye_business_ui 目标仍链接 Qt6::Qml、Qt6::Quick、Qt6::Quick3D、Qt6::QuickWidgets

libs/ui/CMakeLists.txt

按使用点逐个迁移后移除依赖

cmake/Dependencies.cmake 使用 find_package(Qt6 6.12.0 ...)，包含 ShaderTools 和上述 Quick 模块

cmake/Dependencies.cmake

第一阶段沿用现有 Qt 环境，不提前删包

公开功能说明有拟物化 DVD 陈列、关系图谱，3D 场景目前还未完全适配明/暗主题

README

把场景视觉和主题兼容列入回归测试

尚未核实、必须在 P0 阶段确认： DVD 页面现有 QML/GLB 的具体路径、资源复用方式、动画/交互状态、数据绑定入口、是否存在仍需要 Qt Quick 的其他页面。

1.2 盘点任务（P0，必须优先完成）

在仓库根目录执行（PowerShell / Bash 均可使用 rg；没有 rg 可改用 IDE 全局搜索）：

rg -n 'QQuickWidget|QQuickView|QQml|Qml|Quick3D|View3D|Model\s*\{|PrincipledMaterial|PerspectiveCamera' libs apps resources
rg --files | rg '\.(qml|glb|gltf|vert|frag|qsb)$'
rg -n 'Qt6::(Qml|Quick|Quick3D|QuickWidgets)|qt_add_shaders|GuiPrivate' . --glob 'CMakeLists.txt' --glob '*.cmake'

制作迁移矩阵（每条旧功能都需填写当前具体类/文件）：

现有功能

旧实现位置（待补）

新归属

验收行为

DVD 盒模型与封面

TBD

GltfLoader / DvdCaseFactory

外形、UV、封面位置正确

陈列顺序、布局

TBD

DvdShelfController

与现有顺序一致

鼠标/滚轮/双击

TBD

OrbitCameraController + DvdSceneWidget

输入行为与旧版等价

选中/高亮/聚焦

TBD

Raycaster + DvdInteractionController

选中对象正确

盒子运动/镜头过渡

TBD

Animator

支持中断与平滑恢复

筛选与数据库变更

TBD

现有数据服务 + DvdSceneAdapter

切换/更新后画面一致

主题及页面集成

TBD

libs/ui / dvd_scene

与 Widget UI 样式协调

截图及导出（如存在）

TBD

DvdSceneWidget

尺寸、颜色、透明度符合预期

P0 完成标准：形成“旧功能 → 旧文件/类 → 新模块 → 优先级 → 测试方法”的完整清单；保留至少 5 个旧版基线场景截图，并记录交互录屏、GPU 后端、屏幕 DPI、分辨率和 DVD 数量。

2. 范围和非目标

2.1 必做（MVP）

层级场景（Node / Transform）、Camera、Mesh、Material、Texture。

GLB / glTF 2.0 静态网格导入：Node、Mesh、Index、法线、UV、基础材质及贴图。

基础渲染：不透明、透明、深度测试、简单光照、色彩空间正确处理。

鼠标交互：旋转/平移（依旧版行为）、缩放、DVD 点击选择、双击聚焦。

过渡动画：Transform 插值、动画覆盖与中断、恢复原位置。

DVD 业务场景：数据库数据映射为独立 DVD 节点；封面纹理加载与缓存。

Windows D3D11 正常运行；高 DPI、缩放、窗口拖拽与关闭安全。

2.2 第二阶段

Metallic-Roughness PBR 完善、法线贴图、环境光照 IBL、简单阴影或接触阴影。

Frustum Culling、封面 LRU 缓存、多分辨率纹理、实例化/批处理。

后端交叉测试（至少 Vulkan；如支持 macOS 则 Metal）。

统计面板：CPU/GPU 帧时间、Draw Calls、纹理与 Buffer 估算。

2.3 暂不实施

复杂骨骼动画、Morph Target、物理碰撞和刚体模拟。

通用场景编辑器、脚本运行时、ECS、完整 Render Graph。

完整 glTF 扩展生态（KTX2/BasisU/Draco/meshopt 等可评估后再做）。

多线程独立渲染循环：第一版遵循 Qt Widget/QRhiWidget 的线程与执行模型。

立即重构现有 graph_view：先保留独立目标，避免迁移风险放大。

3. 建议模块及依赖方向

DarkEye Application / libs/ui (现有 Qt Widgets 页面)
                      |
                      v
               libs/dvd_scene
               - DvdSceneWidget
               - DvdShelfController
               - DvdCaseFactory
               - DvdInteractionController
               - CoverTextureCache
                      |
                      v
                libs/render3d
      +----------------+-------------------+
      |                                    |
  Scene / CPU Assets                  Renderer / GPU
  Scene, Node, Camera                RhiSceneWidget
  Mesh, Material, TextureData        Renderer, GpuResourceCache
  GltfLoader, Animator               RenderQueue, Pipelines
  Raycaster                          Shaders, UploadQueue
      |                                    |
      +----------------+-------------------+
                       |
                  Qt Gui / QRhi
                       |
             D3D11, Vulkan, Metal...

libs/graph_view (现有): 独立保留；暂不依赖 render3d。

依赖约束：

render3d 不依赖 dvd_scene / ui / 数据库。

dvd_scene 依赖 render3d 和经过适配的数据接口；尽量避免直接把数据库对象传入渲染器。

libs/ui 持有 DvdSceneWidget，负责页面级数据刷新与用户命令。

所有 Qt QRhi 头文件和 Qt6::GuiPrivate 依赖都限制在 render3d 内部实现，尽量不泄漏到公共头文件。

可在成熟后抽取基础渲染工具与 graph_view 共享，但不是迁移的前置条件。

3.1 目标目录树（建议，尚未存在）

libs/
├── graph_view/                         # 现有，保持不变
├── render3d/                            # 新增：通用模块
│   ├── CMakeLists.txt
│   ├── include/render3d/
│   │   ├── Scene.h
│   │   ├── Node.h
│   │   ├── Transform.h
│   │   ├── Camera.h
│   │   ├── Mesh.h
│   │   ├── Material.h
│   │   ├── ModelAsset.h
│   │   ├── GltfLoader.h
│   │   ├── Animator.h
│   │   ├── Raycaster.h
│   │   └── RhiSceneWidget.h
│   ├── src/
│   │   ├── scene/
│   │   ├── assets/
│   │   ├── animation/
│   │   ├── interaction/
│   │   └── renderer/
│   │       ├── Renderer.cpp
│   │       ├── RenderQueue.cpp
│   │       ├── GpuResourceCache.cpp
│   │       ├── ShaderLibrary.cpp
│   │       └── RhiSceneWidget.cpp
│   └── shaders/
│       ├── unlit.vert
│       ├── unlit.frag
│       ├── pbr.vert
│       └── pbr.frag
├── dvd_scene/                           # 新增：产品业务模块
│   ├── CMakeLists.txt
│   ├── include/dvd_scene/
│   │   ├── DvdSceneWidget.h
│   │   ├── DvdSceneAdapter.h
│   │   ├── DvdShelfController.h
│   │   ├── DvdCaseFactory.h
│   │   └── CoverTextureCache.h
│   └── src/
└── ui/                                  # 现有业务页面

tests/
├── render3d/
└── dvd_scene/

apps/                                    # 在现有 apps 组织方式中新增 demo target

3.2 推荐的关键公共 API（接口草案，不是完整可编译实现）

// 渲染无关：CPU 场景层
using NodeId = std::uint64_t;

struct Transform {
    QVector3D translation{0, 0, 0};
    QQuaternion rotation{};
    QVector3D scale{1, 1, 1};
};

class Scene {
public:
    NodeId createNode(NodeId parent = 0);
    bool removeNode(NodeId id);
    void setTransform(NodeId id, const Transform& transform);
    void setMesh(NodeId id, MeshHandle mesh, MaterialHandle material);
    // 支持按需更新 world transform 和收集可见对象
};

struct ModelAsset {
    // CPU-side node hierarchy, mesh data, materials, image data
};

class GltfLoader {
public:
    LoadResult<ModelAsset> load(const std::filesystem::path& path);
};

// 业务层独立的场景显示适配
class RhiSceneWidget : public QRhiWidget {
protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
private:
    std::unique_ptr<Renderer> renderer_;
};

说明：MeshHandle、MaterialHandle、LoadResult<T>、Renderer 等为下一步定义的占位类型。禁止把本节直接复制当成可编译代码。具体 public/private 头文件安排和 NodeId 分配策略在 P1 锁定。

3.3 数据层契约（建议）

struct DvdItemViewModel {
    std::string stableId;           // 数据库作品 ID 的稳定表示
    std::filesystem::path coverPath;
    std::string title;
    std::size_t displayOrder = 0;
    bool selected = false;
};

此结构由 DvdSceneAdapter 负责组装，3D 引擎只接收布局与渲染参数。定义增删改/排序的增量更新接口，避免数据库每次变化都重建完整场景。

4. 渲染管线设计

4.1 第一版：Forward Renderer

输入事件 / 更新业务数据
        |
动画更新 -> Node Transform Dirty Propagation
        |
获取 Camera View / Projection
        |
可见性收集（第一版可先不做 Frustum Culling）
        |
RenderQueue (opaque / transparent)
        |
GPU Upload Queue (Buffer / Texture / Uniform)
        |
Opaque Pass: depth write ON, blending OFF
        |
Transparent Pass: depth test ON, depth write OFF, back-to-front
        |
Selection Overlay (可在早期仅通过替换材质高亮)
        |
QRhiWidget 自动管理的 RenderTarget -> Qt Widgets 合成

具体约定：

坐标与相机：统一右手/左手业务空间约定，屏幕裁剪差异使用 QRhi::clipSpaceCorrMatrix() 处理，不在业务层硬编码 D3D/OpenGL 分支。

UV 与贴图：在导入转换层统一方向约定，验证各后端纹理翻转情况。

材质：起步只做 Unlit 和 MetallicRoughness；明确 alpha OPAQUE/MASK/BLEND；透明对象按深度排序。

颜色管理：sRGB 颜色纹理解码到线性空间计算；normal、metallic、roughness、occlusion 视为线性数据；最终输出编码与 Widget RenderTarget 格式一致。

Shader：Vulkan 风格 GLSL 经 qt_add_shaders() 编译为 .qsb；运行时读取 QShader::fromSerialized()；在 ShaderLibrary 内集中管理。

Pipeline Cache：Key 至少包括 shader variant、顶点布局、透明与剔除状态、深度状态、sample count 和 render pass compatibility。

光照：先做 Ambient + Directional，必要时加少量点光源；PBR 高级反射、IBL 和阴影放在功能等价后。

4.2 QRhiWidget 生命周期——不得省略

RhiSceneWidget 要遵守 Qt 官方生命周期：

initialize(cb)：获取 rhi()；发现 QRhi* 更换则清理旧 GPU 缓存；确保 shader/pipeline/buffer/texture 与当前 RenderTarget 匹配。

render(cb)：处理已完成的 CPU 资产上传；通过 QRhiResourceUpdateBatch 更新资源；beginPass(renderTarget(), ...) → setGraphicsPipeline → setShaderResources → setVertexInput / draw → endPass()。

releaseResources()：清理所有依赖当前 QRhi* 的 GPU 对象并使缓存失效；CPU 场景资产保留，以便重建。

resize / DPR 变化：依据实际颜色缓冲像素尺寸更新相机投影矩阵、viewport、必要的尺寸相关 GPU 资源。

sampleCount / 颜色格式变化：必要时重建不兼容的 Pipeline，而不是仅调整 uniform。

reparent：顶层窗口变化可能导致 QRhi 更换，必须重新创建 GPU 对象。

同一顶层 Widget 窗口中的 QRhiWidget / QQuickWidget 必须使用兼容的同一图形后端；并行迁移期间重点测试。

初期不要在 render() 尾部无条件 update()。使用 QTimer 或事件驱动，仅在镜头/动画、纹理上传、场景变化时请求新帧。无变化时静止渲染。

4.3 CPU/GPU 资源隔离与多实例

资源类别

所属层

生命周期

创建/更新策略

Node、Transform、CPU Mesh

Scene / Asset

按场景/共享资产

主线程或者受控 CPU 后台准备

原始图片、解码后 Image

Asset / CoverCache

LRU/引用计数

后台解码，完成后投递给渲染适配层

QRhiBuffer、QRhiTexture、QRhiSampler

GpuResourceCache

绑定当前 QRhi

由当前 QRhi 渲染执行上下文创建/更新

QRhiShaderResourceBindings、QRhiGraphicsPipeline

PipelineCache

绑定当前 QRhi 和 Pass

变更后重建

视图控制与动画时间轴

Controller

随 Widget/Scene

CPU 更新，按需 requestFrame

不要从任意后台线程直接调用 QRhi 资源创建或上传接口。禁止 GPU Resource 持有指向已经释放的 QRhi*、RenderTarget 或 Renderer 的悬垂指针。

5. glTF / GLB 导入设计

推荐优先评估 fastgltf（C++20 适配良好），但须在 P2 测试现有实际 GLB 资产后才能确定最终依赖。它负责解析，不负责帮我们生成 QRhi 绘制对象。

数据流：

.glb / .gltf
    -> GltfLoader (fastgltf)
    -> ModelAsset (CPU mesh/material/image/nodes)
    -> GPU Upload Queue
    -> GpuResourceCache (QRhiBuffer / Texture / Sampler)
    -> Scene node instance / RenderItem

P2 首轮最低支持面：

.glb Binary Container 与 .gltf 外部资源路径解析。

Indexed triangles；处理 primitive 分拆与必要的顶点属性映射。

POSITION、NORMAL、TEXCOORD_0；没有索引或法线时按既定策略处理。

Node 层级、局部 Transform、世界矩阵、多个 Mesh Primitive。

baseColorFactor / baseColorTexture、metallic/roughness 因子及关联贴图。

纹理采样设置、图片格式、UV 方向、sRGB/linear 区分。

明确未支持项的错误日志和回退材质，不允许静默渲染错误。

后续支持项： tangents/normal map、occlusion、emissive、alpha mask、double-sided、glTF animation、必要的扩展。若现有模型依赖这些项，按盘点结果提前到 MVP。

Asset cache Key：优先使用规范化路径 + 文件修改时间/内容版本 + loader options；同一模型加载 N 次时 CPU mesh 和 GPU geometry 不应重复上传 N 份。

6. DVD 场景实现

6.1 业务对象与交互状态机

DvdSceneWidget (业务 Widget)
  |- DvdShelfController   排列/分页/显示队列
  |- DvdCaseFactory       实例化共享 DVD mesh + 封面材质
  |- CoverTextureCache    图片解码、缓存、降级图
  |- DvdInteractionController
  |      Idle -> Selected -> Focusing -> Focused -> Returning -> Idle
  `- RhiSceneWidget (渲染适配)

事件契约建议：

dvdClicked(stableId)：业务页面处理跳转或元数据面板。

dvdDoubleClicked(stableId)：聚焦或打开，行为与旧版一致。

selectionChanged(optionalId)：驱动业务 UI 选择状态。

setItems(span<DvdItemViewModel>) / updateItem(...)：与渲染解耦的数据接口。

6.2 Camera / Picking

Camera 使用 target + distance + yaw + pitch 描述 Orbit 状态，统一视图矩阵的计算。

将鼠标逻辑像素转换到 framebuffer 像素/Normalized Device Coordinates；使用相机 view/projection 逆矩阵构造射线。

第一版 CPU Ray vs AABB/OBB，返回最近相交 DVD 的稳定 NodeId；对于倾斜盒体可把射线转到节点局部坐标再与 AABB 求交。

不做 GPU ID buffer 作为前置任务；如果实际误判明显，P5 引入 BVH / triangle intersection。

6.3 动画

时间轴基于单调时间、使用 delta time 推进，避免按每帧固定角度更新。

支持 translation、rotation（Slerp）、scale 和 easeInOut。

支持中断/反转/新的聚焦目标，避免快速点击导致盒子错位。

业务状态机与纯数学动画轨道解耦；动画结束回调不直接写数据库。

6.4 纹理与大量 DVD 的性能优化

共享几何：相同 DVD Case 只创建一套 Mesh Buffer；每个对象只保留 Transform、封面索引及实例属性。

纹理按需加载：可见、即将出现、选中的 DVD 优先；不可见封面允许淘汰。

LRU 缓存：为纹理内存设置可配置预算，定期记录命中率与估算占用。

分辨率分级：远景低分辨率、选中或近景高分辨率；CPU 解码与 GPU 上传分离。

Draw Call 优化：共享 Mesh 不能消除不同纹理绑定造成的 Draw Call；先测量，必要时改 Texture Array 或 Atlas + instancing。

按需渲染：静态书架不持续重绘；只在输入、动画和数据更新时 requestFrame。

实例更新：仅更新 dirty 实例的 Transform / 参数，优先避免全量 rebuild。

容量估算示例：500 张 512×512 RGBA8（未压缩）封面，连 Mipmap 约占 667 MiB 显存，尚未包含其他纹理和 Buffer。实际应根据 GPU 预算动态控制缩略图尺寸与保留数量。

7. CMake 接入策略

现有 libs/graph_view/CMakeLists.txt 的 QRhi / Shader Tools 接法可作为风格参照。以下为示意骨架；在仓库中创建实际文件时要补齐真实源文件、include、依赖和 compiler options。

libs/render3d/CMakeLists.txt：

qt_add_library(darkeye_render3d STATIC
    src/renderer/Renderer.cpp
    src/renderer/RhiSceneWidget.cpp
    src/scene/Scene.cpp
    src/assets/GltfLoader.cpp
)

add_library(darkeye::render3d ALIAS darkeye_render3d)

set_property(TARGET darkeye_render3d PROPERTY AUTOMOC ON)

target_include_directories(darkeye_render3d PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(darkeye_render3d
    PUBLIC Qt6::Gui Qt6::Widgets
    PRIVATE Qt6::GuiPrivate
)

qt_add_shaders(darkeye_render3d darkeye_render3d_shaders
    PREFIX "/render3d/shaders"
    FILES
        shaders/unlit.vert
        shaders/unlit.frag
        shaders/pbr.vert
        shaders/pbr.frag
)

darkeye_apply_compiler_options(darkeye_render3d)

实施步骤：

在管理子目录的实际顶层 CMake 中添加 add_subdirectory(libs/render3d)，确保定义顺序早于使用者。

P2 确定 fastgltf 引入方法（优先与仓库当前依赖体系一致，如 vcpkg）；审查许可证和静/动态链接约束。

新增 libs/dvd_scene，链接 darkeye::render3d，让 libs/ui 链接 darkeye::dvd_scene。

保留现有 Qt6::Qml/Quick/Quick3D/QuickWidgets 到完全清理阶段，再逐项移除。

Qt 版本升级前，先运行 GUI 渲染回归测试；QRhi 属于兼容性保证较弱的 API。

辅助命令（以仓库实际 preset 名称为准）：

cmake --list-presets
cmake --preset <your-configure-preset>
cmake --build --preset <your-build-preset>
ctest --test-dir <your-build-dir> --output-on-failure

如果现有 CMakePresets 仅定义 configurePreset 而没有 buildPreset，可改用：

cmake --build <your-build-dir> --config Debug

8. 实施里程碑与任务拆解（按依赖顺序执行）

每个阶段建议单独提交 PR/Commit，前一阶段验收通过再进入下一阶段。表中的“范围”比预估工期更可靠，不承诺固定耗时。

P0 — 基线盘点与验收资产

P0-01 搜索并登记现有 QML/Quick3D 文件、GLB、贴图和 CMake 依赖。

P0-02 列出现有 DVD 交互、动画、点击、筛选、主题行为。

P0-03 保留可重复运行的 DVD 样本：1、10、100、500、1000 件（不够时生成合成数据）。

P0-04 记录老版本截图、体验视频、FPS/CPU/GPU/显存、窗口尺寸/DPI。

P0-05 明确资源许可证、分发路径和导入路径。

验收：完成迁移矩阵和截图数据；明确哪个旧页面将被替换以及回滚开关位置。

P1 — QRhiWidget 最小渲染 Demo

P1-01 新增 darkeye_render3d target 与独立 QRhiWidget demo target。

P1-02 实现 RhiSceneWidget 生命周期及空场景清屏。

P1-03 实现 .qsb Unlit shader、Vertex/Index Buffer、GraphicsPipeline、Uniform Buffer。

P1-04 显示旋转立方体，启用 DepthTest，窗口自适应 aspect ratio。

P1-05 支持 resize、DPI 变更、hidden/show、窗口 reparent 和安全退出。

P1-06 加入调试日志：后端、surface size、sampleCount、QRhi 重建、shader/pipeline 创建失败。

验收：Windows D3D11 下立方体正确绘制；多次 resize/reparent 不崩溃、不黑屏、不出现明显的持续 GPU 资源增长。

P2 — CPU 场景与 GLB 静态模型

P2-01 实现 Scene/Node/Transform/Camera 与 NodeId。

P2-02 实现 Mesh/Material/TextureData/ModelAsset，分离 CPU/GPU 结构。

P2-03 选择 glTF loader 并实现 GLB 静态 Mesh 转换。

P2-04 支持 BaseColor Texture / Unlit、纹理采样、深度绘制。

P2-05 显示仓库当前实际使用的 DVD GLB 盒模型及封面。

P2-06 编写模型变换、UV、错误文件和资源重复加载测试。

验收：真实 DVD 模型正确显示；贴图方向、尺寸、法线、场景层级与旧版匹配；加载失败有可定位日志。

P3 — DVD 交互与场景集成（MVP）

P3-01 创建 libs/dvd_scene 与 DvdSceneWidget。

P3-02 引入 DvdItemViewModel、DvdSceneAdapter，对接现有业务数据。

P3-03 完成 DVD 排列、选中/高亮、事件回传。

P3-04 完成 Orbit/Pan/Zoom 和 CPU Picking。

P3-05 实现聚焦、返回、选择切换动画状态机。

P3-06 完成封面加载、图片缺失占位、路径失效处理。

P3-07 在业务页面使用特性开关选择新旧 3D Widget。

验收：核心 DVD 交互与数据刷新等价；界面不需要修改数据库业务逻辑；可以一键回退旧版 QML。

P4 — 视觉一致性与性能

P4-01 加入 Metallic-Roughness PBR、方向光和环境光。

P4-02 比较旧版截图（同一相机角度、同一模型与纹理）。

P4-03 封面多分辨率加载、LRU、GPU 缓存预算。

P4-04 加入可见性裁剪、共享 Mesh / Material，按数据决定是否实现 instancing。

P4-05 引入实时统计与测试场景（100/500/1000 DVD）。

P4-06 明暗主题、2K/4K、高 DPI、最小窗口及异步加载测试。

验收：无明显交互卡顿、显存随不可见资源淘汰后回落；性能不低于旧方案的可比测量基线（相同设备/分辨率/测试场景）。

P5 — 替换、清理、回归

P5-01 新 Widget 成为默认路径；保留一轮回退能力。

P5-02 搜索所有残余 QQuickWidget / QML / Quick3D 使用点，并确认是否仍为其他页面所需。

P5-03 删除已经完全无使用者的 QML 资源、CMake 依赖、打包复制步骤。

P5-04 清理旧事件绑定与过渡代码，补齐开发文档。

P5-05 Windows 发布构建与打包验证；按项目支持平台进行图形后端验证。

P5-06 完整 GUI 回归和稳定性测试；撤销不必要的临时 feature flag。

验收：DVD 页面不再引用 Qt Quick 3D；主程序无因迁移新增的资源泄漏或崩溃；若其他页面仍需 Quick，保留相应 Qt 模块而非粗暴删除。

9. 测试矩阵与质量门槛

9.1 自动测试

测试

内容

预期

TransformTests

父子变换、旋转/缩放、Dirty propagation

世界矩阵正确

CameraTests

aspect ratio、投影与逆投影、射线生成

同点投影/反投影误差可控

GltfLoaderTests

glb、缺纹理、错误索引、多 primitive

成功或明确失败，不崩溃

RaycastTests

命中、漏检、最近对象、旋转盒体

返回正确 NodeId

AnimatorTests

插值、终值、中断、快速切换

无位置漂移

AssetCacheTests

引用共享、替换资源、LRU 淘汰

可控生命周期

9.2 GUI/集成回归

空场景、只有一个 DVD、100 / 500 / 1000 DVD。

调整窗口尺寸、最小化恢复、隐藏显示、跨屏拖动、DPR/分辨率变化。

Widget 销毁重建、不同顶层窗口间 reparent、MSAA/颜色格式变化。

连续点击/双击、滚轮、拖拽、动画中断和替换数据。

缺失封面、损坏贴图、含非 ASCII 字符路径、无效 GLB。

高比例封面异步上传时保证 GUI 响应。

与已有图谱和 QQuickWidget 混合使用时图形后端不冲突。

至少一个发布构建 smoke test 与关机退出路径检查。

9.3 性能记录模板

请在同一设备、同一视角、同一分辨率、同一测试数据下分别测量旧版和新版。

版本/后端

数量

分辨率/DPR

平均 FPS

P95 帧耗时

CPU

GPU/显存

Draw Calls

备注

旧版 Quick3D

100

TBD

TBD

TBD

TBD

TBD

TBD

基线

新版 D3D11

100

TBD

TBD

TBD

TBD

TBD

TBD

P3

新版 D3D11

500

TBD

TBD

TBD

TBD

TBD

TBD

P4

新版 D3D11

1000

TBD

TBD

TBD

TBD

TBD

TBD

P4

目标不是仅追求高 FPS：静止时应停止主动重绘，交互时应平滑，显存应处于预算内，图片加载不应造成长时间主线程阻塞。

10. 技术风险及回退策略

风险

影响

控制措施

Qt QRhi 接口有限兼容性保证

Qt 小版本升级可能破坏编译/运行

版本锁定、更新时进行编译与运行回归

Quick3D 与自研渲染外观差异

模型看起来不一致

保留基线截图，验证颜色空间/法线/光照/Alpha

QRhi 被销毁或替换

黑屏、GPU 崩溃

releaseResources() 与 GPU 资源重建机制

复杂 glTF 扩展

模型导入不完整

先盘点实际 GLB 能力，按需扩展 importer

海量封面 GPU 内存膨胀

卡顿、OOM、驱动 reset

LRU、纹理降级、预算控制、渐进上传

透明材质排序错误

盒体/封套穿插不正确

区分 opaque/transparent，透明背向排序

同一顶层窗口混用多种图形 API

Widget 不渲染

统一 API 配置，保留混用场景测试

一次性删除 QML

功能不可回退

新旧路径并行，最终验收后再清理

新模块过度抽象

项目周期膨胀

每阶段以运行中的 DVD 结果为完成标志

建议回滚方案：P3–P4 使用编译选项或运行时 feature flag DARKEYE_EXPERIMENTAL_RHI_DVD（名称可按项目习惯调整），默认生产路径仍可切回旧版；P5 确认稳定后删除旧实现。

11. 第一轮开发任务（可直接转为 Issues）

推荐从以下 8 项开始；每项结束都应有可运行结果，不必先把所有头文件一次建完。

RHI3D-001：建立迁移盘点文档
输出 QML/GLB/业务映射清单、旧版截图和复现步骤；不可在盘点前删除旧实现。

RHI3D-002：render3d + 最小 QRhiWidget demo
新增库及独立 Demo，清屏和显示彩色三角形，打印当前 backend/size。

RHI3D-003：顶点/索引/Uniform Buffer、深度测试、相机
渲染能旋转的立方体，正确响应 resize/DPR。

RHI3D-004：生命周期与 GPU cache
initialize/render/releaseResources 完整支持销毁、重建和窗口 reparent。

RHI3D-005：CPU Scene 与模型资源结构
Node、Transform、Mesh、ModelAsset，不将 QRhi 对象嵌入场景树。

RHI3D-006：GLB 导入
接入解析库并成功渲染当前 DVD 盒的几何与 UV。

RHI3D-007：纹理与基础材质
支持封面贴图、采样、sRGB、基本照明和可诊断错误。

RHI3D-008：单个 DVD 交互 Demo
鼠标选中、镜头旋转、点击聚焦、恢复动画；由此进入 P3 批量业务集成。

首个可合并 PR 的 Definition of Done

仅引入 render3d 库、demo 目标及相关测试文件，不改变正式 DVD 页面。

支持 D3D11，绘制可旋转立方体；有可复现的运行说明。

Shader 使用 qt_add_shaders，无运行时手工依赖外部 .vert/.frag 文件。

resize/DPR/Widget 重新创建后仍正确；GPU 对象能够释放。

Debug 和 Release 均能编译；不引入额外 QML 依赖。

CMake 目标按现有命名/编译选项组织。

12. 决策日志（ADR）

ID

决策

理由

重新评估条件

ADR-001

选择 QRhiWidget 作为 Qt Widgets 内嵌的 3D 渲染容器

与现有 UI 架构一致

出现不可接受的纹理合成开销

ADR-002

使用专用 Forward Renderer

DVD 场景需求明确、复杂度可控

多光源/后处理成为刚需

ADR-003

使用层级场景树而非 ECS

层级相机、书架、DVD 直观易维护

CPU 场景规模大到成为瓶颈

ADR-004

优先 CPU Ray Picking

常规 DVD 盒适用、调试简单

复杂形状或大量对象拾取性能不足

ADR-005

GPU/CPU 资源分离

QRhi 生命周期重建与跨后端兼容

不建议撤销

ADR-006

glTF 使用独立解析库

避免手写规范解析

需要特殊扩展、依赖评估不通过

ADR-007

不立即重构 graph_view

降低两个功能同时回归的风险

新库稳定后发现可复用的共性

ADR-008

旧版 QML 暂时保留

便于视觉对照与回滚

P5 全面验收通过

13. 参考资料

DarkEye GitHub：https://github.com/gty5678/darkeye-cpp

现有图谱 CMake：https://github.com/gty5678/darkeye-cpp/blob/main/libs/graph_view/CMakeLists.txt

现有 UI CMake：https://github.com/gty5678/darkeye-cpp/blob/main/libs/ui/CMakeLists.txt

Qt 依赖声明：https://github.com/gty5678/darkeye-cpp/blob/main/cmake/Dependencies.cmake

Qt 官方 QRhiWidget：https://doc.qt.io/qt-6/qrhiwidget.html

Qt 官方 Shader Tools：https://doc.qt.io/qt-6/qtshadertools-index.html

fastgltf：https://github.com/spnda/fastgltf

注意：文档中的性能目标、接口结构和里程碑是实施设计，不代表已经编译或运行验证。实际代码开发应先完成 P0 和 P1，再逐阶段补齐与真实 GLB、QML 及业务数据的差异。