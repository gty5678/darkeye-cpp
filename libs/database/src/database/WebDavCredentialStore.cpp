#include "database/WebDavCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace darkeye
{
namespace
{

QString normalizedProfile(const QString &profile)
{
    const QString value = profile.trimmed();
    return value.isEmpty() ? QStringLiteral("default") : value;
}

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) *errorMessage = message;
}

} // namespace

QString WebDavCredentialStore::serviceName(const QString &profile)
{
    return QStringLiteral("darkeye/webdav/") + normalizedProfile(profile);
}

bool WebDavCredentialStore::save(const QString &profile, const WebDavCredentials &credentials,
                                 QString *errorMessage)
{
    if (credentials.username.trimmed().isEmpty() || credentials.password.isEmpty())
    {
        setError(errorMessage, QStringLiteral("用户名和密码不能为空。"));
        return false;
    }
#ifdef Q_OS_WIN
    const QByteArray payload = QJsonDocument(QJsonObject{{QStringLiteral("username"), credentials.username.trimmed()},
                                                         {QStringLiteral("password"), credentials.password}})
                                   .toJson(QJsonDocument::Compact);
    const std::wstring target = serviceName(profile).toStdWString();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t *>(target.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(payload.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(payload.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<wchar_t *>(L"credentials");
    if (CredWriteW(&credential, 0)) return true;
    setError(errorMessage, QStringLiteral("写入系统凭据管理器失败（%1）。").arg(GetLastError()));
    return false;
#else
    Q_UNUSED(profile);
    setError(errorMessage, QStringLiteral("当前平台尚未实现系统凭据存储。"));
    return false;
#endif
}

std::optional<WebDavCredentials> WebDavCredentialStore::load(const QString &profile,
                                                               QString *errorMessage)
{
#ifdef Q_OS_WIN
    const std::wstring target = serviceName(profile).toStdWString();
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential))
    {
        const DWORD code = GetLastError();
        if (code != ERROR_NOT_FOUND)
            setError(errorMessage, QStringLiteral("读取系统凭据管理器失败（%1）。").arg(code));
        return std::nullopt;
    }
    const QByteArray payload(reinterpret_cast<const char *>(credential->CredentialBlob),
                             static_cast<qsizetype>(credential->CredentialBlobSize));
    CredFree(credential);
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) return std::nullopt;
    const QJsonObject object = document.object();
    const QString username = object.value(QStringLiteral("username")).toString().trimmed();
    const QString password = object.value(QStringLiteral("password")).toString();
    if (username.isEmpty() || password.isEmpty()) return std::nullopt;
    return WebDavCredentials{username, password};
#else
    Q_UNUSED(profile);
    setError(errorMessage, QStringLiteral("当前平台尚未实现系统凭据存储。"));
    return std::nullopt;
#endif
}

bool WebDavCredentialStore::has(const QString &profile)
{
    return load(profile).has_value();
}

bool WebDavCredentialStore::clear(const QString &profile, QString *errorMessage)
{
#ifdef Q_OS_WIN
    const std::wstring target = serviceName(profile).toStdWString();
    if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) return true;
    if (GetLastError() == ERROR_NOT_FOUND) return true;
    setError(errorMessage, QStringLiteral("清除系统凭据失败（%1）。").arg(GetLastError()));
    return false;
#else
    Q_UNUSED(profile);
    setError(errorMessage, QStringLiteral("当前平台尚未实现系统凭据存储。"));
    return false;
#endif
}

} // namespace darkeye
