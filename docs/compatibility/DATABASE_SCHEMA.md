# SQLite Schema 兼容基线

基线来源为 Python 提交
`ed804b7ae79d9d784d575a6e2562c7553e006f3a`，快照格式版本为 1。

| 数据库 | 逻辑版本 | `PRAGMA user_version` | 完整快照 |
|---|---:|---:|---|
| 公共库 | `2` | `2` | `tests/fixtures/schema/public-v2.json` |
| 私有库 | `1.1` | `0` | `tests/fixtures/schema/private-v1.1.json` |

私有库历史脚本没有设置 `PRAGMA user_version`，因此其值为 0；逻辑版本必须从
`db_version` 的最新记录读取。这是兼容行为，不得在无迁移版本的情况下擅自
改成 1 或 11。

快照由实际初始化后的 SQLite 数据库生成，记录：

- 每个表的字段顺序、声明类型、NOT NULL、默认值、主键位置和 hidden 标记；
- 外键的来源列、目标表/列及 update/delete/match 行为；
- 显式和隐式索引、唯一性、来源、partial 标记、排序和 collation；
- 表、视图和触发器在 `sqlite_schema` 中的完整 SQL；
- SQLite `user_version`。

CTest 中的 `darkeye_schema_snapshot_tests` 会重新建空库并逐项比较完整 JSON。
任何字段、约束、索引、视图或触发器变化都必须通过显式数据库迁移和兼容评审，
不能直接更新快照掩盖差异。

## 已知迁移顺序

### 公共库

1. 历史 `1.0`。
2. 执行 `resources/sql/public/v1.0-v2/migration.sql`。
3. 恢复历史脚本删除的 `db_version` 表并写入版本 `2`。
4. 校验 quick_check、外键、完整快照和目标版本。

### 私有库

1. 历史 `1.0`。
2. 在单一事务中重建 `favorite_actress`。
3. 在同一事务中重建 `favorite_work`。
4. 写入 `db_version=1.1` 后提交。
5. 校验 quick_check、完整快照和目标版本。

任一步失败必须回滚。迁移前使用 SQLite 在线备份生成一致性副本，不复制活动的
`-wal` 或 `-shm` 文件。
