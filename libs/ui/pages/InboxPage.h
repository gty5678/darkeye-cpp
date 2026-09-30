#pragma once

#include "darkeye_ui/base/LazyWidget.h"

#include <QHash>
#include <QMap>

class QLabel;
class QListWidget;

namespace darkeye
{

class CrawlerScheduler;
class CrawlerPersistenceService;

class InboxPage final : public LazyWidget
{
    Q_OBJECT

public:
    explicit InboxPage(CrawlerScheduler &crawlerScheduler,
                       CrawlerPersistenceService &crawlerPersistence,
                       QWidget *parent = nullptr);

private:
    void lazyLoad() override;
    void refresh();

    CrawlerScheduler &m_crawlerScheduler;
    CrawlerPersistenceService &m_crawlerPersistence;
    QLabel *m_schedulerLabel = nullptr;
    QListWidget *m_pendingList = nullptr;
    QListWidget *m_activeList = nullptr;
    QListWidget *m_completedList = nullptr;
    QHash<QString, QMap<QString, bool>> m_completeness;
};

} // namespace darkeye
