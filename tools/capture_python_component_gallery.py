"""Capture the canonical Python darkeye_ui demo without changing its source.

The C++ compatibility project owns this runner so the Python implementation
remains a read-only baseline. It calls the demo's existing page factories and
produces Light, Dark and custom-primary screenshots for regression checks.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--custom-primary", default="#336699")
    return parser.parse_args()


def build_advanced_page(theme):
    """Compose the public components omitted by Python's interactive demo."""
    from PySide6.QtCore import QDate, Qt
    from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget
    from darkeye_ui.components.calendar_heatmap import CalendarHeatmap
    from darkeye_ui.components.lazy_scroll_area import LazyScrollArea
    from darkeye_ui.components.label import Label
    from darkeye_ui.components.modern_scroll_menu import ModernScrollMenu
    from darkeye_ui.components.radar_chart_widget import RadarChartWidget
    from darkeye_ui.components.token_collapsible_section import TokenCollapsibleSection
    from darkeye_ui.components.token_list_widget import TokenListWidget
    from darkeye_ui.components.token_v_label import TokenVLabel
    from darkeye_ui.components.token_vertical_tab_bar import TokenVerticalTabBar

    page = QWidget()
    layout = QVBoxLayout(page)
    layout.setContentsMargins(16, 16, 16, 16)
    layout.setSpacing(10)
    layout.addWidget(Label("高级组件（图表、导航与容器）"))
    activity = {QDate(2025, 1, day): (day // 3) % 4 + 1 for day in range(1, 32, 3)}
    heatmap = CalendarHeatmap(2025, activity, theme_manager=theme)
    heatmap.setMaximumHeight(155)
    layout.addWidget(heatmap, 0, Qt.AlignHCenter)
    columns = QHBoxLayout()
    columns.setSpacing(12)

    left = QVBoxLayout()
    left.addWidget(Label("RadarChartWidget"))
    radar = RadarChartWidget(
        ["速度", "质量", "覆盖", "稳定性", "体验"],
        [0.72, 0.86, 1.0, 0.78, 0.68],
        theme_manager=theme,
    )
    radar.setMinimumSize(260, 220)
    radar.update_chart()
    left.addWidget(radar)
    left.addWidget(Label("TokenCollapsibleSection"))
    section = TokenCollapsibleSection("展开的示例", theme_manager=theme)
    section.add_widget(Label("内容随主题令牌切换"))
    section.expand()
    left.addWidget(section)

    middle = QVBoxLayout()
    middle.addWidget(Label("LazyScrollArea / TokenListWidget"))
    lazy = LazyScrollArea(120, theme_manager=theme)
    lazy.setMinimumHeight(130)
    lazy.set_loader(lambda page_index, _: [Label("懒加载项目 A"), Label("懒加载项目 B")]
                    if page_index == 0 else [])
    middle.addWidget(lazy)
    list_widget = TokenListWidget()
    list_widget.addItems(["列表项目 A", "列表项目 B", "列表项目 C"])
    list_widget.setMaximumHeight(100)
    middle.addWidget(list_widget)

    right = QVBoxLayout()
    right.addWidget(Label("TokenVLabel / TokenVerticalTabBar"))
    vertical_row = QWidget()
    vertical_layout = QHBoxLayout(vertical_row)
    vertical_layout.addWidget(TokenVLabel("令牌标签", theme_manager=theme))
    vertical_tabs = TokenVerticalTabBar(theme)
    vertical_tabs.addTab("概览")
    vertical_tabs.addTab("高级")
    vertical_layout.addWidget(vertical_tabs)
    right.addWidget(vertical_row)
    right.addWidget(Label("ModernScrollMenu"))
    menu = ModernScrollMenu(
        {"第一节": Label("滚动菜单内容 A"), "第二节": Label("滚动菜单内容 B")},
        theme,
    )
    menu.setMinimumHeight(180)
    right.addWidget(menu)

    columns.addLayout(left, 1)
    columns.addLayout(middle, 1)
    columns.addLayout(right, 1)
    layout.addLayout(columns, 1)
    return page


def build_settings_page(theme):
    from PySide6.QtWidgets import QLabel, QVBoxLayout, QWidget

    page = QWidget()
    layout = QVBoxLayout(page)
    layout.setContentsMargins(24, 24, 24, 24)
    layout.addWidget(QLabel("设置"))
    layout.addWidget(QLabel("这里是示例设置页面内容。"))
    layout.addStretch()
    return page


def main() -> int:
    args = parse_args()
    python_root = args.python_root.resolve()
    if not (python_root / "darkeye_ui" / "demo.py").is_file():
        raise ValueError(f"not a Darkeye Python root: {python_root}")
    sys.path.insert(0, str(python_root))

    from PySide6.QtCore import QEventLoop
    from PySide6.QtWidgets import QApplication, QHBoxLayout, QWidget
    from controller.app_context import set_theme_manager
    from darkeye_ui import demo
    from darkeye_ui.design import ThemeId, ThemeManager
    from darkeye_ui.components.sidebar import Sidebar

    app = QApplication.instance() or QApplication([])
    manager = ThemeManager()
    set_theme_manager(manager)
    page_factories = {
        "buttons": demo._build_page_buttons,
        "text": demo._build_page_text,
        "toggles": demo._build_page_toggles,
        "inputs": demo._build_page_inputs,
        "containers": demo._build_page_containers,
        "data_nav": demo._build_page_data_nav,
        "p2_experience": demo._build_page_p2_experience,
        "more": demo._build_page_more,
        "color_icons": lambda theme: demo._build_page_color_icons(theme, []),
        "theme": lambda theme: demo._build_page_theme(theme, []),
        "advanced": build_advanced_page,
        "setting": build_settings_page,
    }
    variants = (
        ("light", ThemeId.LIGHT, None),
        ("dark", ThemeId.DARK, None),
        ("light-custom-primary", ThemeId.LIGHT, args.custom_primary),
    )
    menu_defs = [
        ("buttons", "按钮", "square_pen"),
        ("text", "文本输入", "scroll_text"),
        ("toggles", "开关选择", "check"),
        ("inputs", "数值滑块", "list_plus"),
        ("containers", "标签页分组", "layout_panel_left"),
        ("data_nav", "P1 Data/Nav", "layout_panel_left"),
        ("p2_experience", "P2 Experience", "circle_plus"),
        ("more", "更多组件", "circle_plus"),
        ("advanced", "高级组件", "chart_line"),
        ("color_icons", "颜色图标", "copy"),
        ("theme", "主题", "refresh_cw"),
    ]
    output_dir = args.output_dir.resolve()
    for variant_name, theme_id, primary in variants:
        manager.set_custom_primary(primary)
        manager.set_theme(app, theme_id)
        target = output_dir / variant_name
        target.mkdir(parents=True, exist_ok=True)
        for page_name, factory in page_factories.items():
            page = factory(manager)
            root = QWidget()
            root_layout = QHBoxLayout(root)
            root_layout.setContentsMargins(0, 0, 0, 0)
            root_layout.setSpacing(0)
            sidebar = Sidebar(menu_defs=menu_defs, theme_manager=manager, parent=root)
            sidebar.select(page_name)
            root_layout.addWidget(sidebar)
            root_layout.addWidget(page, 1)
            root.resize(1280, 820)
            root.show()
            app.processEvents(QEventLoop.AllEvents)
            pixmap = root.grab()
            if pixmap.isNull():
                raise RuntimeError(f"failed to capture {variant_name}/{page_name}")
            file_name = f"component-gallery-{page_name.replace('_', '-')}.png"
            if not pixmap.save(str(target / file_name), "PNG"):
                raise RuntimeError(f"failed to save {variant_name}/{page_name}")
            root.close()
            root.deleteLater()
            app.processEvents(QEventLoop.AllEvents)
    print(f"Python component snapshots generated: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
