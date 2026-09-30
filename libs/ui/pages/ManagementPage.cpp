#include "ui/pages/ManagementPage.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/TokenControls.h"
#include "ui/pages/management/AddWorkTabPage3.h"
#include "ui/pages/management/MakerManagementWidget.h"
#include "ui/pages/management/LabelManagementWidget.h"
#include "ui/pages/management/ReferenceManagementWidget.h"
#include "ui/pages/management/SummaryQueryWidget.h"
#include "ui/pages/management/TagManagementWidget.h"
#include "ui/pages/management/WorkBatchStateWidget.h"
#include "ui/pages/management/WorkMaintenanceWidget.h"

#include <QTabWidget>
#include <QVBoxLayout>

#include <functional>

namespace darkeye
{

namespace
{

class DeferredTab final : public LazyWidget
{
public:
    using Factory = std::function<QWidget *(QWidget *parent)>;

    explicit DeferredTab(Factory factory, QWidget *parent = nullptr)
        : LazyWidget(parent), m_factory(std::move(factory))
    {
    }

private:
    void lazyLoad() override
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_factory(this));
    }

    Factory m_factory;
};

} // namespace

ManagementPage::ManagementPage(QSqlDatabase database, ThemeService &themes,
                               CrawlerScheduler &crawlerScheduler, QString coverDirectory,
                               QString actressImageDirectory, QString fanartDirectory, QUrl imageFetchEndpoint,
                               QUrl topActressesEndpoint, QWidget *parent)
    : LazyWidget(parent), m_database(std::move(database)), m_themes(themes),
      m_coverDirectory(std::move(coverDirectory)),
      m_actressImageDirectory(std::move(actressImageDirectory)),
      m_fanartDirectory(std::move(fanartDirectory)),
      m_imageFetchEndpoint(std::move(imageFetchEndpoint)),
      m_topActressesEndpoint(std::move(topActressesEndpoint)),
      m_crawlerScheduler(crawlerScheduler)
{
}

void ManagementPage::lazyLoad()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_tabs = new TokenTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("ManagementTabs"));

    const auto addDeferredTab = [this](const QString &title, DeferredTab::Factory factory)
    {
        m_tabs->addTab(new DeferredTab(std::move(factory), m_tabs), title);
    };
    addDeferredTab(QStringLiteral("添加/修改作品"), [this](QWidget *parent)
    {
        m_workEditor = new AddWorkTabPage3(m_database, m_themes, m_crawlerScheduler,
                                           m_coverDirectory, m_fanartDirectory,
                                           m_imageFetchEndpoint, parent);
        connect(m_workEditor, &AddWorkTabPage3::workSaved, this, [this](qint64 workId, bool)
        {
            if (m_softDelete != nullptr)
                m_softDelete->refresh();
            emit workChanged(workId);
        });
        connect(m_workEditor, &AddWorkTabPage3::actressesCreated, this,
                &ManagementPage::actressesCreated);
        connect(m_workEditor, &AddWorkTabPage3::workLinkRequested, this,
                &ManagementPage::workRequested);
        connect(m_workEditor, &AddWorkTabPage3::actressLinkRequested, this,
                &ManagementPage::actressRequested);
        connect(this, &ManagementPage::referencesChanged, m_workEditor,
                &AddWorkTabPage3::refreshReferences);
        connect(this, &ManagementPage::tagsChanged, m_workEditor,
                &AddWorkTabPage3::refreshAssociations);
        connect(this, &ManagementPage::actressesChanged, m_workEditor,
                &AddWorkTabPage3::refreshAssociations);
        return m_workEditor;
    });
    addDeferredTab(QStringLiteral("作品标签管理"), [this](QWidget *parent)
    {
        auto *tagPage = new QWidget(parent);
        auto *layout = new QVBoxLayout(tagPage);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *tagManagement = new TagManagementWidget(m_database, m_themes, tagPage);
        layout->addWidget(tagManagement);
        connect(tagManagement, &TagManagementWidget::tagsChanged, this, &ManagementPage::tagsChanged);
        connect(tagManagement, &TagManagementWidget::tagTypesChanged, this,
                &ManagementPage::tagsChanged);
        return tagPage;
    });
    addDeferredTab(QStringLiteral("番号/片商管理"), [this](QWidget *parent)
    {
        auto *page = new MakerManagementWidget(m_database, m_themes, parent);
        connect(page, &MakerManagementWidget::referencesChanged, this,
                &ManagementPage::referencesChanged);
        connect(page, &MakerManagementWidget::prefixesChanged, this,
                [this] { emit referencesChanged(ReferenceKind::Maker); });
        return page;
    });
    const auto addReferenceTab = [this, &addDeferredTab](ReferenceKind kind, const QString &title)
    {
        addDeferredTab(title, [this, kind](QWidget *parent)
        {
            auto *page = new LabelManagementWidget(kind, m_database, m_themes, parent);
            connect(page, &LabelManagementWidget::referencesChanged, this,
                    &ManagementPage::referencesChanged);
            return page;
        });
    };
    addReferenceTab(ReferenceKind::Label, QStringLiteral("厂牌管理"));
    addReferenceTab(ReferenceKind::Series, QStringLiteral("系列管理"));
    addDeferredTab(QStringLiteral("批量操作"), [this](QWidget *parent)
    {
        auto *page = new WorkMaintenanceWidget(m_database, m_themes, m_coverDirectory,
                                               m_actressImageDirectory, m_imageFetchEndpoint,
                                               m_topActressesEndpoint, parent);
        connect(page, &WorkMaintenanceWidget::worksChanged, this, &ManagementPage::worksChanged);
        connect(page, &WorkMaintenanceWidget::actressesChanged, this,
                &ManagementPage::actressesChanged);
        return page;
    });
    addDeferredTab(QStringLiteral("汇总查询表"), [this](QWidget *parent)
    {
        auto *page = new SummaryQueryWidget(m_database, m_themes, parent);
        connect(page, &SummaryQueryWidget::workRequested, this,
                [this](qint64 workId) { loadWork(workId); });
        return page;
    });
    addDeferredTab(QStringLiteral("作品软删除"), [this](QWidget *parent)
    {
        m_softDelete = new WorkBatchStateWidget(WorkStateMode::Active, m_database, m_themes,
                                                {}, {}, parent);
        connect(m_softDelete, &WorkBatchStateWidget::worksChanged, this, [this]
        {
            if (m_recycleBin != nullptr)
                m_recycleBin->refresh();
            emit worksChanged();
        });
        return m_softDelete;
    });
    addDeferredTab(QStringLiteral("回收站"), [this](QWidget *parent)
    {
        m_recycleBin = new WorkBatchStateWidget(WorkStateMode::RecycleBin, m_database, m_themes,
                                                m_coverDirectory, m_fanartDirectory, parent);
        connect(m_recycleBin, &WorkBatchStateWidget::worksChanged, this, [this]
        {
            if (m_softDelete != nullptr)
                m_softDelete->refresh();
            emit worksChanged();
        });
        return m_recycleBin;
    });
    root->addWidget(m_tabs);
}

void ManagementPage::ensureTabLoaded(int index)
{
    if (index < 0)
        return;

    if (auto *tab = dynamic_cast<DeferredTab *>(m_tabs->widget(index)); tab != nullptr)
        tab->initialize();
}

bool ManagementPage::loadWork(qint64 workId)
{
    initialize();
    ensureTabLoaded(WorkEditorTab);
    if (m_workEditor == nullptr || !m_workEditor->loadWork(workId))
        return false;
    m_tabs->setCurrentIndex(WorkEditorTab);
    return true;
}

void ManagementPage::beginCreateWork()
{
    initialize();
    ensureTabLoaded(WorkEditorTab);
    m_workEditor->beginCreate();
    m_tabs->setCurrentIndex(WorkEditorTab);
}

void ManagementPage::beginCreateWorkAndCrawl(const QString &serialNumber)
{
    initialize();
    ensureTabLoaded(WorkEditorTab);
    m_workEditor->beginCreateAndCrawl(serialNumber);
    m_tabs->setCurrentIndex(WorkEditorTab);
}

void ManagementPage::refreshPersonSelectors()
{
    if (m_workEditor != nullptr)
        m_workEditor->refreshAssociations();
}

} // namespace darkeye
