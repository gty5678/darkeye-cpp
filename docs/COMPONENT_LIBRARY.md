# C++ 公共组件兼容面

本清单以 Python `darkeye_ui/components/__init__.py::__all__` 与
`darkeye_ui/doc.md` 为基线。C++ 页面可统一包含：

```cpp
#include "darkeye_ui/DarkeyeUi.h"
```

对应独立 CMake 目标为 `darkeye::darkeye_ui`。该目标不包含作品、人物、标签、
数据库或采集器相关业务组件；业务页面继续使用 `darkeye::ui`。

## 基础、输入与选择

`Button`、`Label`、`LineEdit`、`TextEdit`、`PlainTextEdit`、`ComboBox`、
`CompleterLineEdit`、`TokenSpinBox`、`TokenDateTimeEdit`、
`TokenKeySequenceEdit`、`TokenCheckBox`、`TokenRadioButton`、`ColorPicker`、
`ClickableSlider`。

## 数据视图、布局与导航

`TokenListView`、`TokenListWidget`、`TokenTableView`、`TokenTableWidget`、
`TreeView`、`TokenTreeView`、`TokenTabWidget`、`TokenGroupBox`、
`TokenCollapsibleSection`、`TokenVerticalTabBar`、`TokenLinkCard`、`Sidebar`、
`Sidebar2`、`ModernScrollMenu`、`LazyScrollArea`、`Breadcrumb`、`Pagination`、
`SearchBar`、`TransparentWidget`。

## 按钮、反馈与视觉组件

`IconPushButton`、`ChamferButton`、`RotateButton`、`ShakeButton`、
`StateToggleButton`、`ToggleSwitch`、`Chip`、`Tag`、`ProgressBar`、
`IndeterminateProgressBar`、`TokenProgressBar`、`ModalDialog`、`Dialog`、
`TokenModalDialog`、`Toast`、`Notification`、`Skeleton`、`EmptyState`、
`CircularLoading`、`CalloutTooltip`、`HeartLabel`、`HeartRatingWidget`、
`Avatar`、`AvatarGroup`、`OctImage`、`VerticalTextLabel`、`TokenVLabel`、
`CalendarHeatmap`、`RadarChartWidget`。

Python 中 `color_slider.py` 的 `AlphaSliderCustom` 和 `TestWindow` 被原组件文档
明确列为非公共内部原型，不属于兼容范围。

## 业务复合组件

`ui/widgets/PersonCard` 复用 `OctImage` 与令牌标签，统一承载女演员/男演员的
150px 八边形头像卡片，并通过左键、右键分别发出详情与编辑请求。
`PersonInfoPanel` 组合头像、爱心、令牌表格和资料字段，`ModifyActressPage` 与
`ModifyActorPage` 则统一
女演员/男演员的字段编辑、姓名增删和顺序调整。它们属于页面级业务组件，不加入
Python `darkeye_ui.components.__all__` 对应的公共聚合头。

`ui/widgets/ImageDropWidget` 复用异步图片预览，封装点击选择、拖放、清除、脏状态
提示、图片格式校验以及原子 JPEG 落盘。控件可配置“人物头像”“作品封面”等用途，
相应更新占位、文件对话框、菜单和错误信息；人物编辑器与作品编辑器共用同一套文件
接收与验证行为，并分别保持旧版头像及封面的相对路径和命名合同。

`ui/widgets/FanartStripWidget` 是可复用的横向剧照工作区，按稳定顺序保存远程 URL、
相对文件名和仅编辑期存在的本地源路径。它提供 JSON 解析/序列化、缩略图、添加、
网址编辑、删除、换序及待提交图片 JPEG 落盘；相同 URL 列表重新同步时保留已有
`file` 下载状态，并兼容从旧封面目录回退读取历史剧照。远程条目复用
`ImageFetchService` 异步下载，可取消且单工作区同一时间只允许一个请求；服务与旧版
一致通过可配置的本地代理接收 Base64 图片，并统一执行响应限额、超时、图片解码和
原子 JPEG 写入。下载成功后组件发出普通 `fanartChanged`，调用方无需维护额外状态。

`ui/widgets/ReferenceManagementWidget` 将片商、厂牌和系列的表格、字段编辑、删除
保护及双选择器重定向收敛为一个按 `ReferenceKind` 配置的业务组件。顶层
`ManagementPage` 仅负责建立对应标签页并转发资料变更事件，SQL 与重定向事务均
留在 `ReferenceRepository`。`MakerPrefixManagementWidget` 独立封装片商前缀映射的
表格和编辑器，并在片商资料改变后同步刷新选择项。

`ui/components/JsonTransferBar` 是参考资料页共用的轻量文件操作栏，统一 JSON 文件
选择、默认文件名和导入/导出信号；格式验证、原子写入和数据库事务由
`ReferenceJsonService` 负责，因此组件本身不持有业务数据。

`TagManagementWidget` 组合公共 `ColorPicker`、`TokenVLabel`、令牌表格及输入组件，
提供标签实时预览、字段/别名编辑和双选择器重定向；`TagTypeManagementWidget` 则
封装标签类型的增删改及顺序调整。两个组件只通过 `ReferenceRepository` 写库，并
向 `ManagementPage` 发出刷新事件。

`WorkBatchStateWidget` 以 `WorkStateMode` 配置活动作品或回收站模式，共用搜索、
复选表格、全选和批量操作结构。组件只调用 `WorkRepository` 的事务接口；活动模式
负责软删除，回收站模式负责恢复及永久删除，并在成功后发出统一 `worksChanged`
事件供管理页和作品页刷新。永久删除提交数据库后会分别清理封面与 fanart 目录，
两类资源都只接受经目录边界校验的安全相对路径。

`WorkMaintenanceWidget` 对齐旧版批量操作页，将库级维护动作集中为统一入口。当前通过
`WorkMaintenanceService` 提供番号前缀片商回填和封面文件名规范化，两者都返回结构化
计数摘要；封面操作同时校验受管目录边界，并在数据库失败时逆序恢复全部文件移动。
依赖采集器或翻译器的按钮保留原名称和说明，但在对应服务迁移前保持禁用。

`pages/management/AddWorkTabPage3` 封装新增与修改作品共用的基础资料表单、封面导入和参考资料选择器。
创建模式允许填写番号并调用完整插入事务，编辑模式载入已有记录并锁定番号；保存后
通过 `workSaved` 统一通知作品页、管理页和状态列表。人物、男优和标签关系复用
`IdCheckList`，由单一事务和基础字段一起保存。新封面按番号原子写入受管理目录，
数据库失败时恢复原文件；`FanartStripWidget` 作为第四个页签组合其中，本地剧照与
数据库提交共用回滚边界，而不再回填页面专用表单。

`ui/components/IdCheckList` 为任意稳定整数 ID 提供标题、搜索、分组显示和复选状态，
过滤时不会丢失隐藏项选择。它不依赖具体 Repository，可供作品人物/标签关系及后续
批量编辑页面复用。
