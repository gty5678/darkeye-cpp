#include "database/DatabaseMaintenanceService.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace darkeye
{
namespace
{

QString timestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HH-mm-ss"));
}

QString quotedIdentifier(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool copyTree(const QString &source, const QString &destination, QString *errorMessage)
{
    const QDir sourceDir(source);
    if (!sourceDir.exists()) return true;
    if (!QDir().mkpath(destination))
    {
        if (errorMessage) *errorMessage = QStringLiteral("无法创建目录：%1").arg(destination);
        return false;
    }

    QDirIterator iterator(source, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString sourceFile = iterator.next();
        const QString relativePath = sourceDir.relativeFilePath(sourceFile);
        const QString destinationFile = QDir(destination).filePath(relativePath);
        if (!QDir().mkpath(QFileInfo(destinationFile).absolutePath()))
        {
            if (errorMessage) *errorMessage = QStringLiteral("无法创建目录：%1")
                                                .arg(QFileInfo(destinationFile).absolutePath());
            return false;
        }
        QFile::remove(destinationFile);
        if (!QFile::copy(sourceFile, destinationFile))
        {
            if (errorMessage) *errorMessage = QStringLiteral("无法复制文件：%1").arg(sourceFile);
            return false;
        }
    }
    return true;
}

QStringList userTables(QSqlDatabase database, const QString &schema = {})
{
    const QString prefix = schema.isEmpty() ? QString() : schema + QLatin1Char('.');
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT name FROM %1sqlite_master WHERE type='table' "
                                   "AND name NOT LIKE 'sqlite_'").arg(prefix)))
        return {};
    QStringList tables;
    while (query.next()) tables.append(query.value(0).toString());
    return tables;
}

DatabaseMaintenanceResult failure(const QString &message)
{
    return {.succeeded = false, .message = message};
}

} // namespace

DatabaseMaintenanceResult DatabaseMaintenanceService::createBackup(
    QSqlDatabase database, const QString &backupDirectory, const QString &prefix)
{
    if (!database.isOpen()) return failure(QStringLiteral("数据库尚未打开。"));
    if (!QDir().mkpath(backupDirectory))
        return failure(QStringLiteral("无法创建备份目录：%1").arg(backupDirectory));

    const QString name = QStringLiteral("%1-backup-%2.db").arg(prefix, timestamp());
    const QString backupPath = QDir(backupDirectory).filePath(name);
    QString escapedPath = QFileInfo(backupPath).absoluteFilePath();
    escapedPath.replace(QLatin1Char('\''), QStringLiteral("''"));
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("VACUUM main INTO '%1'").arg(escapedPath)))
        return failure(QStringLiteral("数据库备份失败：%1").arg(query.lastError().text()));
    QFileInfo backupInfo(backupPath);
    if (!backupInfo.isFile() || backupInfo.size() <= 0)
        return failure(QStringLiteral("数据库备份未生成有效文件：%1").arg(backupPath));
    return {.succeeded = true,
            .message = QStringLiteral("本地备份成功。"),
            .outputPath = backupPath};
}

DatabaseMaintenanceResult DatabaseMaintenanceService::restoreBackup(QSqlDatabase database,
                                                                     const QString &backupPath)
{
    if (!database.isOpen()) return failure(QStringLiteral("数据库尚未打开。"));
    if (!QFileInfo(backupPath).isFile())
        return failure(QStringLiteral("备份文件不存在：%1").arg(backupPath));

    QSqlQuery attach(database);
    attach.prepare(QStringLiteral("ATTACH DATABASE ? AS restore_source"));
    attach.addBindValue(backupPath);
    if (!attach.exec()) return failure(QStringLiteral("无法打开备份：%1").arg(attach.lastError().text()));

    const auto detach = [&database] {
        QSqlQuery query(database);
        query.exec(QStringLiteral("DETACH DATABASE restore_source"));
    };
    const QStringList tables = userTables(database, QStringLiteral("restore_source"));
    if (tables.isEmpty())
    {
        detach();
        return failure(QStringLiteral("备份中没有可恢复的数据表。"));
    }

    QSqlQuery pragma(database);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys=OFF")) || !database.transaction())
    {
        detach();
        return failure(QStringLiteral("无法开始恢复事务：%1").arg(database.lastError().text()));
    }
    for (const QString &table : tables)
    {
        const QString identifier = quotedIdentifier(table);
        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("DELETE FROM %1").arg(identifier))
            || !query.exec(QStringLiteral("INSERT INTO %1 SELECT * FROM restore_source.%1")
                               .arg(identifier)))
        {
            const QString errorMessage = query.lastError().text();
            database.rollback();
            QSqlQuery restorePragma(database);
            restorePragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
            detach();
            return failure(QStringLiteral("恢复失败：%1").arg(errorMessage));
        }
    }
    if (!database.commit())
    {
        const QString errorMessage = database.lastError().text();
        QSqlQuery restorePragma(database);
        restorePragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
        detach();
        return failure(QStringLiteral("恢复提交失败：%1").arg(errorMessage));
    }
    QSqlQuery restorePragma(database);
    restorePragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    detach();
    QSqlQuery checkpoint(database);
    checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"));
    return {.succeeded = true, .message = QStringLiteral("数据库恢复完成。")};
}

DatabaseMaintenanceResult DatabaseMaintenanceService::backupAndVacuum(
    QSqlDatabase publicDatabase, QSqlDatabase privateDatabase, const QString &publicBackupDirectory,
    const QString &privateBackupDirectory)
{
    const auto publicBackup = createBackup(publicDatabase, publicBackupDirectory,
                                           QStringLiteral("darkeye-public-pre-vacuum"));
    if (!publicBackup.succeeded) return publicBackup;
    const auto privateBackup = createBackup(privateDatabase, privateBackupDirectory,
                                            QStringLiteral("darkeye-private-pre-vacuum"));
    if (!privateBackup.succeeded) return privateBackup;

    for (const QSqlDatabase &database : {publicDatabase, privateDatabase})
    {
        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("VACUUM")))
            return failure(QStringLiteral("清理数据库碎片失败：%1").arg(query.lastError().text()));
    }
    return {.succeeded = true,
            .message = QStringLiteral("公共库和私库已备份并完成碎片整理。")};
}

DatabaseMaintenanceResult DatabaseMaintenanceService::createPublicSnapshot(
    QSqlDatabase database, const QString &snapshotRoot, const QString &workCoversDirectory,
    const QString &fanartDirectory, const QString &actressImagesDirectory,
    const QString &actorImagesDirectory)
{
    const QString snapshotDirectory = QDir(snapshotRoot).filePath(
        QStringLiteral("snapshot-%1").arg(timestamp()));
    if (!QDir().mkpath(snapshotDirectory))
        return failure(QStringLiteral("无法创建快照目录：%1").arg(snapshotDirectory));

    auto backup = createBackup(database, snapshotDirectory, QStringLiteral("public"));
    if (!backup.succeeded) return backup;
    const QList<QPair<QString, QString>> resources = {
        {workCoversDirectory, QStringLiteral("workcovers")},
        {fanartDirectory, QStringLiteral("fanart")},
        {actressImagesDirectory, QStringLiteral("actressimages")},
        {actorImagesDirectory, QStringLiteral("actorimages")},
    };
    for (const auto &[source, name] : resources)
    {
        QString errorMessage;
        if (!copyTree(source, QDir(snapshotDirectory).filePath(name), &errorMessage))
            return failure(errorMessage);
    }
    QJsonObject metadata;
    metadata.insert(QStringLiteral("version"), 1);
    metadata.insert(QStringLiteral("database"), QFileInfo(backup.outputPath).fileName());
    QJsonArray resourceNames;
    for (const auto &[source, name] : resources) resourceNames.append(name);
    metadata.insert(QStringLiteral("resources"), resourceNames);
    QFile meta(QDir(snapshotDirectory).filePath(QStringLiteral("meta.json")));
    if (!meta.open(QIODevice::WriteOnly)) return failure(QStringLiteral("无法写入快照元数据。"));
    meta.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
    return {.succeeded = true, .message = QStringLiteral("公共库完整快照成功。"),
            .outputPath = snapshotDirectory};
}

DatabaseMaintenanceResult DatabaseMaintenanceService::restorePublicSnapshot(
    QSqlDatabase database, const QString &metaPath, const QString &workCoversDirectory,
    const QString &fanartDirectory, const QString &actressImagesDirectory,
    const QString &actorImagesDirectory)
{
    QFile meta(metaPath);
    if (!meta.open(QIODevice::ReadOnly)) return failure(QStringLiteral("无法读取快照元数据。"));
    const QJsonDocument document = QJsonDocument::fromJson(meta.readAll());
    if (!document.isObject() || document.object().value(QStringLiteral("version")).toInt() != 1)
        return failure(QStringLiteral("不支持的快照元数据。"));
    const QDir snapshotDirectory(QFileInfo(metaPath).absolutePath());
    const QString databaseName = document.object().value(QStringLiteral("database")).toString();
    const auto restored = restoreBackup(database, snapshotDirectory.filePath(databaseName));
    if (!restored.succeeded) return restored;
    const QList<QPair<QString, QString>> resources = {
        {QStringLiteral("workcovers"), workCoversDirectory}, {QStringLiteral("fanart"), fanartDirectory},
        {QStringLiteral("actressimages"), actressImagesDirectory}, {QStringLiteral("actorimages"), actorImagesDirectory},
    };
    for (const auto &[name, destination] : resources)
    {
        QString errorMessage;
        if (!copyTree(snapshotDirectory.filePath(name), destination, &errorMessage)) return failure(errorMessage);
    }
    return {.succeeded = true, .message = QStringLiteral("公共库及图片快照恢复完成。")};
}

DatabaseMaintenanceResult DatabaseMaintenanceService::checkImageConsistency(
    QSqlDatabase database, const QString &directory, const QString &table, const QString &column)
{
    QSet<QString> referencedFiles;
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT %1 FROM %2 WHERE %1 IS NOT NULL AND %1 != ''")
                        .arg(quotedIdentifier(column), quotedIdentifier(table))))
        return failure(QStringLiteral("读取图片记录失败：%1").arg(query.lastError().text()));
    while (query.next())
    {
        const QString path = query.value(0).toString();
        if (!path.isEmpty()) referencedFiles.insert(QFileInfo(path).fileName());
    }
    QSet<QString> localFiles;
    QDirIterator iterator(directory, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) localFiles.insert(QFileInfo(iterator.next()).fileName());
    const int missing = (referencedFiles - localFiles).size();
    const int extra = (localFiles - referencedFiles).size();
    return {.succeeded = true,
            .message = QStringLiteral("%1：缺失 %2 个，多余 %3 个。").arg(table).arg(missing).arg(extra),
            .missingFiles = missing, .extraFiles = extra};
}

DatabaseMaintenanceResult DatabaseMaintenanceService::rebuildPrivateLinks(
    QSqlDatabase publicDatabase, const QString &privateDatabasePath)
{
    if (!publicDatabase.isOpen()) return failure(QStringLiteral("公共数据库尚未打开。"));
    QSqlQuery attach(publicDatabase);
    attach.prepare(QStringLiteral("ATTACH DATABASE ? AS private_db"));
    attach.addBindValue(privateDatabasePath);
    if (!attach.exec()) return failure(QStringLiteral("无法附加私有库：%1").arg(attach.lastError().text()));
    const auto detach = [&publicDatabase] { QSqlQuery(publicDatabase).exec(QStringLiteral("DETACH DATABASE private_db")); };
    if (!publicDatabase.transaction())
    {
        detach();
        return failure(QStringLiteral("无法开始重建事务：%1").arg(publicDatabase.lastError().text()));
    }
    int createdWorks = 0;
    int createdActresses = 0;
    const auto fail = [&](const QString &error) {
        publicDatabase.rollback();
        detach();
        return failure(error);
    };
    QSqlQuery actresses(publicDatabase);
    if (!actresses.exec(QStringLiteral("SELECT favorite_actress_id, actress_id, jp_name FROM private_db.favorite_actress")))
        return fail(actresses.lastError().text());
    while (actresses.next())
    {
        const qint64 favoriteId = actresses.value(0).toLongLong();
        const qint64 oldId = actresses.value(1).toLongLong();
        const QString japaneseName = actresses.value(2).toString();
        if (japaneseName.isEmpty()) continue;
        QSqlQuery find(publicDatabase);
        find.prepare(QStringLiteral("SELECT actress_id FROM actress_name WHERE jp=? LIMIT 1"));
        find.addBindValue(japaneseName);
        if (!find.exec()) return fail(find.lastError().text());
        qint64 newId = 0;
        if (find.next()) newId = find.value(0).toLongLong();
        else
        {
            QSqlQuery insertActress(publicDatabase);
            if (!insertActress.exec(QStringLiteral("INSERT INTO actress DEFAULT VALUES"))) return fail(insertActress.lastError().text());
            newId = insertActress.lastInsertId().toLongLong();
            QSqlQuery insertName(publicDatabase);
            insertName.prepare(QStringLiteral("INSERT INTO actress_name(actress_id,name_type,cn,jp) VALUES(?,1,?,?)"));
            insertName.addBindValue(newId); insertName.addBindValue(japaneseName); insertName.addBindValue(japaneseName);
            if (!insertName.exec()) return fail(insertName.lastError().text());
            ++createdActresses;
        }
        if (newId != oldId)
        {
            QSqlQuery update(publicDatabase);
            update.prepare(QStringLiteral("UPDATE private_db.favorite_actress SET actress_id=? WHERE favorite_actress_id=?"));
            update.addBindValue(newId); update.addBindValue(favoriteId);
            if (!update.exec()) return fail(update.lastError().text());
        }
    }
    const auto rebuildWorkTable = [&](const QString &table, const QString &primaryKey) -> QString {
        QSqlQuery rows(publicDatabase);
        if (!rows.exec(QStringLiteral("SELECT %1, work_id, serial_number FROM private_db.%2")
                           .arg(quotedIdentifier(primaryKey), quotedIdentifier(table)))) return rows.lastError().text();
        while (rows.next())
        {
            const qint64 recordId = rows.value(0).toLongLong();
            const qint64 oldId = rows.value(1).toLongLong();
            const QString serial = rows.value(2).toString();
            if (serial.isEmpty()) continue;
            QSqlQuery find(publicDatabase);
            find.prepare(QStringLiteral("SELECT work_id FROM work WHERE serial_number=? LIMIT 1"));
            find.addBindValue(serial);
            if (!find.exec()) return find.lastError().text();
            qint64 newId = 0;
            if (find.next()) newId = find.value(0).toLongLong();
            else
            {
                QSqlQuery insert(publicDatabase);
                insert.prepare(QStringLiteral("INSERT INTO work(serial_number) VALUES(?)"));
                insert.addBindValue(serial);
                if (!insert.exec()) return insert.lastError().text();
                newId = insert.lastInsertId().toLongLong();
                ++createdWorks;
            }
            if (newId != oldId)
            {
                QSqlQuery update(publicDatabase);
                update.prepare(QStringLiteral("UPDATE private_db.%1 SET work_id=? WHERE %2=?")
                                   .arg(quotedIdentifier(table), quotedIdentifier(primaryKey)));
                update.addBindValue(newId); update.addBindValue(recordId);
                if (!update.exec()) return update.lastError().text();
            }
        }
        return {};
    };
    if (const QString error = rebuildWorkTable(QStringLiteral("favorite_work"), QStringLiteral("favorite_work_id")); !error.isEmpty()) return fail(error);
    if (const QString error = rebuildWorkTable(QStringLiteral("masturbation"), QStringLiteral("masturbation_id")); !error.isEmpty()) return fail(error);
    if (!publicDatabase.commit())
    {
        const QString error = publicDatabase.lastError().text();
        detach();
        return failure(error);
    }
    detach();
    return {.succeeded = true,
            .message = QStringLiteral("私库关联已重建。"),
            .createdWorks = createdWorks,
            .createdActresses = createdActresses};
}

} // namespace darkeye
