# 原生 QRhi 书架接入

2026-10-23，按用户要求将主程序书架页替换为已验证的 QRhi DVD 场景。

## 运行入口

VS 的 `release-data` 配置启动 `Darkeye`，进入“书架”。可执行文件为 `build/windows-msvc-release-data/Darkeye.exe`；独立的 `darkeye_dvd_scene_demo` 与 `darkeye_render3d_demo` 继续保留。

`DvdShelfView` 现在使用 `dvd_scene::DvdSceneWidget`，不再创建 `QQuickWidget`、QML context 或 DVD delegate。DVD QML 与旧预热场景已移出主程序资源列表，旧源文件保留供基线对照。首页仍使用原来的场景，本次替换范围为书架。

## 模型与数据

- 主程序资源 `:/assets/dvd/dvd.glb` 来自 `libs/dvd/dvd.glb`，替换模型后重新构建即可更新。
- 主程序和 demo 共用 `DvdModelAsset.h` 中的载入适配：保持 `back → spine → front`、独立 `disc` 的结构和导出材质，将 `pic` 在内存中映射到可替换封面的 `cover`。
- 保留现有数据库查询、排序和筛选。作品 ID 直接作为场景稳定 ID；封面相对路径由封面目录解析后交给异步贴图缓存，不再经 QML URL 与 delegate 索引转发。
- 常规移动窗口最多 60 件，保留选中与动画中的实例。CPU/GPU 封面预算各为 128 MiB；背景为 HDR，启用 IBL、AO、方向光阴影、色调映射及 4× MSAA。
- 文件中的动画仍不播放；悬浮、抽出、开盒和归位由交互控制器驱动。

## 原生交互

单击抽出，再次单击开合；右键拖动旋转，中键平移，滚轮横移，Ctrl+滚轮缩放，Esc 归位。展开后点光盘或右侧播放按钮可播放已有本地视频。

按用户修正，书架不显示右侧详情栏，选中作品不会改变场景宽度。收藏、复制番号、编辑、删除、播放和归位可在选中作品的右键菜单中操作；开盒后的盒脊另保留原页面的收藏、编辑和删除按钮；右键拖动仍控制相机，不弹菜单。关系图与剧照继续使用原生控件。跨页定位使用稳定 ID 并在聚焦完成后展开，不再依赖延迟修改 QML delegate。

开盒后盒盖内部的作品信息层使用原生标签、按钮、流式布局和滚动容器：标题、简介、番号、日期、演员、导演、片商、厂牌、系列和标签来自原有数据库查询；演员、标签和制作信息保留原信号跳转，番号可点击复制。按旧 QML 保留 18 像素标题、14 像素正文、8 像素分组间距、4 像素圆角徽章间距；按用户进一步澄清，整块信息容器和收藏/编辑/删除操作栏的大底透明；演员小按钮恢复白底，标签小按钮保持数据库颜色。信息文字使用深色以适配白色盒壳。该控件不加入页面布局，不占用右侧空间；以选中实例 `front` 的封面网格顶点投影定位，仅开盒动画完成后显示，合盒和归位时立即隐藏。相机、窗口或 DVD 变换时重新投影。标题最多两行、简介最多六行，保持演员和标签的空间；特别多的人员或标签仍可在盒盖内滚动。剧照条恢复跟随光盘中心的位置。

主程序的封面纸采用粗糙度 0.9，盒壳 `trans` 材质恢复为不透明白色、金属度 0、粗糙度 0.9、透射 0，避免导出 `pic` 的 0.08 粗糙度使亮色封面被高光冲白；独立 demo 和原始 GLB 均不修改。图片路径去除首尾空白；丢失文件或解码失败记录到运行日志。原生书架回归使用 JPG 并检查闭盒和缩放后读回的封面像素，不能只凭 CPU 贴图已载入判断图片可见。

光盘盘面同样绑定当前作品封面：适配层将 `disc` 下的 `Material` 印刷面标识为 `disc-cover`。按旧 `Dvd.qml` 的方式直接使用模型原有 UV 和整张封面，保留 Repeat 采样，不再额外居中裁切正面图片，也不重建圆环网格。盘面与盒子共用同一个 `TextureData`，异步高清替换和切换图片时同步更新；原始 GLB 不修改，独立 DVD demo 共用此行为。主程序按原 QML 调整盘面粗糙度为 0.9、边缘为 0.8 不透明度/0.9 粗糙度，背面为 0.15 金属度/0.85 粗糙度，中心孔和原网格保持不变。

主程序 HDR 朝向由原先遗漏的 0° 改为原 `dvd_scene.qml` 的 Y 轴 155°，背景与 IBL 使用同一旋转；曝光调整为 0.85、环境强度为 0.8，在过亮和过暗之间恢复正面亮度。书架主灯方向为相机坐标 (-0.5,0.8,1)，强度 1.8；随相机旋转，保持在左前上方。渲染器新增可配置主灯方向、强度和跟随相机选项，其他视图默认仍使用原世界方向及 2.4 强度。直接光照和阴影共用同一世界方向，避免亮面与投影方向不一致。

关系图在未展开的聚焦 DVD 旁显示，恢复原 QML `frontInfoAnchor`：锚点是 spine 下、与 front 同级的节点，局部位置为 front.translation + (0.0675,0,0)，仅随 spine 旋转，不能替换为 front 网格中心或整个盒子中心。关系图高为视口高的 0.88、宽为 0.61，锚点水平左移视口高的 0.42 后居中，并限制在视口内。按用户澄清，关系图保留白底，原锚点和位置修正保持；透明背景只作用于盒内信息按钮和剧照区域。展开时隐藏。剧照条的滚动视口、内容和占位文字背景同样透明，修改仅作用于书架预览。

封面清晰度修正：屏幕内未选中实例按投影高度和 DPR 请求 512～2048 像素封面（常规窗口约 1024 像素），屏幕外实例保留 256 像素缩略图；选中实例请求 2048～4096 像素。当前 GLB 的书脊 UV 仅占整张图宽度约 4.9%，原先整图 256 像素只给书脊留下约 12 像素，单独提高选中清晰度不能解决书架侧面模糊。缓存允许到 4096 像素，且不会放大小于目标尺寸的原图。高清解码完成前保留现有封面。

封面纹理启用最多 8 次采样的软件各向异性过滤：通过纹理空间像素覆盖范围的长短轴计算采样方向和 mip 梯度，避免斜视角为整张封面选取过低的 mip。使用 `TextureData::maxAnisotropy` 控制，其他纹理默认维持原采样；无需依赖 QRhi 的原生各向异性 sampler 接口。主程序预算从 64 MiB 调整到 128 MiB，减少上传阶段再次整体降采样对书脊清晰度的影响。

展示距离按视口比例调整，避免窄窗口裁切展开的盒子；初始书架保留短列表居中、长列表从左侧开始的取景方式。滚动或手动操作后，调整窗口尺寸不会覆盖用户的相机位置。

## 验证

- Debug 主程序和 `release-data` 主程序、两个独立 demo 构建通过。
- 移除详情栏与提高清晰度后，Debug 书架回归 20 项通过，覆盖筛选、收藏状态、删除重置、连续定位、高清封面、4× MSAA 读回、右键菜单及选中前后视口宽度不变。
- DVD 场景回归：17 项通过，包含高清解码、不放大小图、悬浮保持、相机、拾取、生命周期与隐藏/换父窗口。
- 主窗口回归：8 项通过，包含书架预载与导航。
- 隔离启动测试接受/拒绝路径通过；Release 独立 demo 的 1000 件 DVD 检查通过。

本轮使用临时数据库验证，未使用或修改用户作品数据。测试日志位于 `build/shelf-qrhi-*`，截图位于 `build/shelf-qrhi-artifacts/shelf-native-open.png`。

详情栏与清晰度修正的最新日志为 `build/shelf-cover-page-tests.txt`、`build/shelf-cover-scene-tests.txt`，最新截图位于 `build/shelf-cover-artifacts/`；正式 Release 主程序已重新构建。

书脊修正的日志位于 `build/shelf-spine-*`，截图位于 `build/shelf-spine-artifacts/`。渲染回归新增窄长纹理对比，验证各向异性过滤比普通 mip 采样保留更多文字条纹对比度；书架回归验证未选中实例仍保留至少 1024 像素封面及 8 次采样设置。

书脊修正后：渲染回归 14 项、书架回归 20 项、DVD 场景回归 17 项通过；150% DPR 下额外验证各向异性对比与原生书架高清封面。悬浮测试隔离真实桌面鼠标的自发移动/进入/离开事件，仅使用显式合成输入，保持原有抽出位置及归位断言。

恢复盒盖信息与原页面样式后：Debug 测试目标与 Release 主程序构建通过，书架回归 20 项、DVD 场景回归 17 项通过，150% DPR 下原生书架专项检查通过。新增检查覆盖盒盖内文本、演员与标签点击、长简介截断、窗口缩放后的定位、合盒隐藏及跨作品切换。日志为 `build/shelf-cover-info-page-tests.txt`、`build/shelf-cover-info-scene-tests.txt` 和 `build/shelf-cover-info-dpr15-tests.txt`；截图位于 `build/shelf-cover-info-artifacts/`，下方截图已更新。

本轮样式与定位修正的日志为 `build/shelf-fidelity-page-tests.txt`、`build/shelf-fidelity-scene-tests-visible.txt`、`build/shelf-fidelity-dpr15-tests.txt`：书架 20 项、场景 17 项及 150% 缩放专项均通过。书架套件中的可选现有数据探针默认跳过，另通过 `DARKEYE_SHELF_TEST_DATABASE` 指向 VS 的安装目录，以只读连接加载完整书架验证 JPG、开合及信息层，日志为 `build/shelf-fidelity-existing-tests.txt`。合成截图位于 `build/shelf-fidelity-artifacts/`；现有作品截图仅存于本地 build 目录，不复制进文档。Release 可执行文件已同步至 `out/install/release-data/Darkeye.exe`，未覆盖安装目录的数据。

盘面封面修正后：Release 主程序与独立 DVD demo 构建通过；DVD 场景 18 项回归通过，新增验证盘面换图与实例隔离；随后按旧效果改为验证原网格及 UV 始终共享，不再进行裁切；书架盘面像素读回与 150% 缩放专项均通过。日志为 `build/disc-artwork-scene-recheck.txt`、`build/disc-artwork-shelf-tests.txt`、`build/disc-artwork-dpr15-tests.txt`，开盒截图已更新。闭盒像素检查等待最终渲染帧，避免仅依赖固定延时读取动画中间画面。

按原效果回调的截图位于 `build/shelf-legacy-look-artifacts/`，记录原 UV、155° HDR 朝向和曝光/环境强度 0.65。此次保持既有交互逻辑，改动仅涉及贴图、材质和书架光照。

本轮盘面 UV/换图专项、原生书架盘面读回、150% 缩放及只读现有作品探针均通过，日志为 `build/shelf-legacy-look-disc-tests.txt`、`build/shelf-legacy-look-shelf-tests.txt`、`build/shelf-legacy-look-dpr15-tests.txt`、`build/shelf-legacy-look-real-tests.txt`。完整场景回归的悬浮检查仍出现失败，另一次运行有聚焦状态失败（见 `build/shelf-legacy-look-scene-final.txt`）；尚未确认原因，不能视为全量场景通过。本轮未保留试验性的交互修改。Release 已同步至 VS 安装目录。





材质限制保持与 demo 一致：当前不支持 `KHR_materials_clearcoat` 和 `KHR_materials_specular`，多层透射仍为屏幕空间近似，不能保证与 Blender 离线渲染完全一致。

主灯与亮度修正的验证：Debug 测试目标及 Release 主程序构建通过；光照回归 7 项通过，新增正面/背面灯光对比，以及相机转向后跟随灯仍照亮可见面的像素读回；原生书架专项和实际 JPG 只读探针通过。日志为 `build/shelf-front-light-render-tests.txt`、`build/shelf-front-light-shelf-tests.txt`、`build/shelf-front-light-real-tests.txt`。合成截图更新为 `build/shelf-front-light-artifacts/` 的版本。Frame UBO 由 384 扩展为 400 字节（增加方向与强度），CPU 上传、所有引用公共 Frame 的 shader 及统计同步更新。Release 已同步至 VS 安装目录。

白色盒壳与透明叠层修正：Debug 测试目标和 Release 主程序构建通过；原生书架专项及 150% 缩放检查通过，新增材质白色/不透明/零透射断言、旧关系图锚点与位置检查、透明区域与底层场景的像素对比。测试隔离物理鼠标对画布的输入，仍保留作品信息按钮的实际点击及场景 API 检查。日志为 `build/shelf-white-case-tests.txt`、`build/shelf-white-case-dpr15-tests.txt`，截图为 `build/shelf-white-case-artifacts/`；Release 已同步至 VS 安装目录。

用户澄清后，关系图恢复白色清屏，回归验证图内空白区域为白色；盒内信息、演员按钮和剧照区域的透明处理保持。

大底澄清修正：`ShelfCoverInfo`、内容分组和 `ShelfCoverActions` 容器透明，单个演员和标签按钮恢复原底色；关系图白底保持。原生书架专项通过，新增大容器空白处与底层场景的像素对比，并检查演员小按钮仍为白底。日志为 `build/shelf-panel-clear-tests.txt`，截图为 `build/shelf-panel-clear-artifacts/`。Debug 测试目标及 Release 主程序构建通过，VS 安装目录已同步。

盒内信息排版修正：恢复按视口高度 55% 计算的信息宽度，移除原先盒盖高度 90% 的限制及不必要的滚动条预留；根据实际内容高度手动设置滚动内容尺寸，避免 QScrollArea 额外产生滚动范围。演员、标签和制作信息按钮按 14 像素字体的实际尺寸设置高度，保持 18 像素标题、14 像素正文和原有间距。信息使用旧 `frontInfoAnchor` 居中并限制水平位置，窄窗口适当拉近展示距离，使可见文字和按钮保留在盒盖内。超量人员、标签仍允许滚动，不通过缩小字号容纳内容。

光碟本地视频提示恢复：沿原 QML 的盘面主轴 X 做平面内旋转，无本地视频时偏转 30°，有本地视频时为 0°，保留导出模型的中心、缩放和原始朝向。作品详情中的 `videoUrl` 非空状态同步至场景实例，导入/移除视频并刷新详情时用 380ms 动画切换，不重建场景或丢失开盒状态。没有持续自转，以保留盘面姿态的状态含义。模型姿态/动画与封面 UV 两项专项共 4 项通过，书架集成专项 3 项通过，涵盖打开时无视频倾斜、详情更新后摆正、移除后倾斜及仍保持打开选中。日志为 `build/disc-video-state-scene-tests.txt`、`build/disc-video-state-shelf-tests.txt`，截图位于 `build/disc-video-state-artifacts/`。Release 已同步至 VS 安装目录并核对 SHA256。

倾斜光碟点击修正：开盒时改为包围盒粗选后检测真实三角形表面，避免整个盒壳的包围盒抢先命中、把点盘面误判成合盒。未开盒的书架仍使用原包围盒拾取；悬浮和单击共用同一判定。真实 GLB 回归重现旧包围盒遮挡，并分别点击 30° 倾斜和 0° 正向盘面的八个位置，断言只发出光碟点击且盒子始终打开；原有相机与选中检查通过。CPU 场景回归 8 项、DVD 专项 5 项通过，日志为 `build/disc-picking-scene-tests.txt` 与 `build/disc-picking-dvd-tests.txt`。Debug 测试目标与 Release 主程序构建通过。

用户关闭运行中的程序后，光碟点击修复版已同步至 `out/install/release-data/Darkeye.exe`，构建文件与安装文件 SHA256 一致，未覆盖用户数据。

默认贴图恢复原 QML 的 `resources/maps/0.png`（浅白封面、灰色书脊）。DVD 场景库内嵌同一原图，替换原先深色且带 DVD FRONT/BACK 字样的临时贴图；异步加载中、路径为空、图片丢失或解码失败均使用该图，正式封面仍照常替换。缓存回归 3 项通过，检查默认图像与原文件像素一致及缺图回退，日志为 `build/dvd-white-default-tests.txt`。Debug 测试目标与 Release 主程序构建通过，VS `release-data` 安装目录已同步并核对 SHA256 一致。

滚轮悬浮修正：记录画布内鼠标位置，在滚轮推动相机的每次更新及归位完成后重新拾取，更新抽出目标、手形光标与提示。滚轮事件自身记录位置，无需先移动鼠标；鼠标移出后清除位置有效标记，后续滚动帧不会重新触发抽出，拖拽期间也不重算悬浮。新增真实 GLB 回归仅发送滚轮事件，验证原位置 DVD 缩回、移动到鼠标下方的新 DVD 抽出，以及滚动中移出后所有 DVD 恢复原位。该检查、光盘点击及相机输入检查通过；同次运行既有悬浮保持检查首次失败，单独复查通过，不能将首次组合检查记为全部通过。日志为 `build/dvd-wheel-hover-tests.txt`（5 通过、1 失败）与 `build/dvd-wheel-hover-hold-tests.txt`（3 通过）。Debug 测试目标、Release 主程序构建通过，Release 已同步至 VS 安装目录并核对 SHA256。

相机右键控制恢复原 QML：书架使用相机原地转向，位置保持 `(cameraX, 0.1, cameraDistance)`（平移时随平移偏移），不再绕场景目标公转。左右角度限制 ±15°、上下 ±10°，灵敏度由每像素 0.3° 恢复为 0.15°。通用 Camera 新增可选原地转向模式，默认仍保留其他视图使用的轨道模式；投影、射线和选中 DVD 的相机前方锚点使用同一相机朝向。CPU 场景回归 9 项、DVD 输入/盘面点击/滚轮悬浮专项 5 项通过，覆盖固定眼点、中心射线朝向、双向角度限制与重置。日志为 `build/dvd-camera-limits-scene-tests.txt`、`build/dvd-camera-limits-input-tests.txt`。Debug 测试目标与 Release 主程序构建通过，VS `release-data` 已同步并核对 SHA256。

排版验证使用包含两行标题、六行简介、演员、完整制作信息及九个标签的临时数据：普通缩放原生书架专项 3 项通过；150% 缩放的内容完整性、无滚动条和窄窗口边界断言通过，后续归位检查出现选中状态未清除的失败，不能视为该次专项全程通过。日志为 `build/shelf-info-fit-tests.txt` 和 `build/shelf-info-fit-dpr15-tests.txt`；历史测试截图已清理。Debug 测试目标及 Release 主程序构建通过，Release 已同步至 VS 的 `out/install/release-data/Darkeye.exe` 并核对 SHA256 一致，未覆盖用户数据。
