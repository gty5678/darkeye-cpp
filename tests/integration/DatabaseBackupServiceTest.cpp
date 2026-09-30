#include "database/DatabaseBackupService.h"
#include "database/DatabaseMaintenanceService.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class DatabaseBackupServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsReadableConsistentBackup();
    void refusesToOverwriteExistingBackup();
    void restoresBackupIntoOpenDatabase();
};

void DatabaseBackupServiceTest::createsReadableConsistentBackup()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    const QString sourcePath = root.filePath(QStringLiteral("source.db"));
    const QString backupPath = root.filePath(QStringLiteral("backups/source.db"));

    darkeye::SqliteConnection source;
    QString errorMessage;
    QVERIFY2(source.open(sourcePath, false, &errorMessage), qPrintable(errorMessage));
    QSqlQuery createQuery(source.database());
    QVERIFY(createQuery.exec(QStringLiteral("CREATE TABLE item(value TEXT NOT NULL)")));
    QVERIFY(createQuery.exec(QStringLiteral("INSERT INTO item(value) VALUES('preserved')")));

    QVERIFY2(darkeye::DatabaseBackupService::createConsistentBackup(
                 source, backupPath, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(QFileInfo(backupPath).size() > 0);

    darkeye::SqliteConnection backup;
    QVERIFY2(backup.open(backupPath, true, &errorMessage), qPrintable(errorMessage));
    QSqlQuery readQuery(backup.database());
    QVERIFY(readQuery.exec(QStringLiteral("SELECT value FROM item")));
    QVERIFY(readQuery.next());
    QCOMPARE(readQuery.value(0).toString(), QStringLiteral("preserved"));
}

void DatabaseBackupServiceTest::refusesToOverwriteExistingBackup()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    const QString sourcePath = root.filePath(QStringLiteral("source.db"));
    const QString backupPath = root.filePath(QStringLiteral("source-backup.db"));

    darkeye::SqliteConnection source;
    QString errorMessage;
    QVERIFY2(source.open(sourcePath, false, &errorMessage), qPrintable(errorMessage));
    QSqlQuery query(source.database());
    QVERIFY(query.exec(QStringLiteral("CREATE TABLE item(value TEXT)")));
    QVERIFY2(darkeye::DatabaseBackupService::createConsistentBackup(
                 source, backupPath, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(!darkeye::DatabaseBackupService::createConsistentBackup(
        source, backupPath, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("拒绝覆盖")));
}

void DatabaseBackupServiceTest::restoresBackupIntoOpenDatabase()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString databasePath = root.filePath(QStringLiteral("source.db"));
    darkeye::SqliteConnection source;
    QString errorMessage;
    QVERIFY2(source.open(databasePath, false, &errorMessage), qPrintable(errorMessage));
    QSqlQuery query(source.database());
    QVERIFY(query.exec(QStringLiteral("CREATE TABLE sample(value TEXT)")));
    QVERIFY(query.exec(QStringLiteral("INSERT INTO sample VALUES('before')")));

    const auto backup = darkeye::DatabaseMaintenanceService::createBackup(
        source.database(), root.filePath(QStringLiteral("backups")), QStringLiteral("test"));
    QVERIFY2(backup.succeeded, qPrintable(backup.message));
    QVERIFY(query.exec(QStringLiteral("UPDATE sample SET value='after'")));

    const auto restored = darkeye::DatabaseMaintenanceService::restoreBackup(
        source.database(), backup.outputPath);
    QVERIFY2(restored.succeeded, qPrintable(restored.message));
    QVERIFY(query.exec(QStringLiteral("SELECT value FROM sample")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("before"));
}

QTEST_MAIN(DatabaseBackupServiceTest)
#include "DatabaseBackupServiceTest.moc"
