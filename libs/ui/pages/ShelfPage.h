#pragma once

#include "darkeye_ui/base/LazyWidget.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "domain/Work.h"

#include <QSqlDatabase>

class QComboBox;
class QLabel;
class QTimer;

namespace darkeye
{

class CompleterLineEdit;
class DesignLineEdit;
class IconButton;
class DvdShelfView;
class WorkTagSelector;
class MakerSelector;
namespace graph {
class GraphManager;
}

// Uses the same filtering contract as Python's DVD shelf.  The native 3D
// renderer is intentionally isolated from this data and interaction layer.
class ShelfPage final : public LazyWidget
{
    Q_OBJECT

public:
    explicit ShelfPage(QSqlDatabase database, QSqlDatabase privateDatabase,
                       ThemeService &themeService, graph::GraphManager *graphManager = nullptr,
                       QString coverDirectory = {}, QString fanartDirectory = {},
                       QWidget *parent = nullptr);

    void refresh();
    void refreshReferences();
    void refreshTags();
    void filterByDirector(const QString &director);
    void filterByMaker(qint64 makerId);
    void filterByLabel(qint64 labelId);
    void filterBySeries(qint64 seriesId);
    void filterByTag(qint64 tagId);
    bool showWork(qint64 workId);

signals:
    void detailRequested(qint64 workId);
    void editRequested(qint64 workId);
    void workDeleted(qint64 workId);
    void actressRequested(qint64 actressId);
    void actorRequested(qint64 actorId);
    void tagRequested(qint64 tagId);

private:
    void lazyLoad() override;
    void buildUi();
    void applyFilters();
    void clearFilters();
    void toggleTagPanel();
    void refreshData();
    void reloadDvdScene();
    void resetForRoute();
    [[nodiscard]] WorkSearch currentSearch() const;
    [[nodiscard]] WorkSortOrder selectedSortOrder() const;

    QSqlDatabase m_database;
    QSqlDatabase m_privateDatabase;
    ThemeService &m_themeService;
    graph::GraphManager *m_graphManager = nullptr;
    QString m_coverDirectory;
    QString m_fanartDirectory;
    CompleterLineEdit *m_serialFilter = nullptr;
    CompleterLineEdit *m_actressFilter = nullptr;
    DesignLineEdit *m_titleFilter = nullptr;
    DesignLineEdit *m_notesFilter = nullptr;
    CompleterLineEdit *m_directorFilter = nullptr;
    CompleterLineEdit *m_actorFilter = nullptr;
    MakerSelector *m_makerFilter = nullptr;
    MakerSelector *m_labelFilter = nullptr;
    MakerSelector *m_seriesFilter = nullptr;
    QComboBox *m_scopeSelector = nullptr;
    QComboBox *m_sortSelector = nullptr;
    WorkTagSelector *m_tagSelector = nullptr;
    QWidget *m_tagPanel = nullptr;
    IconButton *m_tagPanelButton = nullptr;
    DvdShelfView *m_shelfView = nullptr;
    QLabel *m_countLabel = nullptr;
    QTimer *m_filterTimer = nullptr;
    quint32 m_randomSeed = 1;
    quint32 m_randomSeed2 = 1;
    bool m_tagPanelVisible = true;
};

} // namespace darkeye
