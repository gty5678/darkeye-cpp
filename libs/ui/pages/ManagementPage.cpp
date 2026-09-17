#include "ui/pages/ManagementPage.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/TokenControls.h"
#include "ui/components/MakerPrefixManagementWidget.h"
#include "ui/components/ReferenceManagementWidget.h"
#include "ui/components/TagManagementWidget.h"
#include "ui/components/WorkBatchStateWidget.h"
#include "ui/components/WorkEditorWidget.h"
#include "ui/components/WorkMaintenanceWidget.h"

#include <QTabWidget>
#include <QVBoxLayout>

namespace darkeye
{

ManagementPage::ManagementPage(QSqlDatabase database, ThemeService &themes, QString coverDirectory,
                               QString fanartDirectory, QUrl imageFetchEndpoint,
                               QUrl topActressesEndpoint, QWidget *parent)
    : LazyWidget(parent), m_database(std::move(database)), m_themes(themes),
      m_coverDirectory(std::move(coverDirectory)), m_fanartDirectory(std::move(fanartDirectory)),
      m_imageFetchEndpoint(std::move(imageFetchEndpoint)),
      m_topActressesEndpoint(std::move(topActressesEndpoint))
{
    setObjectName(QStringLiteral("ManagementPage"));
}

void ManagementPage::lazyLoad()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *tabs = new TokenTabWidget(this);
    tabs->setObjectName(QStringLiteral("ManagementTabs"));
    auto *tagManagement = new TagManagementWidget(m_database, m_themes, tabs);
    auto *tagTypes = new TagTypeManagementWidget(m_database, m_themes, tabs);
    tabs->addTab(tagManagement, QStringLiteral("作品标签"));
    tabs->addTab(tagTypes, QStringLiteral("标签类型"));
    auto *makerPrefixes = new MakerPrefixManagementWidget(m_database, m_themes, tabs);
    tabs->addTab(makerPrefixes, QStringLiteral("番号前缀"));
    connect(makerPrefixes, &MakerPrefixManagementWidget::prefixesChanged, this,
            [this] { emit referencesChanged(ReferenceKind::Maker); });
    connect(tagManagement, &TagManagementWidget::tagsChanged, this, &ManagementPage::tagsChanged);
    connect(tagTypes, &TagTypeManagementWidget::typesChanged, tagManagement,
            &TagManagementWidget::refreshTagTypes);
    connect(tagTypes, &TagTypeManagementWidget::typesChanged, tagManagement,
            &TagManagementWidget::refresh);
    connect(tagTypes, &TagTypeManagementWidget::typesChanged, this, &ManagementPage::tagsChanged);
    for (const auto &[kind, title] :
         QList<QPair<ReferenceKind, QString>>{{ReferenceKind::Maker, QStringLiteral("番号/片商")},
                                              {ReferenceKind::Label, QStringLiteral("厂牌")},
                                              {ReferenceKind::Series, QStringLiteral("系列")}})
    {
        auto *widget = new ReferenceManagementWidget(kind, m_database, m_themes, tabs);
        tabs->addTab(widget, title);
        connect(widget, &ReferenceManagementWidget::referencesChanged, this,
                &ManagementPage::referencesChanged);
        if (kind == ReferenceKind::Maker)
        {
            connect(widget, &ReferenceManagementWidget::referencesChanged, makerPrefixes,
                    &MakerPrefixManagementWidget::refreshMakers);
            connect(widget, &ReferenceManagementWidget::referencesChanged, makerPrefixes,
                    &MakerPrefixManagementWidget::refresh);
        }
    }
    auto *maintenance = new WorkMaintenanceWidget(m_database, m_themes, m_coverDirectory,
                                                  std::move(m_topActressesEndpoint), tabs);
    tabs->addTab(maintenance, QStringLiteral("批量操作"));
    connect(maintenance, &WorkMaintenanceWidget::worksChanged, this, &ManagementPage::worksChanged);
    connect(maintenance, &WorkMaintenanceWidget::actressesChanged, this,
            &ManagementPage::actressesChanged);
    auto *softDelete =
        new WorkBatchStateWidget(WorkStateMode::Active, m_database, m_themes, {}, {}, tabs);
    auto *recycleBin = new WorkBatchStateWidget(WorkStateMode::RecycleBin, m_database, m_themes,
                                                m_coverDirectory, m_fanartDirectory, tabs);
    tabs->addTab(softDelete, QStringLiteral("作品软删除"));
    tabs->addTab(recycleBin, QStringLiteral("回收站"));
    connect(softDelete, &WorkBatchStateWidget::worksChanged, recycleBin,
            &WorkBatchStateWidget::refresh);
    connect(recycleBin, &WorkBatchStateWidget::worksChanged, softDelete,
            &WorkBatchStateWidget::refresh);
    connect(softDelete, &WorkBatchStateWidget::worksChanged, this, &ManagementPage::worksChanged);
    connect(recycleBin, &WorkBatchStateWidget::worksChanged, this, &ManagementPage::worksChanged);
    auto *workEditor =
        new WorkEditorWidget(m_database, m_themes, std::move(m_coverDirectory),
                             std::move(m_fanartDirectory), std::move(m_imageFetchEndpoint), tabs);
    tabs->addTab(workEditor, QStringLiteral("添加作品"));
    connect(workEditor, &WorkEditorWidget::workSaved, softDelete,
            [softDelete](qint64, bool) { softDelete->refresh(); });
    connect(workEditor, &WorkEditorWidget::workSaved, this,
            [this](qint64, bool) { emit worksChanged(); });
    connect(workEditor, &WorkEditorWidget::workLinkRequested,
            this, &ManagementPage::workRequested);
    connect(workEditor, &WorkEditorWidget::actressLinkRequested,
            this, &ManagementPage::actressRequested);
    connect(this, &ManagementPage::referencesChanged, workEditor,
            &WorkEditorWidget::refreshReferences);
    connect(this, &ManagementPage::tagsChanged, workEditor, &WorkEditorWidget::refreshAssociations);
    connect(this, &ManagementPage::actressesChanged, workEditor,
            &WorkEditorWidget::refreshAssociations);
    root->addWidget(tabs);
}

} // namespace darkeye
