#include "ui/pages/InboxPage.h"

#include "crawler/CrawlerScheduler.h"
#include "services/CrawlerPersistenceService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignLabel.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace darkeye
{

InboxPage::InboxPage(CrawlerScheduler &crawlerScheduler,
                     CrawlerPersistenceService &crawlerPersistence, QWidget *parent)
    : LazyWidget(parent), m_crawlerScheduler(crawlerScheduler),
      m_crawlerPersistence(crawlerPersistence)
{
    connect(&m_crawlerScheduler, &CrawlerScheduler::queueChanged, this, &InboxPage::refresh);
    connect(&m_crawlerScheduler, &CrawlerScheduler::pausedChanged, this, &InboxPage::refresh);
    connect(&m_crawlerScheduler, &CrawlerScheduler::taskStateChanged, this,
            [this](const CrawlTaskSnapshot &) { refresh(); });
    connect(&m_crawlerPersistence, &CrawlerPersistenceService::completenessChanged, this,
            [this](const QString &serial, const QMap<QString, bool> &flags)
            {
                m_completeness.insert(serial, flags);
            });
    connect(&m_crawlerPersistence, &CrawlerPersistenceService::finished, this,
            [this](const QString &serial, bool succeeded, const QString &errorMessage)
            {
                if (m_completedList == nullptr)
                    return;
                const QMap<QString, bool> flags = m_completeness.value(serial);
                int available = 0;
                for (auto iterator = flags.cbegin(); iterator != flags.cend(); ++iterator)
                    available += iterator.value() ? 1 : 0;
                const QString state = succeeded ? QStringLiteral("完成") : QStringLiteral("失败");
                const QString suffix = flags.isEmpty()
                    ? (errorMessage.isEmpty() ? QString() : QStringLiteral(" · %1").arg(errorMessage))
                    : QStringLiteral(" · 完整度 %1/15").arg(available);
                m_completedList->insertItem(0, QStringLiteral("%1 · %2%3").arg(serial, state, suffix));
                while (m_completedList->count() > 50)
                    delete m_completedList->takeItem(m_completedList->count() - 1);
            });
}

void InboxPage::lazyLoad()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    auto *title = new DesignLabel(QStringLiteral("通知与爬虫任务"), this);
    layout->addWidget(title);

    auto *toolbar = new QHBoxLayout;
    m_schedulerLabel = new QLabel(this);
    toolbar->addWidget(m_schedulerLabel);
    toolbar->addStretch();
    auto *resume = new DesignButton(QStringLiteral("开始队列"), this);
    auto *pause = new DesignButton(QStringLiteral("暂停队列"), this);
    auto *clear = new DesignButton(QStringLiteral("清空队列"), this);
    toolbar->addWidget(resume);
    toolbar->addWidget(pause);
    toolbar->addWidget(clear);
    connect(resume, &QPushButton::clicked, this, [this] { m_crawlerScheduler.resume(); });
    connect(pause, &QPushButton::clicked, this, [this] { m_crawlerScheduler.pause(false); });
    connect(clear, &QPushButton::clicked, this, [this] { m_crawlerScheduler.pause(true); });
    layout->addLayout(toolbar);

    auto *columns = new QHBoxLayout;
    auto *pending = new QVBoxLayout;
    pending->addWidget(new DesignLabel(QStringLiteral("待爬"), this));
    m_pendingList = new QListWidget(this);
    m_pendingList->setSelectionMode(QAbstractItemView::NoSelection);
    pending->addWidget(m_pendingList);
    columns->addLayout(pending);
    auto *active = new QVBoxLayout;
    active->addWidget(new DesignLabel(QStringLiteral("进行中"), this));
    m_activeList = new QListWidget(this);
    m_activeList->setSelectionMode(QAbstractItemView::NoSelection);
    active->addWidget(m_activeList);
    columns->addLayout(active);
    auto *completed = new QVBoxLayout;
    completed->addWidget(new DesignLabel(QStringLiteral("已完成"), this));
    m_completedList = new QListWidget(this);
    m_completedList->setSelectionMode(QAbstractItemView::NoSelection);
    completed->addWidget(m_completedList);
    columns->addLayout(completed);
    layout->addLayout(columns, 1);
    refresh();
    layout->addStretch();
}

void InboxPage::refresh()
{
    if (m_schedulerLabel == nullptr)
        return;
    m_schedulerLabel->setText(
        QStringLiteral("调度：%1 · 队列长度 %2")
            .arg(m_crawlerScheduler.isPaused() ? QStringLiteral("已暂停")
                                                : QStringLiteral("运行中"))
            .arg(m_crawlerScheduler.queuedCount()));
    m_pendingList->clear();
    m_pendingList->addItems(m_crawlerScheduler.pendingSerials());
    m_activeList->clear();
    for (const CrawlTaskSnapshot &task : m_crawlerScheduler.activeTasks())
    {
        const QString state = task.state == CrawlWorkflowState::Crawling
            ? QStringLiteral("API 请求") : QStringLiteral("入库");
        m_activeList->addItem(QStringLiteral("%1 · %2").arg(task.serialNumber, state));
    }
}

} // namespace darkeye
