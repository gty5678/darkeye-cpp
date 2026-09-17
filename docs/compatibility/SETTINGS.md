# settings.ini 兼容合同

C++ 程序继续使用 Qt INI 格式，并保留历史键名，包括旧版拼写 `window/first_lunch`。

## 窗口和界面

| 键 | 默认值 |
|---|---|
| `window/size` | `800x600` |
| `window/pos` | `(100,100)` |
| `window/maximized` | `false` |
| `window/first_lunch` | `true` |
| `App/Theme` | `LIGHT` |
| `App/CustomPrimary` | 空 |
| `WorkPage/LargeCoverView` | `false` |
| `WorkPage/TagSelectorVisible` | `true` |
| `ShelfPage/TagSelectorVisible` | `true` |
| `Update/LastAutoCheckWeek` | 空 |

## 路径

- `Paths/Database`
- `Paths/DatabaseBackups`
- `Paths/Actressimages`
- `Paths/Actorimages`
- `Paths/WorkCovers`
- `Paths/Fanart`
- `Paths/PrivateDatabase`
- `Paths/PrivateDatabaseBackups`
- `Paths/Temp`
- `Paths/Videos`
- `Video/LocalPlayerExe`

未设置自定义路径时使用 `data/` 下的便携目录。自定义路径不得因暂时离线而被程序静默覆盖。

相对路径继续按程序目录解析；绝对路径原样使用。C++ 程序不会因为自定义位置暂时不可访问就改写 `settings.ini`。

## 信息补充器和 WebDAV

- `Crawler/WorkApiBaseUrl`
- `Crawler/ActressApiBaseUrl`
- `Crawler/CoverFetchApiUrl`
- `Crawler/TopActressesApiUrl`
- `Crawler/CollectorExe`
- `Crawler/AutoStartCollector`
- `WebDAV/Enabled`
- `WebDAV/ProfileName`
- `WebDAV/BaseUrl`
- `WebDAV/RemoteRoot`
- `WebDAV/TimeoutSeconds`
- `WebDAV/AutoUploadOnBackup`

## 翻译与 llama.cpp

- `Translation/Engine`
- `Translation/Model`
- `Translation/BaseUrl`
- `Translation/ApiKey`
- `Translation/TimeoutS`
- `Translation/Retries`
- `Translation/Fallback`
- `LlamaCpp/ServerExePath`
- `LlamaCpp/ModelPath`
- `LlamaCpp/Host`
- `LlamaCpp/Port`
- `LlamaCpp/Mode`
- `LlamaCpp/CtxSize`
- `LlamaCpp/GpuLayers`
- `LlamaCpp/Threads`
- `LlamaCpp/ThreadsBatch`
- `LlamaCpp/BatchSize`
- `LlamaCpp/UBatchSize`
- `LlamaCpp/Mlock`
- `LlamaCpp/AutoSyncTranslation`
- `LlamaCpp/AutoStart`

API 密钥后续迁移到系统凭据库；兼容读取旧 INI 值，但不再把新密钥明文写回 INI。
