#pragma once

#include "darkeye_ui/base/LazyWidget.h"

#include <QList>
#include <QSqlDatabase>

class QLabel;
class QPushButton;

namespace darkeye
{

class CalendarHeatmap;
class StatsOverviewCards;
class ThemeService;
class TopActressCard;

class PersonalDataPage final : public LazyWidget
{
    Q_OBJECT

public:
    PersonalDataPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                     ThemeService &themeService, QString actressImageDirectory,
                     QWidget *parent = nullptr);

    void refresh();

private:
    void lazyLoad() override;
    void refreshHeatmap();

    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
    ThemeService &m_themeService;
    QString m_actressImageDirectory;
    StatsOverviewCards *m_overview = nullptr;
    QList<TopActressCard *> m_topActressCards;
    QLabel *m_salesCycle = nullptr;
    QList<QPushButton *> m_yearButtons;
    int m_currentYear = 0;
    int m_recordKindIndex = 0;
    QLabel *m_heatmapTitle = nullptr;
    CalendarHeatmap *m_heatmap = nullptr;
};

} // namespace darkeye
