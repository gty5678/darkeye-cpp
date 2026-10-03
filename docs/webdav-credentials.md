# WebDAV 系统凭据

C++ 版本使用系统后端保存凭据，沿用 Python `keyring` 版本的数据约定：

- service：`darkeye/webdav/<profile>`，profile 去除首尾空白，空值使用 `default`。
- account / username 属性：固定为 `credentials`。
- secret：包含 `username` 和 `password` 的 JSON；用户名去除首尾空白，密码保持原样。

| 平台 | 后端 | 构建及运行要求 |
| --- | --- | --- |
| Windows | Credential Manager | 链接系统 Advapi32；写入 UTF-16LE，同时读取旧 C++ UTF-8 数据 |
| macOS | Keychain，generic password | 链接系统 Security、CoreFoundation；系统可能要求授予钥匙串访问权限 |
| Linux / Unix | Secret Service，通过 libsecret | 构建需要 pkg-config、libsecret-1 开发包；运行需要用户 D-Bus 会话及可用的 Secret Service 密钥环 |

Linux 构建前，例如 Debian / Ubuntu 安装 `pkg-config libsecret-1-dev`，Fedora 安装 `pkgconf-pkg-config libsecret-devel`。密钥环未启动、拒绝访问或解锁失败会返回后端错误，不会退回明文文件存储。

macOS 和 Linux 使用与 Python 默认 Keychain / SecretService 后端相同的查询字段，可读取这些后端保存的凭据。Python 配置的第三方后端、其他 SecretService scheme 或旧式独立 KWallet 后端不在此兼容范围内。此支持范围描述仅针对凭据模块，不代表整套应用已通过各平台验证。

测试目标为 `darkeye_webdav_credential_tests`。使用随机测试 profile 并在测试后清除，不使用真实 WebDAV 凭据。Windows 默认执行系统存取及 Python/旧 C++ 编码兼容测试；macOS、Linux 在桌面会话中设置 `DARKEYE_TEST_SYSTEM_KEYRING=1` 后执行系统存取测试。
