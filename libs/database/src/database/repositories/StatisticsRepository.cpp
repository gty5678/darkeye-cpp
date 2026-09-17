#include "database/repositories/StatisticsRepository.h"

#include <QDate>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

namespace
{

std::optional<int> scalarCount(QSqlDatabase database, const QString &sql,
                               QString *errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec(sql) || !query.next())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }
    return query.value(0).toInt();
}

QString recordTable(darkeye::PersonalRecordKind kind)
{
    switch (kind)
    {
    case darkeye::PersonalRecordKind::Masturbation:
        return QStringLiteral("masturbation");
    case darkeye::PersonalRecordKind::LoveMaking:
        return QStringLiteral("love_making");
    case darkeye::PersonalRecordKind::SexualArousal:
        return QStringLiteral("sexual_arousal");
    }
    return {};
}

QString recordTimeColumn(darkeye::PersonalRecordKind kind)
{
    switch (kind)
    {
    case darkeye::PersonalRecordKind::Masturbation:
        return QStringLiteral("start_time");
    case darkeye::PersonalRecordKind::LoveMaking:
        return QStringLiteral("event_time");
    case darkeye::PersonalRecordKind::SexualArousal:
        return QStringLiteral("arousal_time");
    }
    return {};
}

} // namespace

namespace darkeye
{

StatisticsRepository::StatisticsRepository(QSqlDatabase publicDatabase,
                                           QSqlDatabase privateDatabase)
    : m_publicDatabase(std::move(publicDatabase)), m_privateDatabase(std::move(privateDatabase))
{
}

std::optional<DashboardStatistics> StatisticsRepository::dashboard(QString *errorMessage) const
{
    DashboardStatistics statistics;
    const auto queryPublic = [this, errorMessage](const QString &sql)
    {
        return scalarCount(m_publicDatabase, sql, errorMessage);
    };
    const auto queryPrivate = [this, errorMessage](const QString &sql)
    {
        return scalarCount(m_privateDatabase, sql, errorMessage);
    };

    const auto works = queryPublic(QStringLiteral("SELECT COUNT(*) FROM work"));
    const auto actresses = queryPublic(QStringLiteral("SELECT COUNT(*) FROM actress"));
    const auto actors = queryPublic(QStringLiteral("SELECT COUNT(*) FROM actor"));
    const auto tags =
        queryPublic(QStringLiteral("SELECT COUNT(*) FROM tag WHERE redirect_tag_id IS NULL"));
    const auto recent = queryPublic(QStringLiteral(
        "SELECT COUNT(*) FROM work WHERE create_time >= date('now', '-30 day')"));
    const auto favoriteWorks = queryPrivate(QStringLiteral("SELECT COUNT(*) FROM favorite_work"));
    const auto favoriteActresses =
        queryPrivate(QStringLiteral("SELECT COUNT(*) FROM favorite_actress"));
    if (!works || !actresses || !actors || !tags || !recent || !favoriteWorks ||
        !favoriteActresses)
    {
        return std::nullopt;
    }

    statistics.workCount = *works;
    statistics.actressCount = *actresses;
    statistics.actorCount = *actors;
    statistics.tagCount = *tags;
    statistics.recentThirtyDays = *recent;
    statistics.favoriteWorkCount = *favoriteWorks;
    statistics.favoriteActressCount = *favoriteActresses;
    return statistics;
}

std::optional<TopActressStatistic> StatisticsRepository::topActress(
    int days, QString *errorMessage) const
{
    QSqlQuery recordsQuery(m_privateDatabase);
    recordsQuery.prepare(QStringLiteral(
        "SELECT work_id, MAX(start_time), COUNT(work_id) FROM masturbation "
        "WHERE work_id IS NOT NULL AND start_time >= DATE('now', printf('-%d day', ?)) "
        "GROUP BY work_id"));
    recordsQuery.addBindValue(qMax(0, days));
    if (!recordsQuery.exec())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = recordsQuery.lastError().text();
        }
        return std::nullopt;
    }
    QHash<qint64, QPair<int, QString>> recordsByWork;
    while (recordsQuery.next())
    {
        recordsByWork.insert(recordsQuery.value(0).toLongLong(),
                             {recordsQuery.value(2).toInt(), recordsQuery.value(1).toString()});
    }
    if (recordsByWork.isEmpty())
    {
        return std::nullopt;
    }

    QSqlQuery query(m_publicDatabase);
    QStringList placeholders;
    placeholders.fill(QStringLiteral("?"), recordsByWork.size());
    query.prepare(QStringLiteral(
                      "SELECT a.actress_id, "
                      "(SELECT cn FROM actress_name WHERE actress_id=a.actress_id AND name_type=1), "
                      "a.image_urlA, relation.work_id FROM actress a "
                      "JOIN work_actress_relation relation ON relation.actress_id=a.actress_id "
                      "WHERE relation.work_id IN (%1)")
                      .arg(placeholders.join(QLatin1Char(','))));
    for (auto iterator = recordsByWork.cbegin(); iterator != recordsByWork.cend(); ++iterator)
    {
        query.addBindValue(iterator.key());
    }
    if (!query.exec())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }

    QHash<qint64, TopActressStatistic> byActress;
    while (query.next())
    {
        const qint64 actressId = query.value(0).toLongLong();
        const auto record = recordsByWork.value(query.value(3).toLongLong());
        TopActressStatistic &statistic = byActress[actressId];
        statistic.actressId = actressId;
        statistic.name = query.value(1).toString();
        statistic.imagePath = query.value(2).toString();
        statistic.recordCount += record.first;
        if (record.second > statistic.latestRecordTime)
        {
            statistic.latestRecordTime = record.second;
        }
    }

    std::optional<TopActressStatistic> top;
    for (const TopActressStatistic &statistic : byActress)
    {
        if (!top.has_value() || statistic.recordCount > top->recordCount ||
            (statistic.recordCount == top->recordCount &&
             statistic.latestRecordTime > top->latestRecordTime))
        {
            top = statistic;
        }
    }
    return top;
}

int StatisticsRepository::recordCountInDays(int days, PersonalRecordKind kind,
                                             QString *errorMessage) const
{
    QSqlQuery query(m_privateDatabase);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2 >= "
                                 "DATE('now', printf('-%d day', ?))")
                      .arg(recordTable(kind), recordTimeColumn(kind)));
    query.addBindValue(qMax(0, days));
    if (!query.exec() || !query.next())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = query.lastError().text();
        }
        return 0;
    }
    return query.value(0).toInt();
}

int StatisticsRepository::earliestRecordYear(QString *errorMessage) const
{
    QSqlQuery query(m_privateDatabase);
    if (!query.exec(QStringLiteral(
            "SELECT MIN(record_time) FROM ("
            "SELECT start_time AS record_time FROM masturbation UNION ALL "
            "SELECT event_time FROM love_making UNION ALL "
            "SELECT arousal_time FROM sexual_arousal)")) ||
        !query.next())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = query.lastError().text();
        }
        return QDate::currentDate().year();
    }
    const int year = query.value(0).toString().left(4).toInt();
    return year > 0 ? year : QDate::currentDate().year();
}

int StatisticsRepository::favoriteUnwatchedSalesCycle(QString *errorMessage) const
{
    const auto unwatched = scalarCount(
        m_privateDatabase,
        QStringLiteral("SELECT COUNT(*) FROM favorite_work favorite WHERE NOT EXISTS ("
                       "SELECT 1 FROM masturbation watched WHERE watched.work_id=favorite.work_id)"),
        errorMessage);
    if (!unwatched.has_value())
    {
        return 0;
    }
    const int recentRecords =
        recordCountInDays(90, PersonalRecordKind::Masturbation, errorMessage);
    return recentRecords == 0 ? 114514 : *unwatched * 90 / recentRecords;
}

} // namespace darkeye
