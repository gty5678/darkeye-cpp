#pragma once

#include "darkeye_ui/base/LazyWidget.h"

#include <QHash>
#include <QString>

class QFileSystemWatcher;
class QShowEvent;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;
class QUrl;

namespace darkeye
{

class AvPage final : public LazyWidget
{
public:
    explicit AvPage(QWidget *parent = nullptr);
    explicit AvPage(QString wikiDirectory, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void lazyLoad() override;
    [[nodiscard]] QString resolveWikiDirectory() const;
    void rebuildTree();
    void restoreSelection();
    void selectFirstDocument();
    [[nodiscard]] QTreeWidgetItem *firstDocument(QTreeWidgetItem *item) const;
    void openItem(QTreeWidgetItem *item);
    void loadDocument(const QString &filePath);
    void handleLink(const QUrl &url);

    QString m_requestedWikiDirectory;
    QString m_wikiDirectory;
    QString m_currentPath;
    QTreeWidget *m_tree = nullptr;
    QTextBrowser *m_browser = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QHash<QString, QString> m_stemToPath;
    QHash<QString, QTreeWidgetItem *> m_pathToItem;
};

} // namespace darkeye
