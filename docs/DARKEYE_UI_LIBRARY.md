# `darkeye_ui` 基础组件库迁移边界

Python 基线：`darkeye/darkeye_ui`。C++ 落点：`libs/darkeye_ui`。

## 架构边界

- `darkeye::darkeye_ui`：基础主题、图标、通用布局、基础控件及 `LazyWidget`。
- `darkeye::ui`：页面、对话框和依赖数据库/领域模型/服务的业务组件。
- `darkeye::docking`：Python `ui/myads` 的业务工作区，不属于基础组件库。
- `darkeye::desktop_ui`：桌面应用级 `MainWindow` 与最终页面装配。

基础库公共头为 `darkeye_ui/DarkeyeUi.h`。`darkeye_ui_boundary_tests` 只链接
`darkeye::darkeye_ui` 和 Qt Test，用于阻止基础库重新依赖 `darkeye::ui`。
样式表与 Logo 由 `libs/darkeye_ui/darkeye_ui_resources.qrc` 自主管理，因此该目标
也不依赖应用 `core`、数据库或服务库。

## Python 文件到 C++ 文件映射

| Python | C++ |
|---|---|
| `base/lazy_widget.py` | `base/LazyWidget.*` |
| `_logging.py` | `base/WarningOnce.*` + Qt 日志 |
| `design/tokens.py` | `theme/ThemeService.*::ThemeTokens` |
| `design/theme_manager.py` | `theme/ThemeService.*` |
| `design/theme_context.py` | `theme/ThemeContext.*`，C++ 优先显式依赖注入 |
| `design/loader.py` | `theme/StylesheetLoader.*` |
| `design/icon.py` | `theme/IconProvider.*` |
| `design/myads_adapter.py` | 业务边界 `libs/ui/layouts/myads/ThemeAdapter.*` |
| `layouts/flow_layout.py` | `layouts/FlowLayout.*` |
| `layouts/v_flow_layout.py` | `layouts/VerticalFlowLayout.*` |
| `layouts/vertical_text_layout.py` | `layouts/VerticalTextLayout.*` |
| `layouts/waterfall_layout.py` | `layouts/WaterfallLayout.*` |
| `button.py` | `components/DesignButton.*` (`Button`) |
| `label.py` | `components/DesignLabel.*` (`Label`) |
| `input.py` | `components/DesignInput.*` |
| `combo_box.py` | `components/DesignComboBox.*` |
| `avatar.py` | `components/Avatar.*` |
| `breadcrumb.py` | `components/Breadcrumb.*` |
| `calendar_heatmap.py`, `radar_chart_widget.py` | `components/Charts.*` |
| `callout_tooltip.py`, `circular_loading.py`, `skeleton.py` | `components/LoadingFeedback.*` |
| `chamfer_button.py` | `components/ChamferButton.*` |
| `chip.py` | `components/Chip.*` (`Chip`/`Tag`) |
| `clickable_slider.py`, `rotate_button.py`, `shake_button.py` | `components/InteractionEffects.*` |
| `color_picker.py`, `color_slider.py` | `components/ColorPicker.*`, `ColorWheel.*` |
| `completer_line_edit.py` | `components/CompleterLineEdit.*` |
| `empty_state.py` | `components/EmptyState.*` |
| `heart_label.py` | `components/HeartLabel.*` |
| `heart_rating_widget.py` | `components/RatingSelector.*` |
| `icon_push_button.py` | `components/IconButton.*` |
| `lazy_scroll_area.py` | `components/LazyScrollArea.*` |
| `modal_dialog.py` | `components/ModalDialog.*`及兼容别名 |
| `modern_scroll_menu.py` | `components/ModernScrollMenu.*` |
| `oct_image.py` | `components/OctImage.*` |
| `pagination.py` | `components/Pagination.*` |
| `search_bar.py` | `components/SearchBar.*` |
| `sidebar.py`, `sidebar2.py` | `components/Sidebar.*` |
| `state_toggle_button.py` | `components/StateToggleButton.*` |
| `toast_notification.py` | `components/ToastNotification.*` |
| `toggle_switch.py` | `components/AnimatedIndicators.*` |
| `token_*` 基础输入、视图和容器 | `components/TokenControls.*`, `TokenViews.*` |
| `token_collapsible_section.py` | `components/CollapsibleSection.*` |
| `token_link_card.py` | `components/LinkCard.*` |
| `token_v_label.py`, `token_vertical_tab_bar.py`, `vertical_text_label.py` | `components/VerticalText.*` |
| `transparent_widget.py` | `components/TokenControls.*::TransparentWidget` |

`demo.py` 对应独立应用 `apps/component_gallery`。Python 包导出文件
`__init__.py` 对应 C++ 聚合头 `DarkeyeUi.h`、`Components.h` 和 `Layouts.h`。

## 可执行兼容契约

`docs/DARKEYE_UI_COMPATIBILITY_MATRIX.json` 是 Python 公共组件到 C++ 公共
组件的唯一兼容映射。每项必须说明构造参数、方法/信号、主题机制和 C++ Gallery
截图页。`python_demo_coverage` 明确 Python 视觉样例是由 Demo、外部快照
Harness，还是因基准导出不可解析而暂不可用；校验器还会验证方法
字段中每个显式 `Python -> C++` 映射的 C++ 方法、以及每个非继承的自定义信号，
确实声明在公共组件头文件中。构造参数必须显式映射；仅兼容别名可通过
`api_alias_of` 复用目标组件的构造契约，校验器会解析 C++ `using` 链确认它们
最终是同一个类型。校验器还通过 AST 枚举 Python 类自身的公开方法：每个方法
必须有映射，或者被明确列入 `python_implementation_helpers`（绘制、动画和 Qt
Property 回调等非跨语言公共契约）。
请用下列入口检查它；
两个视觉基线参数成对提供时，会额外验证七种
预设主题加 Light 自定义主色、共 96 张截图的尺寸与严格感知误差
（默认 RGB 平均绝对误差 `2.0/255`、显著变化像素比例 `5%`）。这避免
动画控件的相邻帧或 PNG 编码差异造成误报。
加上 `-CrossImplementationVisual` 后，还会逐张比较 Python 与 C++ 对应截图的
尺寸、RGB 平均绝对误差（默认上限 `6.0/255`），以及 RGB 差异大于 24 的像素
比例（默认上限 `15%`）。

该契约检查的是 **公共组件覆盖、已记录的适配关系与各实现自身的视觉回归**，
不能把它解读为 Python 与 C++ 的逐像素一致，或两种语言 API 的逐字符一致。
例如 Python 的 `snake_case` 在 C++ 中通常是 Qt 风格 `camelCase`，而
`Button`、`Dialog`、`Toast` 等名称由 C++ 兼容别名承接。要宣布“完全对齐”，
还必须逐项消除矩阵中记录的适配差异，并以同一场景的跨实现视觉基线验收。

Python 基准不需要为截图加入命令行代码。C++ 仓库的只读驱动会调用其现有
Demo 页面构造函数，生成 Light、Dark 与 Light 自定义主色快照：

```powershell
.\tools\capture-python-component-gallery.ps1 `
  -PythonRoot <Python-Darkeye-root> `
  -PythonExecutable <Python-with-PySide6>
```

用同一驱动生成两次目录后，可对这 36 张截图做 Python 自身的感知回归：

```powershell
.\tools\verify-python-component-gallery.ps1 `
  -BaselineDirectory <python-baseline> -ActualDirectory <python-current> `
  -PythonExecutable <Python-with-PySide6>
```

```powershell
.\tools\verify-darkeye-ui-contract.ps1 `
  -PythonRoot <Python-Darkeye-root> `
  -PythonExecutable <Python-with-PySide6> `
  -CppBaselineDirectory <cpp-baseline> -CppActualDirectory <cpp-current> `
  -PythonBaselineDirectory <python-baseline> -PythonActualDirectory <python-current> `
  -CrossImplementationVisual
```

## 明确保留在业务层的类型

`AsyncImageLabel`、`CrawlerFieldSelector`、`FanartStripWidget`、`IdCheckList`、
`ImageDropWidget`、`JsonTransferBar`、`MakerSelector`、`PathManagement`、人物/作品卡片、
统计卡片以及所有管理组件继续位于 `libs/ui/components`。它们来自 Python `ui/`
或依赖 Darkeye 的数据库、领域模型、资源目录、采集器或业务语义，不进入基础库。
