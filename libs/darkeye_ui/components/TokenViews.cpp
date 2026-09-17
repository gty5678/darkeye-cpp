#include "darkeye_ui/components/TokenViews.h"

#include <QLineEdit>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

namespace darkeye {
namespace {

class TableEditorDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem clean(option);
        clean.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, clean, index);
    }

    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    {
        editor->setGeometry(option.rect);
    }
};

template<typename View>
void initializeTable(View *view, const QString &name)
{
    view->setObjectName(name);
    view->setItemDelegate(new TableEditorDelegate(view));
}

} // namespace

TokenTableView::TokenTableView(QWidget *parent) : QTableView(parent)
{
    initializeTable(this, QStringLiteral("DesignTableView"));
}

TokenTableWidget::TokenTableWidget(QWidget *parent) : QTableWidget(parent)
{
    initializeTable(this, QStringLiteral("DesignTableWidget"));
}

TokenTableWidget::TokenTableWidget(int rows, int columns, QWidget *parent)
    : QTableWidget(rows, columns, parent)
{
    initializeTable(this, QStringLiteral("DesignTableWidget"));
}

TreeView::TreeView(QWidget *parent) : QTreeView(parent)
{
    setObjectName(QStringLiteral("DesignTreeView"));
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
}

TokenListView::TokenListView(QWidget *parent) : QListView(parent)
{
    setObjectName(QStringLiteral("DesignListView"));
}

TokenListWidget::TokenListWidget(QWidget *parent) : QListWidget(parent)
{
    setObjectName(QStringLiteral("DesignListWidget"));
}

TokenDateTimeEdit::TokenDateTimeEdit(QWidget *parent) : QDateTimeEdit(parent)
{
    setObjectName(QStringLiteral("DesignDateTimeEdit"));
}

TokenKeySequenceEdit::TokenKeySequenceEdit(QWidget *parent)
    : QKeySequenceEdit(parent)
{
    setObjectName(QStringLiteral("DesignKeySequenceEdit"));
    for (auto *lineEdit : findChildren<QLineEdit *>()) {
        lineEdit->setObjectName(QStringLiteral("DesignKeySequenceEditLineEdit"));
    }
}

} // namespace darkeye
