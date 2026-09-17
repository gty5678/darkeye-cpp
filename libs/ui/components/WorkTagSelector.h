#pragma once

#include "domain/Work.h"

#include <QHash>
#include <QSet>
#include <QWidget>
#include <functional>

class QLineEdit;
class QLabel;
class QScrollArea;
class QTabWidget;
class QVariantAnimation;

namespace darkeye
{

class IconButton;
class TagCard;
class TagFlowWidget;
class ThemeService;

class WorkTagSelector final : public QWidget
{
    Q_OBJECT

  public:
    using Loader = std::function<QList<TagOption>()>;

    explicit WorkTagSelector(const QList<TagOption> &tags, ThemeService *themes = nullptr, QWidget *parent = nullptr);

    [[nodiscard]] QList<qint64> selectedIds() const;
    void clearSelection();
    void setLoader(Loader loader);
    void reloadTags();

  signals:
    void selectionChanged();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void setPanelExpanded(bool expanded);
    void setTagSelected(qint64 tagId, bool selected, bool switchTab = true);
    void rebuildSearch(const QString &text);
    void navigateSearch(int offset);
    void showSearchResult();
    void updateAvailableVisibility();
    void setTags(const QList<TagOption> &tags);
    [[nodiscard]] const TagOption *tagById(qint64 tagId) const;
    void applyViewStyle(QWidget *view) const;
    void applyTabStyle() const;

    QList<TagOption> m_tags;
    QHash<qint64, TagCard *> m_items;
    QHash<qint64, TagCard *> m_selectedItems;
    QHash<QString, TagFlowWidget *> m_flows;
    QHash<QString, QScrollArea *> m_views;
    QSet<qint64> m_selectedIds;
    QList<qint64> m_selectedOrder;
    TagFlowWidget *m_selectedFlow = nullptr;
    QScrollArea *m_selectedView = nullptr;
    QWidget *m_panel = nullptr;
    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_search = nullptr;
    QLabel *m_searchResult = nullptr;
    IconButton *m_searchPrevious = nullptr;
    IconButton *m_searchNext = nullptr;
    IconButton *m_expandButton = nullptr;
    QVariantAnimation *m_panelAnimation = nullptr;
    bool m_panelExpanded = false;
    bool m_updating = false;
    QList<qint64> m_searchMatches;
    int m_searchIndex = -1;
    Loader m_loader;
    ThemeService *m_themes = nullptr;
};

} // namespace darkeye
