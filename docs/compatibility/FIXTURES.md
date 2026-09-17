# 数据库 fixture 与黄金输出

所有 fixture 均由虚构数据生成，不包含真实用户路径、姓名或私人记录。

## 样本矩阵

| 文件 | 用途 |
|---|---|
| `public-empty-v2.db` | 当前公共库空库 |
| `private-empty-v1.1.db` | 当前私有库空库 |
| `public-legacy-v1.0.db` | 公共库 1.0 升级输入 |
| `private-legacy-v1.0.db` | 私有库 1.0 升级输入 |
| `public-typical-v2.db` | 两部作品及人物、标签、制作商关联 |
| `private-typical-v1.1.db` | 收藏与三类个人记录 |
| `unknown-v99.db` | 未知版本拒绝路径 |
| `public-corrupt-foreign-key.db` | 外键损坏拒绝路径 |

二进制位于 `tests/fixtures/databases/`，哈希记录在同目录
`SHA256SUMS`。典型数据源位于 `tests/fixtures/seeds/`，便于审查和重新生成。

## 第一批黄金查询

`tests/fixtures/golden/core-queries.json` 冻结：

- 全部作品 ID 与未删除作品 ID；
- 作品编号、标题、时长、软删除状态；
- 作品与女演员、男演员、标签关系；
- 私有收藏作品、收藏女演员；
- 2026 年按日个人记录计数。

`darkeye_database_fixture_tests` 只读打开固定数据库并与 JSON 比较，同时确认
历史版本识别、未知版本拒绝和外键损坏检测。后续仍需使用基线 Python 代码对同一
输入生成独立输出，并继续加入写入后状态、导入、HTTP、番号和路径规则。

重新生成二进制 fixture 时必须显式运行
`darkeye_fixture_database_builder tests/fixtures/databases`，随后审查语义差异并
更新哈希；禁止仅因测试失败而盲目覆盖 fixture。
