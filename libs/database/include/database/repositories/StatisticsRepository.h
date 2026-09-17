#pragma once

#include "database/repositories/PrivateRepository.h"

#include <QSqlDatabase>
#include <QString>
#include <optional>

namespace darkeye
{

struct DashboardStatistics final
{
    int workCount = 0;
    int actressCount = 0;
    int actorCount = 0;
    int tagCount = 0;
    int recentThirtyDays = 0;
    int favoriteWorkCount = 0;
    int favoriteActressCount = 0;
};

struct TopActressStatistic final
{
    qint64 actressId = 0;
    QString name;
    QString imagePath;
    QString latestRecordTime;
    int recordCount = 0;
};

class StatisticsRepository final
{
public:
    StatisticsRepository(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase);

    [[nodiscard]] std::optional<DashboardStatistics> dashboard(
        QString *errorMessage = nullptr) const;
    [[nodiscard]] std::optional<TopActressStatistic> topActress(
        int days, QString *errorMessage = nullptr) const;
    [[nodiscard]] int recordCountInDays(int days, PersonalRecordKind kind,
                                        QString *errorMessage = nullptr) const;
    [[nodiscard]] int earliestRecordYear(QString *errorMessage = nullptr) const;
    [[nodiscard]] int favoriteUnwatchedSalesCycle(QString *errorMessage = nullptr) const;

private:
    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
};

} // namespace darkeye
