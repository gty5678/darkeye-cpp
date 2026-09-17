#pragma once

#include "darkeye_ui/base/LazyWidget.h"

#include <QSqlDatabase>

namespace darkeye
{

class StatsOverviewCards;

class DashboardPage final : public LazyWidget
{
    Q_OBJECT

public:
    DashboardPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                  QWidget *parent = nullptr);

    void refresh();

private:
    void lazyLoad() override;

    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
    StatsOverviewCards *m_overview = nullptr;
};

} // namespace darkeye
