#include "settings/Paths.h"
#include "database/DatabaseManager.h"
#include "database/SchemaManager.h"
#include "database/SqlScriptRunner.h"

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class SchemaManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void scriptSplitterKeepsTriggerBodyTogether();
    void initializesAndValidatesPublicDatabase();
    void acceptsPublicUserVersionWithoutLegacyTable();
    void initializesAndValidatesPrivateDatabase();
    void migratesPublicV1DataToV2();
    void migratesPrivateV1DataToV11();
    void rejectsUnknownNonEmptySchema();
    void managerBacksUpBeforeMigratingLegacyPublicDatabase();
    void rejectsForeignKeyCorruption();
};

void SchemaManagerTest::scriptSplitterKeepsTriggerBodyTogether()
{
    const QString script = QStringLiteral(
        "CREATE TABLE item(id INTEGER, updated TEXT);"
        "CREATE TRIGGER touch_item AFTER UPDATE ON item BEGIN "
        "UPDATE item SET updated='a;b' WHERE id=OLD.id; END;"
        "PRAGMA user_version=2;");
    const QStringList statements = darkeye::SqlScriptRunner::splitStatements(script);
    QCOMPARE(statements.size(), 3);
    QVERIFY(statements.at(1).contains(QStringLiteral("UPDATE item")));
    QVERIFY(statements.at(1).endsWith(QStringLiteral("END")));
}

void SchemaManagerTest::initializesAndValidatesPublicDatabase()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("public.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(path, false, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(connection.schemaVersion(), QStringLiteral("2"));
    QVERIFY(connection.database().tables(QSql::Tables).contains(QStringLiteral("work")));
    QVERIFY(connection.database().tables(QSql::Views).contains(QStringLiteral("v_work_all_info")));

    QSqlQuery contentCountQuery(connection.database());
    QVERIFY(contentCountQuery.exec(QStringLiteral("SELECT COUNT(*) FROM tag")));
    QVERIFY(contentCountQuery.next());
    QVERIFY(contentCountQuery.value(0).toInt() > 0);
    QVERIFY(contentCountQuery.exec(QStringLiteral("SELECT COUNT(*) FROM maker")));
    QVERIFY(contentCountQuery.next());
    QVERIFY(contentCountQuery.value(0).toInt() > 0);
}

void SchemaManagerTest::acceptsPublicUserVersionWithoutLegacyTable()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("public.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(path, false, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("DROP TABLE db_version")));
    QCOMPARE(connection.schemaVersion(), QStringLiteral("2"));
    QVERIFY2(darkeye::SchemaManager::validateCurrentSchema(
                 connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
}

void SchemaManagerTest::initializesAndValidatesPrivateDatabase()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("private.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(path, false, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::migrateToCurrent(
                 connection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(connection.schemaVersion(), QStringLiteral("1.1"));
    QVERIFY(connection.database().tables(QSql::Tables).contains(QStringLiteral("masturbation")));
}

void SchemaManagerTest::migratesPublicV1DataToV2()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("public-v1.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(path, false, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(darkeye::SqlScriptRunner::executeResource(
                 connection.database(), QStringLiteral(":/sql/fixtures/initPublicTable-v1.0.sql"),
                 &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(connection.schemaVersion(), QStringLiteral("1.0"));

    QSqlQuery insertQuery(connection.database());
    QVERIFY(insertQuery.exec(QStringLiteral(
        "INSERT INTO work(serial_number, story) VALUES('LEGACY-1', 'legacy notes')")));

    QVERIFY2(darkeye::SchemaManager::migrateToCurrent(
                 connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(connection.schemaVersion(), QStringLiteral("2"));

    QSqlQuery migratedQuery(connection.database());
    QVERIFY(migratedQuery.exec(QStringLiteral(
        "SELECT notes, runtime, maker_id, label_id, series_id FROM work "
        "WHERE serial_number='LEGACY-1'")));
    QVERIFY(migratedQuery.next());
    QCOMPARE(migratedQuery.value(0).toString(), QStringLiteral("legacy notes"));
    QVERIFY(migratedQuery.value(1).isNull());
    QVERIFY(migratedQuery.value(2).isNull());
    QVERIFY(migratedQuery.value(3).isNull());
    QVERIFY(migratedQuery.value(4).isNull());

    QVERIFY(migratedQuery.exec(QStringLiteral(
        "SELECT COUNT(*) FROM label WHERE cn_name='S1 NO.1 STYLE'")));
    QVERIFY(migratedQuery.next());
    QVERIFY(migratedQuery.value(0).toInt() > 0);
    QVERIFY(migratedQuery.exec(QStringLiteral(
        "SELECT COUNT(*) FROM prefix_maker_relation p "
        "JOIN maker m ON m.maker_id=p.maker_id "
        "WHERE p.prefix='SONE' AND m.cn_name='S1 NO.1 STYLE'")));
    QVERIFY(migratedQuery.next());
    QCOMPARE(migratedQuery.value(0).toInt(), 1);
    QVERIFY(migratedQuery.exec(QStringLiteral(
        "SELECT COUNT(*) FROM series WHERE cn_name='出張先相部屋NTR'")));
    QVERIFY(migratedQuery.next());
    QVERIFY(migratedQuery.value(0).toInt() > 0);
}

void SchemaManagerTest::migratesPrivateV1DataToV11()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("private-v1.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(path, false, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(darkeye::SqlScriptRunner::executeResource(
                 connection.database(), QStringLiteral(":/sql/fixtures/initPrivateTable-v1.0.sql"),
                 &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(connection.schemaVersion(), QStringLiteral("1.0"));

    QSqlQuery insertQuery(connection.database());
    QVERIFY(insertQuery.exec(QStringLiteral(
        "INSERT INTO favorite_actress(actress_id, jp_name) VALUES(8, 'legacy')")));

    QVERIFY2(darkeye::SchemaManager::migrateToCurrent(
                 connection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(connection.schemaVersion(), QStringLiteral("1.1"));

    QVERIFY(insertQuery.exec(QStringLiteral(
        "INSERT INTO favorite_actress(actress_id, jp_name) VALUES(8, 'duplicate allowed')")));
    QVERIFY(insertQuery.exec(QStringLiteral(
        "SELECT COUNT(*) FROM favorite_actress WHERE actress_id=8")));
    QVERIFY(insertQuery.next());
    QCOMPARE(insertQuery.value(0).toInt(), 2);
}

void SchemaManagerTest::rejectsUnknownNonEmptySchema()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("unknown.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(path, false, &errorMessage), qPrintable(errorMessage));
    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("CREATE TABLE unexpected(id INTEGER)")));

    QVERIFY(!darkeye::SchemaManager::migrateToCurrent(
        connection, darkeye::DatabaseKind::Public, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("不支持")));
}

void SchemaManagerTest::managerBacksUpBeforeMigratingLegacyPublicDatabase()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString applicationDirectory = temporaryDirectory.path();
    const darkeye::settings::Paths paths(applicationDirectory);
    QVERIFY(paths.ensureRuntimeDirectories());

    {
        darkeye::SqliteConnection legacyConnection;
        QString errorMessage;
        QVERIFY2(legacyConnection.open(paths.publicDatabase(), false, &errorMessage),
                 qPrintable(errorMessage));
        QVERIFY2(darkeye::SqlScriptRunner::executeResource(
                     legacyConnection.database(),
                     QStringLiteral(":/sql/fixtures/initPublicTable-v1.0.sql"),
                     &errorMessage),
                 qPrintable(errorMessage));
        QSqlQuery insertQuery(legacyConnection.database());
        QVERIFY(insertQuery.exec(QStringLiteral(
            "INSERT INTO work(serial_number, story) VALUES('BACKUP-1', 'preserve me')")));
    }

    darkeye::DatabaseManager manager;
    QString errorMessage;
    QVERIFY2(manager.initialize(paths, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(manager.publicConnection().schemaVersion(), QStringLiteral("2"));

    const QStringList backups = QDir(paths.publicBackupDirectory()).entryList(
        {QStringLiteral("public-pre-migration-*.db")}, QDir::Files);
    QCOMPARE(backups.size(), 1);

    darkeye::SqliteConnection backupConnection;
    QVERIFY2(backupConnection.open(
                 QDir(paths.publicBackupDirectory()).filePath(backups.constFirst()), true,
                 &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(backupConnection.schemaVersion(), QStringLiteral("1.0"));
    QSqlQuery backupQuery(backupConnection.database());
    QVERIFY(backupQuery.exec(QStringLiteral(
        "SELECT story FROM work WHERE serial_number='BACKUP-1'")));
    QVERIFY(backupQuery.next());
    QCOMPARE(backupQuery.value(0).toString(), QStringLiteral("preserve me"));
}

void SchemaManagerTest::rejectsForeignKeyCorruption()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false,
                             &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));

    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO work_actress_relation(work_id, actress_id) VALUES(999, 999)")));
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=ON")));

    QVERIFY(!darkeye::SchemaManager::validateCurrentSchema(
        connection, darkeye::DatabaseKind::Public, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("外键损坏")));
}

QTEST_MAIN(SchemaManagerTest)
#include "SchemaManagerTest.moc"
