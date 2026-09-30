#include "ui/pages/AvPage.h"

#include "darkeye_ui/components/DesignButton.h"
#include "settings/Paths.h"

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QShowEvent>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace darkeye
{
namespace
{
constexpr auto documentPathRole = Qt::UserRole;

QString normalizedPath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString preprocessWikiLinks(QString markdown)
{
    static const QRegularExpression wikiLink(QStringLiteral(R"(\[\[(.+?)\]\])"));
    markdown.replace(wikiLink, QStringLiteral(R"(<a href="internal:\1">\1</a>)"));
    return markdown;
}
} // namespace

AvPage::AvPage(QWidget *parent) : AvPage({}, parent)
{
}

AvPage::AvPage(QString wikiDirectory, QWidget *parent)
    : LazyWidget(parent), m_requestedWikiDirectory(std::move(wikiDirectory))
{
}

void AvPage::lazyLoad()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("AvWikiSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);

    m_tree = new QTreeWidget(splitter);
    m_tree->setObjectName(QStringLiteral("AvWikiTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);

    auto *content = new QWidget(splitter);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QHBoxLayout;
    auto *previewButton = new Button(QStringLiteral("预览"), content);
    previewButton->setCheckable(true);
    previewButton->setChecked(true);
    auto *editButton = new Button(QStringLiteral("编辑"), content);
    editButton->setCheckable(true);
    editButton->setEnabled(false);
    editButton->setToolTip(QStringLiteral("知识库当前为只读内容"));
    toolbar->addWidget(previewButton);
    toolbar->addWidget(editButton);
    toolbar->addStretch();
    contentLayout->addLayout(toolbar);

    m_browser = new QTextBrowser(content);
    m_browser->setObjectName(QStringLiteral("AvWikiBrowser"));
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    QFont contentFont = m_browser->font();
    contentFont.setPointSize(13);
    m_browser->setFont(contentFont);
    contentLayout->addWidget(m_browser, 1);

    splitter->addWidget(m_tree);
    splitter->addWidget(content);
    splitter->setSizes({220, 2000});
    layout->addWidget(splitter);

    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int) { openItem(item); });
    connect(m_browser, &QTextBrowser::anchorClicked, this,
            [this](const QUrl &url) { handleLink(url); });

    m_wikiDirectory = resolveWikiDirectory();
    m_watcher = new QFileSystemWatcher(this);
    if (QFileInfo::exists(m_wikiDirectory))
    {
        m_watcher->addPath(m_wikiDirectory);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
                [this] {
                    rebuildTree();
                    restoreSelection();
                });
    }

    rebuildTree();
    selectFirstDocument();
}

void AvPage::showEvent(QShowEvent *event)
{
    LazyWidget::showEvent(event);
    if (m_tree == nullptr) return;
    rebuildTree();
    restoreSelection();
    if (m_currentPath.isEmpty()) selectFirstDocument();
}

QString AvPage::resolveWikiDirectory() const
{
    if (!m_requestedWikiDirectory.trimmed().isEmpty())
        return normalizedPath(m_requestedWikiDirectory);

    const QString installed = QDir(settings::Paths().resourcesDirectory())
                                  .filePath(QStringLiteral("avwiki"));
    if (QFileInfo::exists(installed)) return normalizedPath(installed);

#ifdef DARKEYE_SOURCE_DIR
    const QString source = QDir(QStringLiteral(DARKEYE_SOURCE_DIR))
                               .filePath(QStringLiteral("resources/avwiki"));
    if (QFileInfo::exists(source)) return normalizedPath(source);
#endif
    return normalizedPath(installed);
}

void AvPage::rebuildTree()
{
    if (m_tree == nullptr) return;
    m_tree->clear();
    m_stemToPath.clear();
    m_pathToItem.clear();

    QDir root(m_wikiDirectory);
    if (!root.exists())
    {
        m_browser->setPlainText(QStringLiteral("未找到知识库目录：%1").arg(m_wikiDirectory));
        return;
    }

    QStringList files;
    QDirIterator iterator(m_wikiDirectory, {QStringLiteral("*.md")}, QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) files.push_back(normalizedPath(iterator.next()));
    std::sort(files.begin(), files.end(), [](const QString &left, const QString &right) {
        return QString::localeAwareCompare(left, right) < 0;
    });

    QHash<QString, QTreeWidgetItem *> directoryItems;
    for (const QString &filePath : files)
    {
        const QString relativePath = QDir::fromNativeSeparators(root.relativeFilePath(filePath));
        const QStringList parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        QTreeWidgetItem *parentItem = nullptr;
        QString directoryKey;
        for (qsizetype index = 0; index + 1 < parts.size(); ++index)
        {
            if (!directoryKey.isEmpty()) directoryKey += QLatin1Char('/');
            directoryKey += parts.at(index);
            auto *directoryItem = directoryItems.value(directoryKey, nullptr);
            if (directoryItem == nullptr)
            {
                directoryItem = parentItem == nullptr
                                    ? new QTreeWidgetItem(m_tree, {parts.at(index)})
                                    : new QTreeWidgetItem(parentItem, {parts.at(index)});
                directoryItems.insert(directoryKey, directoryItem);
            }
            parentItem = directoryItem;
        }

        const QString stem = QFileInfo(filePath).completeBaseName();
        auto *item = parentItem == nullptr ? new QTreeWidgetItem(m_tree, {stem})
                                           : new QTreeWidgetItem(parentItem, {stem});
        item->setData(0, documentPathRole, filePath);
        if (!m_stemToPath.contains(stem)) m_stemToPath.insert(stem, filePath);
        m_pathToItem.insert(filePath, item);
    }
}

void AvPage::restoreSelection()
{
    if (m_currentPath.isEmpty() || m_tree == nullptr) return;
    auto *item = m_pathToItem.value(normalizedPath(m_currentPath), nullptr);
    if (item == nullptr)
    {
        m_currentPath.clear();
        return;
    }
    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);
    loadDocument(m_currentPath);
}

void AvPage::selectFirstDocument()
{
    if (m_tree == nullptr || m_tree->topLevelItemCount() == 0) return;
    auto *item = firstDocument(m_tree->invisibleRootItem());
    if (item == nullptr) return;
    m_tree->setCurrentItem(item);
    openItem(item);
}

QTreeWidgetItem *AvPage::firstDocument(QTreeWidgetItem *item) const
{
    if (item == nullptr) return nullptr;
    if (!item->data(0, documentPathRole).toString().isEmpty()) return item;
    for (int index = 0; index < item->childCount(); ++index)
    {
        if (auto *document = firstDocument(item->child(index)); document != nullptr)
            return document;
    }
    return nullptr;
}

void AvPage::openItem(QTreeWidgetItem *item)
{
    if (item == nullptr) return;
    const QString filePath = item->data(0, documentPathRole).toString();
    if (filePath.isEmpty()) return;
    m_currentPath = normalizedPath(filePath);
    loadDocument(m_currentPath);
    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);
}

void AvPage::loadDocument(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_browser->setPlainText(QStringLiteral("读取失败：%1").arg(file.errorString()));
        return;
    }
    m_browser->setMarkdown(preprocessWikiLinks(QString::fromUtf8(file.readAll())));
}

void AvPage::handleLink(const QUrl &url)
{
    if (url.scheme() != QStringLiteral("internal"))
    {
        QDesktopServices::openUrl(url);
        return;
    }

    QString page = QUrl::fromPercentEncoding(url.path().toUtf8());
    if (page.startsWith(QLatin1Char('/'))) page.remove(0, 1);
    const QString filePath = m_stemToPath.value(page);
    if (filePath.isEmpty())
    {
        m_browser->setPlainText(QStringLiteral("未找到页面：%1").arg(page));
        return;
    }
    m_currentPath = filePath;
    loadDocument(filePath);
    if (auto *item = m_pathToItem.value(filePath, nullptr); item != nullptr)
    {
        m_tree->setCurrentItem(item);
        m_tree->scrollToItem(item);
    }
}

} // namespace darkeye
