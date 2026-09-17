#pragma once

#include "darkeye_ui/base/LazyWidget.h"

#include <QSqlDatabase>

namespace darkeye
{

class PersonalDataPage;
class ThemeService;

class StatisticsPage final : public LazyWidget
{
    Q_OBJECT

public:
    StatisticsPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                   ThemeService &themeService, QString actressImageDirectory,
                   QWidget *parent = nullptr);

    void refresh();

private:
    void lazyLoad() override;

    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
    ThemeService &m_themeService;
    QString m_actressImageDirectory;
    PersonalDataPage *m_personalData = nullptr;
};

} // namespace darkeye
