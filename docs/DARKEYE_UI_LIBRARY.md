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

## 明确保留在业务层的类型

`AsyncImageLabel`、`CrawlerFieldSelector`、`FanartStripWidget`、`IdCheckList`、
`ImageDropWidget`、`JsonTransferBar`、`MakerSelector`、`PathManagement`、人物/作品卡片、
统计卡片以及所有管理组件继续位于 `libs/ui/components`。它们来自 Python `ui/`
或依赖 Darkeye 的数据库、领域模型、资源目录、采集器或业务语义，不进入基础库。
