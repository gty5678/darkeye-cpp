#include "database/SchemaManager.h"

#include "database/SqlScriptRunner.h"

#include <QSet>
#include <QMap>
#include <QSqlError>
#include <QSqlQuery>

namespace darkeye {

namespace {

QMap<QString, QStringList> requiredColumns(DatabaseKind kind)
{
    if (kind == DatabaseKind::Public) {
        return {
            {QStringLiteral("work"),
             {QStringLiteral("work_id"), QStringLiteral("serial_number"),
              QStringLiteral("runtime"), QStringLiteral("notes"),
              QStringLiteral("maker_id"), QStringLiteral("label_id"),
              QStringLiteral("series_id"), QStringLiteral("fanart"),
              QStringLiteral("is_deleted")}},
            {QStringLiteral("actress"),
             {QStringLiteral("actress_id"), QStringLiteral("notes"),
              QStringLiteral("need_update")}},
            {QStringLiteral("actor"),
             {QStringLiteral("actor_id"), QStringLiteral("notes")}},
            {QStringLiteral("label"),
             {QStringLiteral("label_id"), QStringLiteral("aliases")}},
            {QStringLiteral("series"),
             {QStringLiteral("series_id"), QStringLiteral("aliases")}},
            {QStringLiteral("tag"),
             {QStringLiteral("tag_id"), QStringLiteral("tag_name"),
              QStringLiteral("tag_type_id"), QStringLiteral("redirect_tag_id")}},
        };
    }
    return {
        {QStringLiteral("favorite_actress"),
         {QStringLiteral("favorite_actress_id"), QStringLiteral("actress_id"),
          QStringLiteral("jp_name"), QStringLiteral("added_time")}},
        {QStringLiteral("favorite_work"),
         {QStringLiteral("favorite_work_id"), QStringLiteral("work_id"),
          QStringLiteral("serial_number"), QStringLiteral("added_time")}},
        {QStringLiteral("masturbation"),
         {QStringLiteral("masturbation_id"), QStringLiteral("work_id"),
          QStringLiteral("serial_number"), QStringLiteral("start_time"),
          QStringLiteral("rating")}},
        {QStringLiteral("love_making"),
         {QStringLiteral("love_making_id"), QStringLiteral("event_time"),
          QStringLiteral("rating")}},
        {QStringLiteral("sexual_arousal"),
         {QStringLiteral("sexual_arousal_id"), QStringLiteral("arousal_time")}},
    };
}

} // namespace

QString SchemaManager::requiredVersion(DatabaseKind kind)
{
    return kind == DatabaseKind::Public ? QStringLiteral("2") : QStringLiteral("1.1");
}

QStringList SchemaManager::requiredTables(DatabaseKind kind)
{
    if (kind == DatabaseKind::Public) {
        return {
            QStringLiteral("actor"),
            QStringLiteral("actor_name"),
            QStringLiteral("actress"),
            QStringLiteral("actress_name"),
            QStringLiteral("label"),
            QStringLiteral("maker"),
            QStringLiteral("prefix_maker_relation"),
            QStringLiteral("series"),
            QStringLiteral("tag"),
            QStringLiteral("tag_type"),
            QStringLiteral("work"),
            QStringLiteral("work_actor_relation"),
            QStringLiteral("work_actress_relation"),
            QStringLiteral("work_tag_relation"),
        };
    }
    return {
        QStringLiteral("db_version"),
        QStringLiteral("favorite_actress"),
        QStringLiteral("favorite_work"),
        QStringLiteral("love_making"),
        QStringLiteral("masturbation"),
        QStringLiteral("sexual_arousal"),
    };
}

bool SchemaManager::initializeEmptyDatabase(SqliteConnection &connection, DatabaseKind kind,
                                            QString *errorMessage)
{
    if (!connection.isOpen() || connection.isReadOnly()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("初始化需要已打开的可写数据库");
        }
        return false;
    }
    if (!isEmpty(connection.database())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("拒绝在非空数据库上执行初始化脚本");
        }
        return false;
    }

    const QString resourcePath = kind == DatabaseKind::Public
                                     ? QStringLiteral(":/sql/initPublicTable.sql")
                                     : QStringLiteral(":/sql/initPrivateTable.sql");
    if (!SqlScriptRunner::executeResource(connection.database(), resourcePath, errorMessage)) {
        return false;
    }
    return validateCurrentSchema(connection, kind, errorMessage);
}

bool SchemaManager::migrateToCurrent(SqliteConnection &connection, DatabaseKind kind,
                                     QString *errorMessage)
{
    if (!connection.isOpen() || connection.isReadOnly()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("迁移需要已打开的可写数据库");
        }
        return false;
    }
    if (isEmpty(connection.database())) {
        return initializeEmptyDatabase(connection, kind, errorMessage);
    }

    const QString currentVersion = connection.schemaVersion(errorMessage).trimmed();
    if (currentVersion == requiredVersion(kind)) {
        return validateCurrentSchema(connection, kind, errorMessage);
    }
    if (currentVersion != QStringLiteral("1.0")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("不支持的数据库版本：%1（目标版本 %2）")
                                .arg(currentVersion.isEmpty() ? QStringLiteral("未知")
                                                              : currentVersion,
                                     requiredVersion(kind));
        }
        return false;
    }

    const bool migrated = kind == DatabaseKind::Public
                              ? migratePublicFromV1(connection, errorMessage)
                              : migratePrivateFromV1(connection, errorMessage);
    return migrated && validateCurrentSchema(connection, kind, errorMessage);
}

bool SchemaManager::validateCurrentSchema(const SqliteConnection &connection, DatabaseKind kind,
                                          QString *errorMessage)
{
    if (!connection.isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("数据库尚未打开");
        }
        return false;
    }

    QSqlQuery integrityQuery(connection.database());
    if (!integrityQuery.exec(QStringLiteral("PRAGMA quick_check")) || !integrityQuery.next()
        || integrityQuery.value(0).toString().compare(QStringLiteral("ok"),
                                                      Qt::CaseInsensitive) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = integrityQuery.lastError().isValid()
                                ? QStringLiteral("数据库完整性检查失败：%1")
                                      .arg(integrityQuery.lastError().text())
                                : QStringLiteral("数据库完整性检查失败：%1")
                                      .arg(integrityQuery.value(0).toString());
        }
        return false;
    }

    QSqlQuery foreignKeyQuery(connection.database());
    if (!foreignKeyQuery.exec(QStringLiteral("PRAGMA foreign_key_check"))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("外键检查无法执行：%1")
                                .arg(foreignKeyQuery.lastError().text());
        }
        return false;
    }
    if (foreignKeyQuery.next()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("检测到外键损坏：表 %1，第 %2 行，引用表 %3")
                                .arg(foreignKeyQuery.value(0).toString(),
                                     foreignKeyQuery.value(1).toString(),
                                     foreignKeyQuery.value(2).toString());
        }
        return false;
    }

    const QStringList tableList = connection.database().tables(QSql::Tables);
    const QSet<QString> existingTables(tableList.cbegin(), tableList.cend());
    QStringList missingTables;
    for (const QString &requiredTable : requiredTables(kind)) {
        if (!existingTables.contains(requiredTable)) {
            missingTables.append(requiredTable);
        }
    }
    if (!missingTables.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("数据库缺少必要表：%1").arg(missingTables.join(", "));
        }
        return false;
    }

    const QMap<QString, QStringList> columnRequirements = requiredColumns(kind);
    for (auto table = columnRequirements.cbegin(); table != columnRequirements.cend(); ++table) {
        QSqlQuery columnQuery(connection.database());
        if (!columnQuery.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(table.key()))) {
            if (errorMessage != nullptr) {
                *errorMessage = columnQuery.lastError().text();
            }
            return false;
        }
        QSet<QString> existingColumns;
        while (columnQuery.next()) {
            existingColumns.insert(columnQuery.value(1).toString());
        }
        QStringList missingColumns;
        for (const QString &column : table.value()) {
            if (!existingColumns.contains(column)) {
                missingColumns.append(column);
            }
        }
        if (!missingColumns.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("表 %1 缺少必要字段：%2")
                                    .arg(table.key(), missingColumns.join(", "));
            }
            return false;
        }
    }

    const QString version = connection.schemaVersion(errorMessage).trimmed();
    if (version != requiredVersion(kind)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("数据库版本 %1 与目标版本 %2 不一致")
                                .arg(version, requiredVersion(kind));
        }
        return false;
    }
    return true;
}

bool SchemaManager::isEmpty(const QSqlDatabase &database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' LIMIT 1"))) {
        return false;
    }
    return !query.next();
}

bool SchemaManager::migratePublicFromV1(SqliteConnection &connection, QString *errorMessage)
{
    QSqlDatabase database = connection.database();
    if (!SqlScriptRunner::executeResource(database,
                                          QStringLiteral(":/sql/public/v1.0-v2/migration.sql"),
                                          errorMessage)) {
        return false;
    }

    QSqlQuery versionTableQuery(database);
    if (!versionTableQuery.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS db_version("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, version TEXT NOT NULL, "
            "applied_at DATETIME DEFAULT (datetime('now', 'localtime')), description TEXT)"))) {
        if (errorMessage != nullptr) {
            *errorMessage = versionTableQuery.lastError().text();
        }
        return false;
    }
    versionTableQuery.prepare(QStringLiteral(
        "INSERT INTO db_version(version, description) VALUES(?, ?)"));
    versionTableQuery.addBindValue(QStringLiteral("2"));
    versionTableQuery.addBindValue(QStringLiteral("C++ 迁移器完成公共库 1.0 → 2"));
    if (!versionTableQuery.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = versionTableQuery.lastError().text();
        }
        return false;
    }
    return true;
}

bool SchemaManager::migratePrivateFromV1(SqliteConnection &connection, QString *errorMessage)
{
    QSqlDatabase database = connection.database();
    if (!database.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }

    if (!SqlScriptRunner::executeResource(
            database, QStringLiteral(":/sql/private/v1.0-v1.1/RBfavorite_actress.sql"),
            errorMessage)
        || !SqlScriptRunner::executeResource(
            database, QStringLiteral(":/sql/private/v1.0-v1.1/RBfavorite_work.sql"),
            errorMessage)) {
        database.rollback();
        return false;
    }

    QSqlQuery versionQuery(database);
    versionQuery.prepare(QStringLiteral(
        "INSERT INTO db_version(version, description) VALUES(?, ?)"));
    versionQuery.addBindValue(QStringLiteral("1.1"));
    versionQuery.addBindValue(QStringLiteral("重建收藏表，取消旧唯一约束"));
    if (!versionQuery.exec()) {
        database.rollback();
        if (errorMessage != nullptr) {
            *errorMessage = versionQuery.lastError().text();
        }
        return false;
    }

    if (!database.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }
    return true;
}

} // namespace darkeye
