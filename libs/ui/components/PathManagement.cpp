#include "ui/components/PathManagement.h"

#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/TokenViews.h"

#include <QApplication>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QStyleOptionButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace darkeye
{

class PathBrowseDelegate final : public QStyledItemDelegate
{
public:
    using BrowseHandler = std::function<void(int)>;

    explicit PathBrowseDelegate(BrowseHandler browseHandler, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_browseHandler(std::move(browseHandler))
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (!m_visibleRows.contains(index.row()))
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionButton button;
        button.rect = option.rect;
        button.text = QStringLiteral("...");
        button.state = QStyle::State_Enabled;
        if (option.state.testFlag(QStyle::State_MouseOver))
        {
            button.state |= QStyle::State_MouseOver;
        }
        QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (!m_visibleRows.contains(index.row()))
        {
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
            return true;
        case QEvent::MouseButtonRelease:
        {
            const auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                m_browseHandler(index.row());
            }
            return true;
        }
        default:
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        if (m_visibleRows.contains(index.row()))
        {
            size.rwidth() += 10;
        }
        return size;
    }

    void showForRow(int row)
    {
        m_visibleRows.insert(row);
    }

    void clear()
    {
        m_visibleRows.clear();
    }

private:
    QSet<int> m_visibleRows;
    BrowseHandler m_browseHandler;
};

SinglePathManagement::SinglePathManagement(const QString &labelText, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("SinglePathManagement"));
    resize(400, 100);

    auto *layout = new QHBoxLayout(this);
    layout->addWidget(new QLabel(labelText, this));
    m_filePath = new QLineEdit(this);
    m_filePath->setObjectName(QStringLiteral("SinglePathLineEdit"));
    m_filePath->setMinimumWidth(300);
    layout->addWidget(m_filePath);
    auto *browse = new QPushButton(QStringLiteral("..."), this);
    browse->setObjectName(QStringLiteral("SinglePathBrowseButton"));
    browse->setMaximumWidth(30);
    layout->addWidget(browse);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString current = m_filePath->text();
        const QString initialDirectory = QFileInfo(current).isDir() ? current : QString{};
        const QString folder = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择文件夹"), initialDirectory);
        if (!folder.isEmpty())
        {
            m_filePath->setText(folder);
        }
    });
}

QString SinglePathManagement::path() const
{
    return m_filePath->text();
}

void SinglePathManagement::loadPath(const QString &path)
{
    m_filePath->setText(path);
}

MultiplePathManagement::MultiplePathManagement(const QString &labelText, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("MultiplePathManagement"));
    auto *layout = new QVBoxLayout(this);
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(new DesignLabel(labelText, this));
    auto *addButton = new DesignButton(QStringLiteral("添加地址"), this);
    addButton->setObjectName(QStringLiteral("MultiplePathAddButton"));
    auto *deleteButton = new DesignButton(QStringLiteral("删除地址"), this);
    deleteButton->setObjectName(QStringLiteral("MultiplePathDeleteButton"));
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    m_table = new TokenTableWidget(this);
    m_table->setObjectName(QStringLiteral("MultiplePathTable"));
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("路径（双击输入或选按钮）"), QStringLiteral("操作")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setRowCount(0);
    m_table->horizontalHeader()->hide();
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 22);
    m_delegate = new PathBrowseDelegate([this](int row) { browseRow(row); }, m_table);
    m_table->setItemDelegateForColumn(1, m_delegate);
    layout->addWidget(m_table);

    connect(addButton, &QPushButton::clicked, this, &MultiplePathManagement::addRow);
    connect(deleteButton, &QPushButton::clicked, this,
            &MultiplePathManagement::deleteSelectedRows);
    connect(m_table, &TokenTableWidget::addRequested, this, &MultiplePathManagement::addRow);
    connect(m_table, &TokenTableWidget::deleteRequested, this,
            &MultiplePathManagement::deleteSelectedRows);
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            &MultiplePathManagement::handleCellDoubleClicked);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MultiplePathManagement::clearBrowseButtons);
}

QStringList MultiplePathManagement::paths() const
{
    QStringList result;
    result.reserve(m_table->rowCount());
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        const auto *item = m_table->item(row, 0);
        result.append(item ? item->text().trimmed() : QString{});
    }
    return result;
}

void MultiplePathManagement::loadPaths(const QStringList &paths)
{
    m_table->setRowCount(0);
    m_delegate->clear();
    for (const QString &path : paths)
    {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *pathItem = new QTableWidgetItem(path);
        pathItem->setFlags(pathItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(row, 0, pathItem);
        m_table->setItem(row, 1, new QTableWidgetItem);
    }
}

QTableWidget *MultiplePathManagement::table() const
{
    return m_table;
}

void MultiplePathManagement::addRow()
{
    m_delegate->clear();
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *pathItem = new QTableWidgetItem;
    pathItem->setFlags(pathItem->flags() | Qt::ItemIsEditable);
    m_table->setItem(row, 0, pathItem);
    m_table->setItem(row, 1, new QTableWidgetItem);
    m_table->setCurrentCell(row, 0);
    handleCellDoubleClicked(row, 0);
}

void MultiplePathManagement::deleteSelectedRows()
{
    QSet<int> selectedRows;
    for (const QModelIndex &index : m_table->selectionModel()->selectedIndexes())
    {
        selectedRows.insert(index.row());
    }
    if (selectedRows.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要删除的行"));
        return;
    }
    QList<int> rows(selectedRows.cbegin(), selectedRows.cend());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (const int row : rows)
    {
        m_table->removeRow(row);
    }
    m_delegate->clear();
}

void MultiplePathManagement::handleCellDoubleClicked(int row, int column)
{
    m_delegate->showForRow(row);
    m_table->viewport()->update();
    if (column == 0)
    {
        m_table->editItem(m_table->item(row, 0));
    }
}

void MultiplePathManagement::browseRow(int row)
{
    QTableWidgetItem *item = m_table->item(row, 0);
    if (!item)
    {
        return;
    }
    const QString current = item->text();
    const QString initialDirectory = QFileInfo(current).isDir() ? current : QString{};
    const QString folder = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择文件夹"), initialDirectory);
    if (!folder.isEmpty())
    {
        item->setText(folder);
    }
}

void MultiplePathManagement::clearBrowseButtons()
{
    m_delegate->clear();
    m_table->viewport()->update();
}

} // namespace darkeye
