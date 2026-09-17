#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QWidget>

class QComboBox;
class QLabel;

namespace darkeye
{

class CalendarHeatmap;
class StatsOverviewCards;
class ThemeService;
class TopActressCard;

class PersonalDataPage final : public QWidget
{
    Q_OBJECT

public:
    PersonalDataPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                     ThemeService &themeService, QString actressImageDirectory,
                     QWidget *parent = nullptr);

    void refresh();

private:
    void refreshHeatmap();

    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
    ThemeService &m_themeService;
    QString m_actressImageDirectory;
    StatsOverviewCards *m_overview = nullptr;
    QList<TopActressCard *> m_topActressCards;
    QLabel *m_salesCycle = nullptr;
    QComboBox *m_yearSelector = nullptr;
    QComboBox *m_recordKindSelector = nullptr;
    QLabel *m_heatmapTitle = nullptr;
    CalendarHeatmap *m_heatmap = nullptr;
};

} // namespace darkeye
