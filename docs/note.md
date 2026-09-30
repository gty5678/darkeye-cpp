# 开发笔记

## ImageOverlay 首次加载 Hover 无响应 (2026-02-11)

**现象**：首次进入页面时节点悬停不触发图片；首次点击后才正常。

**原因**：叠加层使用了 `WindowTransparentForInput`，整窗始终对输入透明，导致底层 `ForceViewOpenGL` 在首次未获得焦点/点击前收不到鼠标移动，`nodeHoveredWithInfo` 不触发。

**处理**：按显示/隐藏动态切换窗口标志——隐藏时 `_update_window_flags(transparent_input=True)` 保持事件穿透；显示前 `_update_window_flags(transparent_input=False)`，再由 `eventFilter` 把事件转发给底层 view。在 `__init__`、`on_node_hovered_with_info`（显示/隐藏分支）、`_on_image_loaded`、`hide_image` 中统一调用 `_update_window_flags`。

---

## 图片跟随节点移动 - 缩放后失效问题修复 (2026-02-11)

### 问题描述
ImageOverlayWidget 中，图片跟随节点移动的功能在缩放后失效，不再跟随节点移动。不缩放时功能正常。

### 问题原因
在 `_on_scale_changed()` 方法中，使用 `_last_hover_info` 保存的节点坐标来计算图片位置。但 `_last_hover_info` 中的坐标是悬停时的静态值，不会随节点移动而更新。因此缩放后虽然更新了图片位置，但使用的是旧的节点坐标，导致图片位置不准确，表现为不跟随节点移动。

### 解决方案
修改 `_on_scale_changed()` 方法，不再使用 `_last_hover_info` 中的旧坐标，而是通过 `self._view_widget.getNodePosition(node_id)` 从视图获取节点的最新位置，确保缩放后图片能正确跟随节点移动。

### 代码改动
- 将 `self._last_hover_info` 中解包的 `x, y` 改为 `_`（忽略旧坐标）
- 调用 `self._view_widget.getNodePosition(node_id)` 获取最新的节点位置
- 添加异常处理，增强代码健壮性

### 关键代码
```python
# 从视图获取节点的最新位置（而不是使用保存的旧坐标）
node_pos = self._view_widget.getNodePosition(node_id)
if not node_pos.isNull():
    x = node_pos.x()
    y = node_pos.y()
    self.image_rect = self._calculate_image_rect(x, y, radius, scale, node_id)
    self._update_widget_geometry(x, y)
```

### 经验总结
在涉及动态位置更新的场景中，应始终从数据源获取最新状态，而不是依赖缓存的静态值。特别是在缩放、平移等变换操作后，必须重新获取对象的最新位置信息。
