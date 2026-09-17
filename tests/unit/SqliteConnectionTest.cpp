#include "database/SqliteConnection.h"

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class SqliteConnectionTest final : public QObject
{
    Q_OBJECT

private slots:
    void enablesRequiredPragmasAndReadsUserVersion();
    void readsLegacyTextVersion();
    void refusesMissingReadOnlyDatabase();
};

void SqliteConnectionTest::enablesRequiredPragmasAndReadsUserVersion()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("test.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(databasePath, false, &errorMessage), qPrintable(errorMessage));
    QVERIFY(connection.isOpen());
    QVERIFY(!connection.isReadOnly());

    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);

    QVERIFY(query.exec(QStringLiteral("PRAGMA journal_mode")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString().toLower(), QStringLiteral("wal"));

    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=2")));
    QCOMPARE(connection.schemaVersion(&errorMessage), QStringLiteral("2"));
}

void SqliteConnectionTest::readsLegacyTextVersion()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("legacy.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(databasePath, false, &errorMessage), qPrintable(errorMessage));

    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE db_version(version TEXT NOT NULL, applied_at TEXT NOT NULL)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO db_version(version, applied_at) VALUES('1.1', '2026-03-12 09:28:01')")));
    QCOMPARE(connection.schemaVersion(&errorMessage), QStringLiteral("1.1"));
}

void SqliteConnectionTest::refusesMissingReadOnlyDatabase()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("missing.db"));

    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY(!connection.open(databasePath, true, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(!QFileInfo::exists(databasePath));
}

QTEST_MAIN(SqliteConnectionTest)
#include "SqliteConnectionTest.moc"

