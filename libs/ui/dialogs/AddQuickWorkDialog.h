#pragma once

#include <QDialog>

class QTableWidget;

namespace darkeye
{

class CrawlerFieldSelector;
class CrawlerScheduler;
class ThemeService;

// Batch-oriented counterpart of Python's AddQuickWork dialog.  Keeping this
// separate from the work editor makes the global shortcut useful without
// unexpectedly navigating away from the current page.
class AddQuickWorkDialog final : public QDialog
{
public:
    explicit AddQuickWorkDialog(CrawlerScheduler &crawlerScheduler, ThemeService &themes,
                                QWidget *parent = nullptr);

private:
    void addRow(const QString &serial = {});
    void deleteSelectedRows();
    void cleanSuffixes();
    void deletePrefixRows();
    void sortRows();
    void importCsv();
    void submit();
    [[nodiscard]] QStringList checkedSerials() const;

    CrawlerScheduler &m_crawlerScheduler;
    ThemeService &m_themes;
    QTableWidget *m_table = nullptr;
    CrawlerFieldSelector *m_fields = nullptr;
    bool m_sortAscending = true;
    bool m_selectiveCrawl = false;
};

} // namespace darkeye
