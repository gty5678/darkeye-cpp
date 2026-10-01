#include "ui/pages/management/PersonalRecordManagementWidget.h"

#include "database/CsvExport.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenViews.h"
#include "darkeye_ui/theme/ThemeService.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QModelIndexList>
#include <QSortFilterProxyModel>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace darkeye
{
namespace
{
class AllColumnsFilterModel final : public QSortFilterProxyModel
{
public:
    explicit AllColumnsFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const QString needle = filterRegularExpression().pattern().trimmed();
        if (needle.isEmpty())
            return true;
        const auto *model = sourceModel();
        for (int column = 0; column < model->columnCount(sourceParent); ++column)
            if (model->index(sourceRow, column, sourceParent).data().toString().contains(
                    needle, Qt::CaseInsensitive))
                return true;
        return false;
    }
};
} // namespace

PersonalRecordManagementWidget::PersonalRecordManagementWidget(QSqlDatabase privateDatabase,
                                                               ThemeService &themes, QWidget *parent)
    : QWidget(parent), m_privateDatabase(std::move(privateDatabase)), m_themes(themes)
{
    setObjectName(QStringLiteral("PersonalRecordManagementWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(new DesignLabel(QStringLiteral("选择表:"), this));
    m_tableChoice = new DesignComboBox(this);
    m_tableChoice->setObjectName(QStringLiteral("personalRecordTableChoice"));
    m_tableChoice->addItems({QStringLiteral("love_making"), QStringLiteral("masturbation"),
                             QStringLiteral("sexual_arousal")});
    toolbar->addWidget(m_tableChoice);
    auto *addButton = new DesignButton(QStringLiteral("新增行"), this);
    auto *deleteButton = new DesignButton(QStringLiteral("删除行"), this);
    auto *saveButton = new DesignButton(QStringLiteral("保存修改"), this);
    auto *revertButton = new DesignButton(QStringLiteral("撤销修改"), this);
    auto *refreshButton = new DesignButton(QStringLiteral("刷新数据"), this);
    auto *exportButton = new DesignButton(QStringLiteral("导出为 CSV"), this);
    for (QPushButton *button : {addButton, deleteButton, saveButton, revertButton, refreshButton,
                                exportButton})
        toolbar->addWidget(button);
    toolbar->addStretch();
    root->addLayout(toolbar);

    m_model = new QSqlTableModel(this, m_privateDatabase);
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_filterModel = new AllColumnsFilterModel(this);
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setDynamicSortFilter(true);
    m_table = new TokenTableView(this);
    m_table->setObjectName(QStringLiteral("personalRecordTable"));
    m_table->setModel(m_filterModel);
    m_table->setSortingEnabled(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(m_table, 1);

    m_search = new DesignLineEdit(this);
    m_search->setObjectName(QStringLiteral("personalRecordSearch"));
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(QStringLiteral("搜索"));
    root->addWidget(m_search);

    connect(m_tableChoice, &QComboBox::currentTextChanged, this,
            &PersonalRecordManagementWidget::loadTable);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filterModel->setFilterFixedString(text);
    });
    connect(addButton, &QPushButton::clicked, this, &PersonalRecordManagementWidget::addRow);
    connect(deleteButton, &QPushButton::clicked, this,
            &PersonalRecordManagementWidget::deleteSelectedRows);
    connect(saveButton, &QPushButton::clicked, this, &PersonalRecordManagementWidget::saveChanges);
    connect(revertButton, &QPushButton::clicked, this,
            &PersonalRecordManagementWidget::revertChanges);
    connect(refreshButton, &QPushButton::clicked, this, &PersonalRecordManagementWidget::refresh);
    connect(exportButton, &QPushButton::clicked, this, &PersonalRecordManagementWidget::exportCsv);

    loadTable(m_tableChoice->currentText());
}

void PersonalRecordManagementWidget::refresh()
{
    if (!m_model->select())
        Toast::showError(window(), QStringLiteral("刷新数据失败: %1").arg(m_model->lastError().text()),
                         &m_themes);
}

void PersonalRecordManagementWidget::loadTable(const QString &tableName)
{
    m_model->revertAll();
    m_model->setTable(tableName);
    if (!m_model->select())
    {
        Toast::showError(window(), QStringLiteral("加载表 %1 失败: %2")
                                      .arg(tableName, m_model->lastError().text()),
                         &m_themes);
        return;
    }
    const QSqlRecord record = m_model->record();
    const int primaryKey = record.indexOf(tableName + QStringLiteral("_id"));
    if (primaryKey >= 0)
        m_table->setColumnHidden(primaryKey, true);
}

void PersonalRecordManagementWidget::addRow()
{
    m_search->clear();
    const int row = m_model->rowCount();
    if (!m_model->insertRow(row))
    {
        Toast::showError(window(), QStringLiteral("新增行失败: %1").arg(m_model->lastError().text()),
                         &m_themes);
        return;
    }
    const QModelIndex index = m_filterModel->mapFromSource(m_model->index(row, 0));
    m_table->selectRow(index.row());
    m_table->scrollTo(index, QAbstractItemView::PositionAtCenter);
}

void PersonalRecordManagementWidget::deleteSelectedRows()
{
    const QModelIndexList selected = m_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        Toast::showWarning(window(), QStringLiteral("请先选择要删除的行"), &m_themes);
        return;
    }
    QList<int> rows;
    for (const QModelIndex &index : selected)
        rows.append(m_filterModel->mapToSource(index).row());
    std::sort(rows.begin(), rows.end(), [](int left, int right) { return left > right; });
    for (const int row : rows)
        if (!m_model->removeRow(row))
        {
            Toast::showError(window(), QStringLiteral("删除行失败: %1").arg(m_model->lastError().text()),
                             &m_themes);
            return;
        }
}

void PersonalRecordManagementWidget::saveChanges()
{
    if (!m_model->submitAll())
    {
        Toast::showError(window(), QStringLiteral("保存失败: %1").arg(m_model->lastError().text()),
                         &m_themes);
        return;
    }
    refresh();
    Toast::showSuccess(window(), QStringLiteral("保存成功"), &m_themes);
}

void PersonalRecordManagementWidget::revertChanges()
{
    m_model->revertAll();
    Toast::showSuccess(window(), QStringLiteral("已撤销修改"), &m_themes);
}

void PersonalRecordManagementWidget::exportCsv()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存为 CSV 文件"), {},
                                                QStringLiteral("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");
    QString errorMessage;
    if (!exportModelToCsv(m_filterModel, path, &errorMessage))
    {
        Toast::showError(window(), QStringLiteral("导出失败: %1").arg(errorMessage), &m_themes);
        return;
    }
    Toast::showSuccess(window(), QStringLiteral("已导出 CSV 文件"), &m_themes);
}

} // namespace darkeye
