#include "ui/pages/StatisticsPage.h"

#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/TokenControls.h"
#include "ui/pages/PersonalDataPage.h"

#include <QVBoxLayout>

namespace darkeye
{

StatisticsPage::StatisticsPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                               ThemeService &themeService, QString actressImageDirectory,
                               QWidget *parent)
    : LazyWidget(parent), m_publicDatabase(std::move(publicDatabase)),
      m_privateDatabase(std::move(privateDatabase)), m_themeService(themeService),
      m_actressImageDirectory(std::move(actressImageDirectory))
{
    setObjectName(QStringLiteral("StatisticsPage"));
}

void StatisticsPage::lazyLoad()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *tabs = new TokenTabWidget(this);
    tabs->setObjectName(QStringLiteral("StatisticsTabs"));
    m_personalData = new PersonalDataPage(std::move(m_publicDatabase), std::move(m_privateDatabase),
                                          m_themeService, std::move(m_actressImageDirectory), tabs);
    auto *plots = new QWidget(tabs);
    plots->setObjectName(QStringLiteral("PlotTabPage"));
    auto *plotsLayout = new QVBoxLayout(plots);
    auto *notice = new DesignLabel(QStringLiteral("统计图表正在逐图迁移"), plots);
    notice->setAlignment(Qt::AlignCenter);
    plotsLayout->addWidget(notice);
    tabs->addTab(m_personalData, QStringLiteral("信息面版"));
    tabs->addTab(plots, QStringLiteral("统计"));
    layout->addWidget(tabs);
}

void StatisticsPage::refresh()
{
    initialize();
    m_personalData->refresh();
}

} // namespace darkeye
