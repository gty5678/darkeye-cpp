#pragma once

#include <QList>
#include <QScrollArea>
#include <functional>

class QWidget;
class QVBoxLayout;

namespace darkeye {

class WaterfallLayout;

class LazyScrollArea final : public QScrollArea
{
public:
    using Loader = std::function<QList<QWidget *>(int pageIndex, int pageSize)>;

    explicit LazyScrollArea(int columnWidth = 220, QWidget *parent = nullptr);

    void setLoader(Loader loader);
    void reset();
    bool loadNextPage();
    void setHeaderWidget(QWidget *widget);
    QWidget *headerWidget() const;

    int pageSize() const;
    void setPageSize(int pageSize);
    int currentPage() const;
    bool reachedEnd() const;
    bool loading() const;
    int itemCount() const;

    int columnWidth() const;
    void setColumnWidth(int columnWidth);
    void setPrefetchDistance(int distance);

private:
    void handleScroll(int value);
    void scheduleScrollableCheck();
    void checkScrollableAndLoad(quint64 generation);

    QWidget *m_contentWidget = nullptr;
    QWidget *m_waterfallWidget = nullptr;
    QWidget *m_headerWidget = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    WaterfallLayout *m_layout = nullptr;
    Loader m_loader;
    int m_pageSize = 70;
    int m_currentPage = 0;
    int m_prefetchDistance = 300;
    int m_scrollCheckRetries = 0;
    quint64 m_generation = 0;
    bool m_reachedEnd = false;
    bool m_loading = false;
};

} // namespace darkeye
