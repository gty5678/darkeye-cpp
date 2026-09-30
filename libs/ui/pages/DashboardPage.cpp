#include "ui/pages/DashboardPage.h"

#include "darkeye_ui/components/DesignLabel.h"
#include "ui/components/StatsOverviewCards.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QVBoxLayout>

#include <utility>

namespace darkeye
{

DashboardPage::DashboardPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                             QWidget *parent)
    : LazyWidget(parent), m_publicDatabase(std::move(publicDatabase)),
      m_privateDatabase(std::move(privateDatabase))
{
}

void DashboardPage::lazyLoad()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    m_overview = new StatsOverviewCards(std::move(m_publicDatabase), std::move(m_privateDatabase), this);
    root->addWidget(m_overview);

    auto *recentContainer = new QWidget(this);
    auto *recentLayout = new QVBoxLayout(recentContainer);
    recentLayout->setContentsMargins(0, 0, 0, 0);
    recentLayout->setSpacing(8);
    auto *recentHeading = new DesignLabel(QStringLiteral("最近行为"), recentContainer);
    recentHeading->setObjectName(QStringLiteral("dashboard_section_title"));
    recentLayout->addWidget(recentHeading);
    auto *recentColumns = new QHBoxLayout;
    recentColumns->setSpacing(16);
    const auto addRecentColumn = [recentContainer, recentColumns](const QString &title,
                                                                  const QStringList &items)
    {
        auto *column = new QWidget(recentContainer);
        auto *layout = new QVBoxLayout(column);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        layout->addWidget(new DesignLabel(title, column));
        auto *list = new QListWidget(column);
        list->setMaximumHeight(200);
        list->addItems(items);
        layout->addWidget(list);
        recentColumns->addWidget(column, 1);
    };
    addRecentColumn(QStringLiteral("最近观看 / 最近标记"),
                    {QStringLiteral("（占位）最近看过的一部作品"),
                     QStringLiteral("（占位）最近看过的另一部作品")});
    addRecentColumn(QStringLiteral("最近新增"),
                    {QStringLiteral("（占位）最近新增作品"),
                     QStringLiteral("（占位）最近新增女优")});
    recentLayout->addLayout(recentColumns);
    root->addWidget(recentContainer);

    auto *pendingContainer = new QWidget(this);
    auto *pendingLayout = new QVBoxLayout(pendingContainer);
    pendingLayout->setContentsMargins(0, 0, 0, 0);
    pendingLayout->setSpacing(8);
    auto *pendingHeading = new DesignLabel(QStringLiteral("待处理事项"), pendingContainer);
    pendingHeading->setObjectName(QStringLiteral("dashboard_section_title"));
    pendingLayout->addWidget(pendingHeading);
    auto *pending = new QListWidget(pendingContainer);
    pending->setMaximumHeight(140);
    pending->addItems({QStringLiteral("（占位）12 部作品没有封面"),
                       QStringLiteral("（占位）8 部作品未绑定女优")});
    pendingLayout->addWidget(pending);
    root->addWidget(pendingContainer);
    root->addStretch();
}

void DashboardPage::refresh()
{
    initialize();
    m_overview->refresh();
}

} // namespace darkeye
