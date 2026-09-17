#include "database/SchemaManager.h"
#include "database/repositories/PrivateRepository.h"

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class PrivateRepositoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void managesFavoritesIdempotently();
    void writesAndAggregatesPersonalRecords();
};

void PrivateRepositoryTest::managesFavoritesIdempotently()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(QDir(temporaryDirectory.path()).filePath("private.db"), false,
                             &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 connection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));

    darkeye::PrivateRepository repository(connection.database());
    QVERIFY(repository.addFavoriteWork(7, QStringLiteral("ABC-7"), &errorMessage));
    QVERIFY(repository.addFavoriteWork(8, QStringLiteral("ABC-8"), &errorMessage));
    QVERIFY(repository.addFavoriteWork(7, QStringLiteral("ABC-7"), &errorMessage));
    QVERIFY(repository.isFavoriteWork(7, &errorMessage));
    QVERIFY(repository.addFavoriteActress(3, QStringLiteral("名前"), &errorMessage));
    QVERIFY(repository.isFavoriteActress(3, &errorMessage));

    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM favorite_work")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);
    QVERIFY(repository.addMasturbationRecord(7, QStringLiteral("ABC-7"),
                                             QStringLiteral("2026-09-01"),
                                             QString(), 3, QString(), &errorMessage));
    QCOMPARE(repository.favoriteWorkIds(&errorMessage), QList<qint64>({7, 8}));
    QCOMPARE(repository.masturbationWorkIds(&errorMessage), QList<qint64>({7}));
    QCOMPARE(repository.favoriteUnwatchedWorkIds(&errorMessage), QList<qint64>({8}));

    QVERIFY(repository.removeFavoriteWork(7, &errorMessage));
    QVERIFY(!repository.isFavoriteWork(7, &errorMessage));
}

void PrivateRepositoryTest::writesAndAggregatesPersonalRecords()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(QDir(temporaryDirectory.path()).filePath("private.db"), false,
                             &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 connection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));

    darkeye::PrivateRepository repository(connection.database());
    QVERIFY(repository.addMasturbationRecord(1, QStringLiteral("ABC-1"),
                                             QStringLiteral("2026-09-01 20:30"),
                                             QStringLiteral("tool"), 4,
                                             QStringLiteral("note"), &errorMessage));
    QVERIFY(repository.addMasturbationRecord(0, QString(),
                                             QStringLiteral("2026-09-01 22:00"),
                                             QString(), 3, QString(), &errorMessage));
    QVERIFY(!repository.addMasturbationRecord(0, QString(),
                                              QStringLiteral("2026-09-02"), QString(), 6,
                                              QString(), &errorMessage));

    const QMap<QDate, int> counts = repository.dailyCounts(
        2026, darkeye::PersonalRecordKind::Masturbation, &errorMessage);
    QCOMPARE(counts.value(QDate(2026, 9, 1)), 2);

    QVERIFY(repository.addLoveMakingRecord(QStringLiteral("2026-09-02 21:15"), 5,
                                           QStringLiteral("love note"), &errorMessage));
    QVERIFY(!repository.addLoveMakingRecord(QStringLiteral("2026-09-02 21:15"), 0,
                                            QString(), &errorMessage));
    QVERIFY(repository.addSexualArousalRecord(QStringLiteral("2026-09-03 06:00"),
                                              QStringLiteral("dream"), &errorMessage));
    QCOMPARE(repository.dailyCounts(2026, darkeye::PersonalRecordKind::LoveMaking,
                                    &errorMessage)
                 .value(QDate(2026, 9, 2)),
             1);
    QCOMPARE(repository.dailyCounts(2026, darkeye::PersonalRecordKind::SexualArousal,
                                    &errorMessage)
                 .value(QDate(2026, 9, 3)),
             1);
    QCOMPARE(repository.masturbationToolSuggestions(&errorMessage),
             QStringList({QStringLiteral("tool"), QString()}));
}

QTEST_MAIN(PrivateRepositoryTest)
#include "PrivateRepositoryTest.moc"
