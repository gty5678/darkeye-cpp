# `ActressWorkTimeline.py` 实现要点（重写参考）

本文用于梳理 `ui/widgets/ActressWorkTimeline.py` 的核心实现思路，便于后续重写时保留行为一致性。

## 1. 组件目标与交互契约

- 显示女优作品的“时间轴视图”：发行日为主轴，未知发行日单独分区。
- 支持连续缩放（按天像素 `ppd`），并且缩放锚点跟随鼠标位置。
- 支持多种滚动手势：
  - 普通滚轮：缩放（围绕指针位置）。
  - `Ctrl + 滚轮`：横向平移。
  - `Shift + 滚轮`：纵向滚动。
  - 中键拖拽：同时进行横/纵方向平移。
- 作品点位为菱形 marker，悬停后弹出 `CoverCard2` 卡片。

---

## 2. 结构分层（重写时建议保持）

当前实现分为三层：

1. `ActressWorkTimeline`
   - 主容器与状态中心。
   - 管理数据、布局重建、缩放/滚动行为、悬浮卡片生命周期。
2. `TimelineCanvas`
   - 负责“背景 + 标尺 + 日期轴 + 刻度文字”的绘制。
   - 不直接画作品，作品由子控件 marker 承载。
3. `_TimelineWorkMarker`
   - 单作品菱形锚点控件。
   - 处理 hover、中键拖拽转发、滚轮事件转发。

辅助层：

- `_HoverCardPopup`：顶层 tooltip 容器，用于承载 `CoverCard2`，并支持鼠标移入维持显示。
- `_TimelineScrollArea`：主要作为语义封装（当前未重写 wheel 行为）。

---

## 3. 关键常量与坐标体系

核心概念：

- `ppd`（pixels per day）：1 天对应多少像素，是缩放的唯一标尺。
- `TRACK_PAD_X`：时间轴左边额外留白，避免第一个 marker 裁切。
- `date_min + day_index`：日期轴线性映射基础。
- `x = TRACK_PAD_X + day_index * ppd`：日期点在画布上的横坐标。

关键常量分组：

- 缩放范围：
  - `PPD_MIN = 0.001`
  - `PPD_MAX = 300.0`
  - slider 通过 `ZOOM_SLIDER_SCALE` 与 `ppd` 双向同步。
- 轴与刻度：
  - `RULER_HEIGHT`
  - `TICK_TARGET_MAJOR_PX`
  - `TICK_TARGET_LABEL_PX`
  - `RULER_CLIP_PAD_PX`
- marker 相关：
  - `MARKER_HIT`（命中区）
  - `DIAMOND_STRIDE`（同日多作品纵向错位）
- 时间范围策略：
  - 默认窗口 `2016-01-01 ~ 2026-12-31`
  - 合并 padding：`TIMELINE_MERGE_SIDE_PAD_DAYS`
  - 向两端扩展：`TIMELINE_INFINITE_EXTRA_YEARS`

---

## 4. 数据输入与归类规则

`set_work_rows(rows)` 接收 `list[tuple]`，重建时使用字段：

- `row[:6]`：`serial_number, title, cover_path, tag_id, work_id, standard`
- `row[6]`：发行日期字符串（`YYYY-MM-DD` 形式）

预处理逻辑：

1. 通过 `_parse_release` 解析日期，失败视为未知日期。
2. 已知日期数据按 `date -> list[row]` 分桶（`by_day`）。
3. 同一天作品按 `(serial_number, work_id)` 排序，保证稳定布局。
4. 未知日期单独放入 `unknown_list`。

---

## 5. 时间范围构建逻辑（非常关键）

布局重建 `_rebuild_layout` 时：

1. 若有已知日期：
   - `raw_min/raw_max` 来自真实数据。
   - 与默认窗 `2016~2026` 做并集，得到 `merged_min/merged_max`。
   - 两边再加 `TIMELINE_MERGE_SIDE_PAD_DAYS`。
   - 然后继续向过去/未来各延伸 `TIMELINE_INFINITE_EXTRA_YEARS`。
2. 若只有未知日期：
   - 以默认窗为核心，同样做 side pad 和两端扩展。
3. 若画布宽度不足以覆盖视口：
   - `_ensure_axis_covers_viewport` 会“左右对称补天数”，避免看起来挤成一条线。

这套逻辑确保：

- 时间轴不是“只包数据最小盒”，而是有较大可平移空间。
- 首屏可稳定定位到默认窗区域。
- 在极小缩放下仍不会因内容宽度不足导致交互异常。

---

## 6. 布局与绘制策略

### 6.1 marker 布局

- 同日第 `lane` 个作品：`center_y = date_axis_y - lane * DIAMOND_STRIDE`。
- 未知日期：统一放到右侧未知分区 `unk_x0` 上，也按 lane 向上叠放。
- 最终 marker 用独立 QWidget 放置到 canvas 上。

### 6.2 轴与刻度绘制

`TimelineCanvas.paintEvent` 做以下事：

- 背景填充、标尺分割线、日期主轴线。
- 根据 `ppd` 自适应刻度步长：
  - `_pick_tick_step_days(ppd, target_px)` 从“易读档位”选最近步长。
  - 主刻度与文字刻度独立计算。
- 仅绘制当前可见区域（带 `RULER_CLIP_PAD_PX` 外扩），避免超宽画布全量循环。
- 跨度大时标签显示 `%Y-%m`，跨度小显示 `%m-%d`。
- 有未知日期且有已知日期时，在未知区前绘制分隔线。

---

## 7. 缩放与平移行为实现

### 7.1 缩放锚点保持

入口：

- 滚轮缩放：`bump_zoom_from_wheel`
- slider 缩放：`_on_zoom_changed`

核心：`_apply_ppd_with_anchor(new_ppd, anchor_canvas_x)`  
逻辑是“重建前后保持 anchor 在视口中的相对位置不变”：

1. 记录旧 scroll 与锚点在视口中的位置。
2. 重建布局（可能导致 canvas 宽度变化）。
3. 分两种情况重算新锚点：
   - 锚点位于“已知日期区域”内：按 day index 比例映射。
   - 否则（如未知分区）：按整体画布宽度比例映射。
4. 重新设置 horizontal scrollbar，尽量保持用户视觉稳定。

### 7.2 中键平移

- 使用 `_middle_pan_active` 标记状态。
- `pan_scroll_by` 使用浮点余量累加（`_pan_remainder_h/_pan_remainder_v`），减少抖动和丢帧感。
- 中键平移期间：
  - 关闭滚动触发弹窗隐藏的信号连接（防闪烁）。
  - resize 导致的重建会延后，结束后再补做。

---

## 8. 悬浮卡片（Hover Popup）策略

- marker `enterEvent` 触发 `_on_marker_enter`，构建 `CoverCard2` 放入 `_HoverCardPopup`。
- popup 是 `Qt.ToolTip | Frameless | StaysOnTop`，允许鼠标进入。
- marker 离开后不立即关闭，而是 `POPUP_HIDE_MS` 延迟关闭，给用户移动到卡片的时间。
- 滚动时通过定时器防抖关闭弹窗（`_scroll_popup_hide_timer`）。
- 中键平移时直接关闭弹窗并暂停滚动隐藏监听，避免持续闪烁。

---

## 9. 首次定位默认时间窗

`set_work_rows` 后设置 `_pending_timeline_scroll_to_default = True`。  
重建完成后异步执行 `_try_apply_initial_timeline_scroll`：

1. 目标是把默认窗 `2016-01-01 ~ 2026-12-31` 的中心放在视口中心。
2. 处理一个现实问题：`setFixedSize` 后 scrollbar range 可能尚未同步。
3. 因此允许最多 64 次 `singleShot(0)` 重试，直到 range 足够可信。
4. 若 range 仍不可用，退化用 `ensureVisible` 尝试逼近中心。

这是当前实现保证“首次进入不跑到极左历史区域”的关键机制。

---

## 10. 主题与样式接入点

- `resolve_theme_manager` + `ThemeTokens` 获取颜色。
- `ActressWorkTimeline` 与 `TimelineCanvas` 都监听 `themeChanged`。
- marker 颜色优先使用 `tag_id -> _tag_color`，无效时回退主题主色。

---

## 11. 重写时建议保留的行为清单（验收基线）

1. 滚轮缩放时，鼠标下日期点“基本不漂移”。
2. `Ctrl/Shift/中键拖拽` 三种操作不冲突，滚动方向一致。
3. 有数据时首屏应落在默认窗中心附近，不应停在超早年份。
4. 极端缩放（很小/很大）下，轴与 marker 不错位，不消失。
5. 同日多作品有稳定顺序和垂直分层。
6. 未知日期始终在右侧独立区域，且可见分隔。
7. hover 卡片可从 marker 平滑移入，不频繁闪烁。
8. resize 时不应出现明显跳动，尤其在中键平移中。

---

## 12. 建议的重写拆分（可选）

如果准备重构为更可维护版本，建议拆成：

- `timeline_model.py`：数据解析、日期区间策略、marker 位置计算（纯逻辑）。
- `timeline_canvas.py`：仅负责绘制轴和刻度。
- `timeline_interaction.py`：缩放/平移/锚点换算。
- `timeline_widget.py`：组装 UI 与 popup 生命周期。

这样可把“纯计算”与“Qt 事件处理”解耦，便于加单元测试。

