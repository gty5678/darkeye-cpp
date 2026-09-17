# 数据目录兼容合同

本合同冻结于 Python 基线提交
`ed804b7ae79d9d784d575a6e2562c7553e006f3a`。默认数据根目录为可执行文件
同级的 `data/`；测试可以用 `DARKEYE_DATA_DIR` 指向隔离目录。

## 用户拥有的数据

| 旧版真实路径 | 语义 | C++ 规则 |
|---|---|---|
| `data/settings.ini` | Qt INI 用户配置 | 原路径读写，不覆盖已有文件 |
| `data/shortcuts.json` | 快捷键 action ID 与键序列 | 原路径读写，UTF-8 |
| `data/crawler_nav_buttons.json` | 采集页外链按钮 | 原路径读写 |
| `data/actress_nav_buttons.json` | 女优页外链按钮 | 原路径读写 |
| `data/add_work_workspace_layout.json` | MyADS 工作区布局树 | 保持 data 根目录位置 |
| `data/public/public.db` | 公共资料库，Schema 版本 2 | 仅经数据库服务读写 |
| `data/public/public_backup/` | 公共库历史备份 | 保留真实旧目录名 |
| `data/public/workcovers/` | 作品封面 | 原目录升级，不改名 |
| `data/public/fanart/` | 作品剧照 | 原目录升级，不改名 |
| `data/public/actressimages/` | 女优图片 | 原目录升级，不改名 |
| `data/public/actorimages/` | 男优图片 | 原目录升级，不改名 |
| `data/private/private.db` | 私人记录库，Schema 版本 1.1 | 仅经数据库服务读写 |
| `data/private/private_backup/` | 私有库历史备份 | 保留真实旧目录名 |
| `data/temp/` | 可清理的任务临时文件 | 不作为唯一数据来源 |
| `data/cache/` | 可重建缓存 | 升级可重新生成 |
| `data/logs/` | C++ 日志 | 必须脱敏 |

旧版路径常量的来源是基线
`settings/paths.py`。历史实现会在自定义路径暂时不可访问时把设置重置为默认值；
这是已冻结缺陷，C++ 版不得复现。离线盘、网络盘或只读位置必须保留原设置并报告
可操作错误。

早期 C++ 骨架曾创建 `public/backups/`、`private/backups/` 和
`data/workspace/`。这些不是 Python 基线的正式目录，不作为迁移输入，也不得
自动删除；新版默认值已经改回 `public_backup/`、`private_backup/`，MyADS
布局使用 data 根目录的原文件名。

## 随程序发布的只读资源

以下内容属于 `resources/`，升级程序可以替换，不能与用户数据混放：

| 路径 | 用途 |
|---|---|
| `resources/config/tag_map.json` | 文本到标签的映射 |
| `resources/config/label.json` | 厂牌导入基线 |
| `resources/config/series.json` | 系列导入基线 |
| `resources/config/maker_prefix.json` | 番号前缀与制作商映射 |
| `resources/config/actors_cn_jp_export.json` | NFO 演员性别辅助名单 |
| `resources/config/sensitive_words.txt` | 绿色模式敏感词 |
| `resources/sql/` | 建库与版本迁移脚本 |
| `resources/icons/`、`styles/` | 图标和样式 |
| `resources/meshes/`、`maps/`、`hdr/` | 3D 场景资源 |
| `resources/avwiki/` | AVWiki 更新资源 |

首次启动可从只读默认模板创建缺失的用户 JSON，但不得在升级时用模板覆盖用户
修改。模板复制必须采用临时文件和原子替换。

## 路径解释

1. `settings.ini` 中未配置路径时使用上表的便携式默认值。
2. 相对路径按程序目录解析，保持 Python 基线行为。
3. 绝对路径原样保留；暂时不存在不等于用户要放弃该路径。
4. `Paths/Videos` 先按历史逗号分隔格式读取，忽略空片段但不擅自重写。
5. 路径必须覆盖中文、空格、长路径、多盘符、离线盘和只读目录测试。

## 升级与回滚

正式迁移执行：

`选择旧 data → 确认旧程序退出 → 在线备份 → quick_check/外键检查 →`
`复制到临时目录 → 打开副本验证 → 原子切换`。

- 不直接开发或测试用户唯一数据库。
- 不把 SQLite 的 `-wal`、`-shm` 当普通文件复制。
- Python 和 C++ 不得同时写同一数据库。
- 升级失败时不修改旧目录，用户仍可启动 Python 版。
- Preview 使用独立版本目录，默认不共用可写数据库。
