# MyADS C++ 高级停靠区

这是 Python `ui/myads` 的 Qt/C++ 迁移版本，对外目标为 `darkeye::docking`。

## 已迁移能力

- `LayoutTree` 的横向、纵向、嵌套与根级拆分，以及拆分比例归一化。
- `PaneWidget` 的稳定内容 ID、标签顺序、活动标签、图标、关闭权限和内容转移。
- 标签在同一窗格内排序，以及拖到中央合并、四边拆分、工作区四边中段根级拆分。
- 拖放区域预览和独立 `DockTheme`，不会修改应用全局样式表。
- `schema_version=1` 布局、内容描述、窗格元数据和活动标签的原子保存与恢复。
- 加载前完整校验；工厂恢复中途失败时回到单个可用空窗格。

## 测试程序

构建 Release 演示程序：

```powershell
cmake --build --preset release --target darkeye_myads_demo
```

输出为 `build/windows-msvc/Release/darkeye_myads_demo.exe`。工具栏可以添加标签、
拆分、保存、恢复、重置和切换主题；也可以直接拖动标签测试五方向及根级停靠。

自动验证：

```powershell
ctest --test-dir build/windows-msvc -C Release -R darkeye_myads --output-on-failure
```
