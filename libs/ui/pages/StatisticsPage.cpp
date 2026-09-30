#include "ui/pages/StatisticsPage.h"

#include "darkeye_ui/components/TokenControls.h"
#include "ui/pages/PersonalDataPage.h"
#include "ui/pages/PlotTabPage.h"

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
}

void StatisticsPage::lazyLoad()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *tabs = new TokenTabWidget(this);
    m_personalData = new PersonalDataPage(m_publicDatabase, m_privateDatabase,
                                          m_themeService, m_actressImageDirectory, tabs);
    m_plots = new PlotTabPage(m_publicDatabase, m_privateDatabase, tabs);
    tabs->addTab(m_personalData, QStringLiteral("信息面版"));
    tabs->addTab(m_plots, QStringLiteral("统计"));
    layout->addWidget(tabs);
}

void StatisticsPage::refresh()
{
    initialize();
    m_personalData->refresh();
    m_plots->refresh();
}

} // namespace darkeye
