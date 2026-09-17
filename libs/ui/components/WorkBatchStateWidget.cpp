#include "ui/components/WorkBatchStateWidget.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenViews.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace darkeye
{

WorkBatchStateWidget::WorkBatchStateWidget(WorkStateMode mode, QSqlDatabase database,
                                           ThemeService &themes, QString coverDirectory,
                                           QString fanartDirectory, QWidget *parent)
    : QWidget(parent), m_mode(mode), m_repository(std::move(database)), m_themes(themes),
      m_coverDirectory(std::move(coverDirectory)), m_fanartDirectory(std::move(fanartDirectory))
{
    setObjectName(mode == WorkStateMode::Active ? QStringLiteral("WorkSoftDeleteWidget")
                                                : QStringLiteral("WorkRecycleBinWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    auto *toolbar = new QHBoxLayout;
    m_search = new DesignLineEdit(this);
    m_search->setObjectName(QStringLiteral("WorkStateSearch"));
    m_search->setPlaceholderText(QStringLiteral("搜索番号或标题"));
    auto *refreshButton = new DesignButton(QStringLiteral("刷新"), this);
    refreshButton->setObjectName(QStringLiteral("WorkStateRefreshButton"));
    toolbar->addWidget(m_search, 1);
    toolbar->addWidget(refreshButton);
    root->addLayout(toolbar);

    m_table = new TokenTableWidget(0, 6, this);
    m_table->setObjectName(QStringLiteral("WorkStateTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("选择"), QStringLiteral("ID"),
                                        QStringLiteral("番号"), QStringLiteral("中文标题"),
                                        QStringLiteral("日文标题"), QStringLiteral("发布日期")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    auto *buttons = new QHBoxLayout;
    auto *checkAllButton = new DesignButton(QStringLiteral("全选"), this);
    checkAllButton->setObjectName(QStringLiteral("WorkStateCheckAllButton"));
    auto *uncheckAllButton = new DesignButton(QStringLiteral("全不选"), this);
    uncheckAllButton->setObjectName(QStringLiteral("WorkStateUncheckAllButton"));
    buttons->addWidget(checkAllButton);
    buttons->addWidget(uncheckAllButton);
    buttons->addStretch();
    if (m_mode == WorkStateMode::Active)
    {
        auto *removeButton = new DesignButton(QStringLiteral("软删除选中"), this);
        removeButton->setObjectName(QStringLiteral("WorkSoftDeleteButton"));
        removeButton->setVariant(QStringLiteral("danger"));
        buttons->addWidget(removeButton);
        connect(removeButton, &QPushButton::clicked, this,
                &WorkBatchStateWidget::moveCheckedToRecycleBin);
    }
    else
    {
        auto *restoreButton = new DesignButton(QStringLiteral("恢复选中"), this);
        restoreButton->setObjectName(QStringLiteral("WorkRestoreButton"));
        auto *restoreAllButton = new DesignButton(QStringLiteral("恢复全部"), this);
        restoreAllButton->setObjectName(QStringLiteral("WorkRestoreAllButton"));
        auto *removeButton = new DesignButton(QStringLiteral("彻底删除选中"), this);
        removeButton->setObjectName(QStringLiteral("WorkPermanentDeleteButton"));
        removeButton->setVariant(QStringLiteral("danger"));
        auto *removeAllButton = new DesignButton(QStringLiteral("删除全部"), this);
        removeAllButton->setObjectName(QStringLiteral("WorkPermanentDeleteAllButton"));
        removeAllButton->setVariant(QStringLiteral("danger"));
        buttons->addWidget(restoreButton);
        buttons->addWidget(restoreAllButton);
        buttons->addWidget(removeButton);
        buttons->addWidget(removeAllButton);
        connect(restoreButton, &QPushButton::clicked, this, [this] { restore(false); });
        connect(restoreAllButton, &QPushButton::clicked, this, [this] { restore(true); });
        connect(removeButton, &QPushButton::clicked, this, [this] { removePermanently(false); });
        connect(removeAllButton, &QPushButton::clicked, this, [this] { removePermanently(true); });
    }
    root->addLayout(buttons);
    connect(refreshButton, &QPushButton::clicked, this, &WorkBatchStateWidget::refresh);
    connect(checkAllButton, &QPushButton::clicked, this, [this] { setAllChecked(true); });
    connect(uncheckAllButton, &QPushButton::clicked, this, [this] { setAllChecked(false); });
    connect(m_search, &QLineEdit::textChanged, this, &WorkBatchStateWidget::refresh);
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int)
            {
                if (QTableWidgetItem *item = m_table->item(row, 0))
                    item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked
                                                                          : Qt::Checked);
            });
    refresh();
}

void WorkBatchStateWidget::refresh()
{
    QString errorMessage;
    m_records = m_repository.listByDeletedState(m_mode == WorkStateMode::RecycleBin,
                                                m_search->text(), &errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_table->setRowCount(m_records.size());
    for (qsizetype row = 0; row < m_records.size(); ++row)
    {
        const WorkStateRecord &record = m_records.at(row);
        auto *checkItem = new QTableWidgetItem;
        checkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(Qt::UserRole, record.id);
        m_table->setItem(row, 0, checkItem);
        const QStringList values{QString::number(record.id), record.serialNumber,
                                 record.chineseTitle, record.japaneseTitle, record.releaseDate};
        for (int column = 0; column < values.size(); ++column)
            m_table->setItem(row, column + 1, new QTableWidgetItem(values.at(column)));
    }
}

QList<qint64> WorkBatchStateWidget::checkedIds() const
{
    QList<qint64> ids;
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        const QTableWidgetItem *item = m_table->item(row, 0);
        if (item != nullptr && item->checkState() == Qt::Checked)
            ids.append(item->data(Qt::UserRole).toLongLong());
    }
    return ids;
}

QList<qint64> WorkBatchStateWidget::allIds() const
{
    QList<qint64> ids;
    ids.reserve(m_records.size());
    for (const WorkStateRecord &record : m_records)
        ids.append(record.id);
    return ids;
}

void WorkBatchStateWidget::setAllChecked(bool checked)
{
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        if (QTableWidgetItem *item = m_table->item(row, 0))
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

void WorkBatchStateWidget::moveCheckedToRecycleBin()
{
    const QList<qint64> ids = checkedIds();
    if (ids.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("请先勾选要软删除的作品"), &m_themes);
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("确认软删除"),
            QStringLiteral("确定将选中的 %1 部作品移入回收站？").arg(ids.size())) !=
        QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.setDeletedMany(ids, true, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    refresh();
    emit worksChanged();
    Toast::showSuccess(window(), QStringLiteral("作品已移入回收站"), &m_themes);
}

void WorkBatchStateWidget::restore(bool all)
{
    const QList<qint64> ids = all ? allIds() : checkedIds();
    if (ids.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("回收站中没有待恢复的作品"), &m_themes);
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认恢复"),
                              QStringLiteral("确定恢复 %1 部作品？").arg(ids.size())) !=
        QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.setDeletedMany(ids, false, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    refresh();
    emit worksChanged();
    Toast::showSuccess(window(), QStringLiteral("作品已恢复"), &m_themes);
}

void WorkBatchStateWidget::removePermanently(bool all)
{
    const QList<qint64> ids = all ? allIds() : checkedIds();
    if (ids.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("回收站中没有可删除的作品"), &m_themes);
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("确认彻底删除"),
            QStringLiteral("确定彻底删除 %1 部作品？此操作不可撤销。").arg(ids.size())) !=
        QMessageBox::Yes)
        return;
    QStringList imageUrls;
    QStringList fanartFiles;
    QString errorMessage;
    if (!m_repository.permanentlyRemoveDeletedMany(ids, &imageUrls, &errorMessage, &fanartFiles))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    removeManagedFiles(m_coverDirectory, imageUrls);
    removeManagedFiles(m_fanartDirectory, fanartFiles);
    refresh();
    emit worksChanged();
    Toast::showSuccess(window(), QStringLiteral("作品已彻底删除"), &m_themes);
}

void WorkBatchStateWidget::removeManagedFiles(const QString &directory,
                                              const QStringList &relativeFiles)
{
    const QString root = QDir(directory).canonicalPath();
    if (root.isEmpty())
        return;
    const QString normalizedRoot = QDir::fromNativeSeparators(QDir::cleanPath(root));
    const QString rootPrefix = normalizedRoot + QChar('/');
    for (const QString &relativeFile : relativeFiles)
    {
        if (QFileInfo(relativeFile).isAbsolute())
            continue;
        const QFileInfo candidate(QDir(root).filePath(relativeFile));
        const QString canonical = QDir::fromNativeSeparators(candidate.canonicalFilePath());
        if (!canonical.isEmpty() && canonical.startsWith(rootPrefix, Qt::CaseInsensitive))
            QFile::remove(canonical);
    }
}

} // namespace darkeye


