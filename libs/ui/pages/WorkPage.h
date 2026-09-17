#pragma once

#include "darkeye_ui/base/LazyWidget.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/repositories/PrivateRepository.h"
#include "database/repositories/WorkRepository.h"
#include "settings/Settings.h"

#include <QSqlDatabase>
#include <QUrl>
#include <optional>

class QLabel;
class QComboBox;
class QDialog;
class QTableWidget;
class QTimer;

namespace darkeye
{

class DesignLineEdit;
class CompleterLineEdit;
class IconButton;
class LazyScrollArea;
class WorkTagSelector;
class WorkEditorWidget;

class WorkPage final : public LazyWidget
{
    Q_OBJECT

public:
    explicit WorkPage(
        QSqlDatabase database, ThemeService &themeService, Settings &settings,
        QString coverDirectory = {}, QSqlDatabase privateDatabase = {},
        QString fanartDirectory = {},
        QUrl imageFetchEndpoint = QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/image")),
        QWidget *parent = nullptr);

    void refresh();
    void refreshReferences();
    void refreshTags();
    void openEditor(qint64 workId);
    void openCreateEditor();

signals:
    void detailRequested(qint64 workId);

private:
    void lazyLoad() override;
    void buildUi();
    void refreshData();
    void applyFilters();
    void clearFilters();
    void toggleCoverSize();
    [[nodiscard]] QList<QWidget *> loadCardPage(int pageIndex, int pageSize);
    void buildEditor();
    void loadSelectedWork();
    void showWorkDetails(qint64 workId);
    void loadWork(qint64 workId);
    [[nodiscard]] WorkSortOrder selectedSortOrder() const;
    [[nodiscard]] WorkSearch currentSearch() const;

    ThemeService &m_themeService;
    Settings &m_settings;
    QSqlDatabase m_database;
    WorkRepository m_repository;
    PrivateRepository m_privateRepository;
    QString m_coverDirectory;
    QString m_fanartDirectory;
    QUrl m_imageFetchEndpoint;
    DesignLineEdit *m_searchInput = nullptr;
    CompleterLineEdit *m_serialFilter = nullptr;
    DesignLineEdit *m_titleFilter = nullptr;
    DesignLineEdit *m_storyFilter = nullptr;
    CompleterLineEdit *m_directorFilter = nullptr;
    CompleterLineEdit *m_actressFilter = nullptr;
    CompleterLineEdit *m_actorFilter = nullptr;
    QComboBox *m_makerFilter = nullptr;
    QComboBox *m_labelFilter = nullptr;
    QComboBox *m_seriesFilter = nullptr;
    WorkTagSelector *m_tagSelector = nullptr;
    QComboBox *m_sortSelector = nullptr;
    QComboBox *m_scopeSelector = nullptr;
    QTimer *m_filterTimer = nullptr;
    quint32 m_randomSeed = 1;
    quint32 m_randomSeed2 = 1;
    QLabel *m_countLabel = nullptr;
    QWidget *m_tagPanel = nullptr;
    IconButton *m_tagPanelButton = nullptr;
    IconButton *m_viewButton = nullptr;
    QTableWidget *m_table = nullptr;
    LazyScrollArea *m_lazyArea = nullptr;
    bool m_largeCoverView = false;
    bool m_tagPanelVisible = true;
    WorkEditorWidget *m_editor = nullptr;
    QDialog *m_editorDialog = nullptr;
};

} // namespace darkeye
