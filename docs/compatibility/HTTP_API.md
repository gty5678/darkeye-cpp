# 本地 HTTP API 兼容合同

监听地址默认为 `127.0.0.1:56789`。C++ 实现需支持 CORS，并保持现有 JSON 字段。

| 方法 | 路径 | 成功响应/行为 |
|---|---|---|
| GET | `/api/v1/health` | `{"status":"ok","service":"DarkEye Server"}` |
| POST | `/api/v1/check_existence` | 接收 `{"items":[string]}`，返回保留原始键的 `results` 布尔映射 |
| POST | `/api/v1/minnano-actress-capture` | 接收任意 JSON 对象，排队发送至 GUI |
| POST | `/api/v1/capture/one` | 读取必需字段 `content`，将字符串排队送入爬虫 |
| POST | `/api/v1/crawler-backlog-warning` | 接收 `count`、`browser`、`threshold`；低于阈值时返回 `ignored` |
| POST | `/api/v1/cloudflare-challenge-notify` | 接收可选的站点、阶段、URL、番号和合并请求 ID |

## 错误兼容

- JSON 校验失败返回 4xx。
- 数据库或内部处理失败返回 500，并包含 `detail` 字段。
- 数据库存在性检查忽略大小写和输入首尾空白。
- HTTP 工作线程不得直接访问 QWidget。

