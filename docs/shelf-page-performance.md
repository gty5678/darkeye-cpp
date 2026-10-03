# 书架页首次进入性能优化

本文记录 C++ 版本 QML 书架页已经实现的首次进入性能优化。优化目标是减少用户第一次点击“书架”后发生的同步工作，同时保持筛选、滚动、选中、开盒、光盘和关系图等行为不变。

## 问题拆分

书架页第一次打开时原本会集中执行以下工作：

1. 创建并初始化 `ShelfPage`、筛选控件和 `QQuickWidget`。
2. 查询作品以及多个筛选补全列表。
3. 从磁盘加载并解析 QML。
4. 首次初始化 Qt Quick 3D、QRhi、HDR 环境、网格、材质和着色器。
5. 一次创建完整可见窗口内的 DVD 委托。
6. 解码封面并将纹理上传到 GPU。

这些工作单项并不一定很慢，但集中在一次导航点击后会形成明显停顿。当前实现按照“提前准备、减少同步工作、分帧摊销、降低单项成本”的顺序处理。

## 1. 将书架 QML 编译为模块

`libs/ui/CMakeLists.txt` 使用 `qt_add_qml_module` 注册 `Darkeye.Shelf 1.0` 模块，包含：

- `dvd_scene.qml`
- `Dvd.qml`
- 书架使用的四个 SVG 图标

模块使用资源前缀 `/qt/qml` 和 `NO_PLUGIN`。运行时通过以下资源地址加载：

```text
qrc:/qt/qml/Darkeye/Shelf/dvd_scene.qml
qrc:/qt/qml/Darkeye/Shelf/Dvd.qml
```

这样可以让 Qt 在构建阶段生成 QML cache/AOT C++ 文件，避免第一次进入书架时再从源码目录读取和完整解释 QML，也避免发布目录与源码目录不一致导致的额外路径探测。

构建产物中可看到类似文件：

```text
Dvd_qml.cpp
dvd_scene_qml.cpp
```

同时删除了 `dvd_scene.qml` 中没有使用的 `QtQuick3D.AssetUtils` 和 `QtQuick3D.Helpers` import，减少无用模块解析和部署依赖。

## 2. 启动阶段预热真实图形路径

`Application::prepareGraphicsPrewarm()` 在主窗口第一次显示时创建一个临时图形预热容器，其中同时放置：

- 一个小尺寸 `QQuickWidget`，预热 Qt Quick 3D 书架渲染路径；
- 一个 `ForceViewRhiWidget`，保留原有关系图 QRhi 预热。

Quick 3D 预热场景位于 `resources/qml/graphics_prewarm_scene.qml`。它不是一个无关的最小三角形，而是使用生产环境中的真实资源和配置：

- 实际的 `Dvd.qml` 组件；
- `back.mesh`、`spine.mesh`、`front.mesh` 和光盘模型；
- 默认封面纹理；
- `lebombo_1k.hdr` 光照探针；
- 与书架一致的 MSAA、色调映射、环境光遮蔽、灯光和阴影路径；
- 将 DVD 设为展开状态，从而连同延迟创建的光盘和全部材质变体一起预热。

预热容器会进入顶层窗口的控件树并处理 10 帧，每帧间隔约 16 ms。这样 Windows 上的 QRhi 和 Quick 3D 能提交真实帧，而不是只构造对象却没有创建底层图形资源。完成后暂停关系图模拟并隐藏预热容器，避免它继续参与合成。

开发构建和安装构建的资源位置不同。预热逻辑优先使用运行时资源目录；如果其中不存在 `meshes/back.mesh`，则回退到编译时的 `DARKEYE_SOURCE_DIR/resources`。

## 3. 空闲时预加载真实书架页面

主窗口显示并完成首屏绘制后，`Application::run()` 使用 250 ms 单次定时器调用 `MainWindow::preloadShelfPage()`。

这里预加载的是之后导航真正使用的 `ShelfPage` 实例，而不是一次性的替身页面。预加载会完成：

- 页面创建；
- `ShelfPage::initialize()`；
- 筛选控件创建；
- 初始作品查询；
- `QQuickWidget` 和书架 QML 场景创建。

预加载不会切换 `QStackedWidget` 当前页，因此用户仍停留在启动后的作品页。第一次点击书架时直接复用这个实例，不再支付 `createPage` 和 `initialize` 的主要成本。

初始化耗时通过以下日志保留，便于真实数据库下继续采样：

```text
Shelf page preloaded in ... ms
```

## 4. 初始 DVD 委托分帧创建

`DvdShelfView` 不再一次创建整个可见窗口，而是分批增加渲染数量：

| 参数 | 当前值 | 含义 |
| --- | ---: | --- |
| `initialDvdBatchSize` | 18 | 第一批创建的 DVD 数量 |
| `dvdBatchSize` | 18 | 后续每批增加的数量 |
| 分批间隔 | 16 ms | 每帧最多推进一批 |
| `minimumDvdWindowSize` | 42 | 正常窗口的最低目标数量 |
| `fallbackDvdWindowSize` | 48 | 控件尺寸尚未稳定时的目标数量 |
| `dvdWindowSize` | 60 | 虚拟窗口上限 |
| `dvdWindowOverscan` | 8 | 视口两侧预留数量 |

目标窗口大小根据控件宽高比、60° 垂直视场角、0.25 相机距离和 0.0145 DVD 间距计算，并限制在 42 到 60 之间。这样窄窗口无需固定创建 60 个委托，宽窗口仍有足够的超扫区域。

窗口尺寸在控件完成 resize 后会重新评估。如果路由要求直接打开某个作品，`openWork()` 会先完成初始窗口人口，确保目标委托已经存在，避免优化破坏深链接和从作品页跳转到指定 DVD 的行为。

滚动时仍保持最多 60 个 DVD 的移动虚拟窗口，单次窗口平移最多 10 个索引，避免程序化大跳转一次重建全部委托。

## 5. 减少书架初始化数据库工作

### 5.1 补全数据延迟到后台加载

番号、女优、导演和男优四组补全数据不再在 `ShelfPage::buildUi()` 中同步查询。每个 `CompleterLineEdit` 保存一个加载函数，在真正需要补全数据时使用独立的只读 SQLite 连接执行查询。

独立连接很重要：SQLite/Qt SQL 连接具有线程归属，不能把界面线程创建的连接直接交给后台任务使用。

制作商、厂牌、系列和标签等创建筛选控件时必须立即使用的数据仍按原逻辑加载。

### 5.2 删除重复的数量查询

刷新书架时原本先执行一次 `COUNT`，随后再执行一次搜索查询。现在 `reloadDvdScene()` 返回已经取得的作品列表长度，`refreshData()` 直接使用这个数量更新“过滤总数”。

这使一次刷新只需要作品搜索查询，不再对同一筛选条件额外执行一次计数查询。

## 6. 控制封面解码和 GPU 纹理上传

### 6.1 根据像素尺寸决定是否生成缩略图

只看 JPEG 文件字节数不能判断解码和 GPU 成本：压缩率很高的图片可能文件很小，但像素尺寸仍然很大。当前实现先通过 `QImageReader::size()` 读取图片头：

- 最长边不超过 1024：直接使用原图；
- 最长边超过 1024：后台生成最长边为 1024 的缩略图；
- 无法读取尺寸：使用 320 KiB 文件大小阈值作为回退判断。

现场缓存检查中，466 张旧缩略图里有 448 张最长边为 2048。最长边从 2048 降到 1024 后，单张未压缩纹理的最坏像素量和 GPU 上传量约为原来的四分之一。

### 6.2 新缓存版本避免复用旧大图

缩略图缓存键包含版本和目标尺寸：

```text
v2_1024_<work-id>_<mtime-ms>_<source-size>.jpg
```

因此不会继续命中旧的 2048 像素缓存；源文件时间或大小变化后也会自动产生新缓存。

### 6.3 后台缩放和渐进替换

需要缩略图时，界面先使用默认封面占位，不在 GUI 线程解码大图。缩略图任务由最多两个工作线程处理，使用 `QImageReader::setScaledSize()` 在解码阶段限制尺寸，再以 JPEG 质量 88 写入临时缓存。

缩略图完成后不会在相机移动期间立即刷新 QML 纹理列表。结果先进入内存缓存，等相机停止后通过 50 ms 单次定时器合并刷新，减少滚动过程中反复同步场景和上传纹理。

当前缓存限制如下：

- 内存纹理 URL 缓存：320 项；
- 磁盘缩略图缓存：最多 900 个文件；
- 磁盘缩略图缓存：最多 900 MiB。

磁盘缓存清理已经移出 GUI 线程，由全局线程池执行，避免目录扫描阻塞页面预加载或第一次点击。

## 7. 延迟创建关闭状态不可见的光盘

关闭状态的 DVD 盒不会显示光盘，但原实现会为每个可见 DVD 都创建：

- 一个额外的光盘 `Model`；
- 两个额外的 `PrincipledMaterial`。

现在光盘由 `Loader3D` 管理，仅在以下情况激活：

- DVD 已展开；
- 正在执行关闭动画；
- 开合动画进度仍大于零。

这减少了首屏 18 个以及最终可见窗口最多 60 个委托的模型和材质对象数量，同时保证关闭动画开始后光盘不会提前销毁。

## 8. 用数学曲线替代每实例 Timeline 关键帧

原 `Dvd.qml` 的每个实例都会创建：

- 1 个 `Timeline`；
- 2 个 `KeyframeGroup`；
- 24 个 `Keyframe`；
- 2 个动画对象。

两组关键帧实际表达的是同一条 smoothstep 开合曲线。当前实现保留两个 `NumberAnimation`，使用闭式公式计算旋转角度：

```text
t = clamp((frame - 41.6667) / 458.3333, 0, 1)
curve = t² × (3 - 2t)
angle = -90° × curve
```

由此移除了每个 DVD 的 Timeline、2 个关键帧组和 24 个关键帧对象：

- 首批 18 个 DVD 少创建约 486 个 QML 对象；
- 60 个 DVD 窗口少创建约 1620 个 QML 对象。

开盒、关闭、半途反向以及关闭完成信号仍由原来的帧值和时长逻辑驱动。

## 9. 优化后的首次进入流程

当前流程如下：

```text
应用启动
  → 预热真实 Quick 3D / QRhi 图形路径
  → 显示初始页面
  → 250 ms 后创建并初始化真实 ShelfPage
  → 先创建 18 个 DVD 委托
  → 每 16 ms 增加 18 个，直到视口目标数量
  → 大封面在后台生成 1024 缩略图
  → 相机空闲后合并替换纹理
用户第一次点击书架
  → 直接显示已经存在的 ShelfPage 和 QQuickWidget
```

因此，第一次点击承担的主要工作从“创建页面、查库、解析 QML、初始化图形栈、创建全部委托并上传大纹理”缩小为“显示已预加载页面，以及完成尚未结束的少量场景同步”。

## 10. 回归保护和验证

`MainWindowSmokeTest` 增加了书架预加载覆盖，验证：

- 预加载后 `ShelfPage` 已创建；
- 页面已经完成初始化；
- 预加载不会改变用户当前页面；
- 之后通过作品详情跳转书架仍会正确导航。

本轮优化使用 Release 构建验证，并运行：

```powershell
ctest --test-dir build/windows-msvc-release -C Release `
  --output-on-failure -R "darkeye_(shelf_page|ui_smoke)_tests"
```

最近一次结果：

```text
darkeye_ui_smoke_tests      Passed
darkeye_shelf_page_tests    Passed
100% tests passed
```

## 11. 维护注意事项

- 新增书架 QML 或图标时，应同步加入 `darkeye_business_ui` 的 QML 模块资源列表，避免退回磁盘源码加载。
- 修改书架材质、HDR、灯光或模型后，应同步检查 `graphics_prewarm_scene.qml`，确保预热的仍是生产路径。
- 调整 DVD 间距、相机距离或视场角时，应同步检查 `targetWindowSize()` 的可见数量计算。
- 修改缩略图尺寸或编码规则时，应更新缓存版本前缀，防止旧缓存掩盖新策略。
- 不要在相机移动期间逐张推送缩略图结果，否则会重新引入滚动卡顿。
- 页面预加载必须复用导航使用的真实页面，并保持当前页面不变；不要创建用完即丢的第二套书架场景。

## 相关文件

| 文件 | 作用 |
| --- | --- |
| `apps/desktop/Application.cpp` | 图形预热、书架空闲预加载调度 |
| `apps/desktop/MainWindow.cpp` | 创建并初始化真实书架页面 |
| `libs/ui/CMakeLists.txt` | 书架 QML 模块和 AOT 资源配置 |
| `libs/ui/pages/ShelfPage.cpp` | 数据查询和补全加载优化 |
| `libs/ui/components/DvdShelfView.cpp` | 分批委托、虚拟窗口、封面缩略图与缓存 |
| `resources/qml/graphics_prewarm_scene.qml` | 真实 Quick 3D 预热场景 |
| `resources/qml/dvd/dvd_scene.qml` | 书架 3D 场景和委托虚拟化 |
| `resources/qml/dvd/Dvd.qml` | DVD 模型、延迟光盘和数学开合动画 |
| `tests/integration/MainWindowSmokeTest.cpp` | 预加载与导航回归测试 |
