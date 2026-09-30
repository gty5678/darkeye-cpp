#include "ui/pages/management/LabelManagementWidget.h"

#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenViews.h"
#include "darkeye_ui/theme/ThemeService.h"

#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace darkeye
{

namespace
{

class ReferenceRedirectDialog final : public QDialog
{
public:
    ReferenceRedirectDialog(ReferenceKind kind, const QList<ReferenceRecord> &records,
                            QWidget *parent)
        : QDialog(parent)
    {
        const QString kindName = kind == ReferenceKind::Series ? QStringLiteral("系列")
                                                               : QStringLiteral("厂牌");
        setWindowTitle(QStringLiteral("重定向%1").arg(kindName));
        setModal(true);
        setMinimumWidth(620);

        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        m_source = new DesignComboBox(this);
        m_target = new DesignComboBox(this);
        m_source->addItem(QStringLiteral("请选择被重定向%1").arg(kindName), QVariant());
        m_target->addItem(QStringLiteral("请选择保留%1").arg(kindName), QVariant());
        for (const ReferenceRecord &record : records)
        {
            const QString name = record.chineseName.trimmed().isEmpty()
                                     ? record.japaneseName.trimmed()
                                     : record.chineseName.trimmed();
            const QString displayName = QStringLiteral("%1 (#%2)").arg(name).arg(record.id);
            m_source->addItem(displayName, record.id);
            m_target->addItem(displayName, record.id);
        }
        form->addRow(QStringLiteral("被重定向%1").arg(kindName), m_source);
        form->addRow(QStringLiteral("保留%1").arg(kindName), m_target);
        layout->addLayout(form);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *confirmButton = new DesignButton(QStringLiteral("确定"), this);
        confirmButton->setVariant(QStringLiteral("primary"));
        auto *cancelButton = new DesignButton(QStringLiteral("取消"), this);
        buttons->addWidget(confirmButton);
        buttons->addWidget(cancelButton);
        layout->addLayout(buttons);

        connect(confirmButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    }

    [[nodiscard]] qint64 sourceId() const { return m_source->currentData().toLongLong(); }
    [[nodiscard]] qint64 targetId() const { return m_target->currentData().toLongLong(); }

private:
    QComboBox *m_source = nullptr;
    QComboBox *m_target = nullptr;
};

} // namespace

LabelManagementWidget::LabelManagementWidget(ReferenceKind kind, QSqlDatabase database, ThemeService &themes,
                                             QWidget *parent)
    : QWidget(parent), m_database(std::move(database)), m_kind(kind), m_repository(m_database),
      m_jsonService(m_database), m_themes(themes)
{
    const bool series = m_kind == ReferenceKind::Series;
    const QString kindName = series ? QStringLiteral("系列") : QStringLiteral("厂牌");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    m_table = new TokenTableWidget(0, series ? 6 : 5, this);
    m_table->setHorizontalHeaderLabels(
        series ? QStringList{QStringLiteral("中文名"), QStringLiteral("日文名"),
                             QStringLiteral("别名"), QStringLiteral("详情"),
                             QStringLiteral("相关系列"), QStringLiteral("ID")}
               : QStringList{QStringLiteral("中文名"), QStringLiteral("日文名"),
                             QStringLiteral("别名"), QStringLiteral("详情"), QStringLiteral("ID")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_table->setColumnWidth(0, 180);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->setColumnWidth(1, 180);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->setColumnWidth(2, 220);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    if (series)
    {
        m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
        m_table->setColumnWidth(4, 160);
        m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    }
    else
        m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    root->addWidget(m_table, 1);

    m_search = new DesignLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索%1").arg(kindName));
    root->addWidget(m_search);

    auto *buttons = new QHBoxLayout;
    const auto iconButton = [this](const QString &icon, const QString &toolTip) {
        auto *button = new IconButton(icon, &m_themes, this);
        button->setToolTip(toolTip);
        return button;
    };
    auto *newButton = iconButton(QStringLiteral("list_plus"), QStringLiteral("新增行"));
    auto *deleteButton = iconButton(QStringLiteral("list_x"), QStringLiteral("删除行"));
    auto *saveButton = iconButton(QStringLiteral("save"), QStringLiteral("保存修改"));
    auto *revertButton = iconButton(QStringLiteral("eraser"), QStringLiteral("撤销修改"));
    auto *refreshButton = iconButton(QStringLiteral("refresh"), QStringLiteral("读数据库数据"));
    auto *exportButton = iconButton(QStringLiteral("arrow_up_to_line"),
                                    QStringLiteral("导出%1到 JSON 文件").arg(kindName));
    auto *importButton = iconButton(QStringLiteral("arrow_down_to_line"),
                                    QStringLiteral("从 JSON 文件导入%1").arg(kindName));
    auto *redirectButton = iconButton(QStringLiteral("share_2"),
                                      QStringLiteral("重定向%1").arg(kindName));
    for (QPushButton *button : {newButton, deleteButton, saveButton, revertButton, refreshButton,
                                exportButton, importButton})
        buttons->addWidget(button);
    buttons->addStretch();
    buttons->addWidget(redirectButton);
    root->addLayout(buttons);

    auto *form = new QFormLayout;
    m_chineseName = new DesignLineEdit(this);
    m_japaneseName = new DesignLineEdit(this);
    m_aliases = new DesignLineEdit(this);
    m_aliases->setPlaceholderText(QStringLiteral("多个别名使用英文逗号分隔"));
    m_detail = new DesignPlainTextEdit(this);
    m_detail->setMaximumHeight(80);
    form->addRow(new DesignLabel(QStringLiteral("中文名"), this), m_chineseName);
    form->addRow(new DesignLabel(QStringLiteral("日文名"), this), m_japaneseName);
    form->addRow(new DesignLabel(QStringLiteral("别名"), this), m_aliases);
    form->addRow(new DesignLabel(QStringLiteral("详情"), this), m_detail);
    root->addLayout(form);

    connect(m_table, &QTableWidget::cellClicked, this,
            [this](int row, int) { selectRow(row); });
    connect(m_search, &QLineEdit::textChanged, this, &LabelManagementWidget::applyFilter);
    connect(newButton, &QPushButton::clicked, this, &LabelManagementWidget::beginNew);
    connect(deleteButton, &QPushButton::clicked, this, &LabelManagementWidget::removeCurrent);
    connect(saveButton, &QPushButton::clicked, this, &LabelManagementWidget::saveCurrent);
    connect(revertButton, &QPushButton::clicked, this, &LabelManagementWidget::refresh);
    connect(refreshButton, &QPushButton::clicked, this, &LabelManagementWidget::refresh);
    connect(redirectButton, &QPushButton::clicked, this, &LabelManagementWidget::redirectCurrent);
    connect(exportButton, &QPushButton::clicked, this, &LabelManagementWidget::exportJson);
    connect(importButton, &QPushButton::clicked, this, &LabelManagementWidget::importJson);
    refresh();
}

void LabelManagementWidget::refresh()
{
    QString error;
    m_records = m_repository.list(m_kind, &error);
    if (!error.isEmpty())
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    m_table->setRowCount(m_records.size());
    for (qsizetype row = 0; row < m_records.size(); ++row)
    {
        const ReferenceRecord &record = m_records.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(record.chineseName));
        m_table->setItem(row, 1, new QTableWidgetItem(record.japaneseName));
        m_table->setItem(row, 2, new QTableWidgetItem(record.aliases));
        m_table->setItem(row, 3, new QTableWidgetItem(record.detail));
        if (m_kind == ReferenceKind::Series)
        {
            m_table->setItem(row, 4, new QTableWidgetItem(record.extra));
            m_table->setItem(row, 5, new QTableWidgetItem(QString::number(record.id)));
        }
        else
            m_table->setItem(row, 4, new QTableWidgetItem(QString::number(record.id)));
    }
    applyFilter(m_search->text());
    beginNew();
}

void LabelManagementWidget::applyFilter(const QString &text)
{
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        bool matches = text.trimmed().isEmpty();
        for (int column = 0; !matches && column < m_table->columnCount(); ++column)
        {
            const QTableWidgetItem *item = m_table->item(row, column);
            matches = item != nullptr && item->text().contains(text, Qt::CaseInsensitive);
        }
        m_table->setRowHidden(row, !matches);
    }
}

void LabelManagementWidget::selectRow(int row)
{
    if (row < 0 || row >= m_records.size()) return;
    const ReferenceRecord &record = m_records.at(row);
    m_currentId = record.id;
    m_chineseName->setText(record.chineseName);
    m_japaneseName->setText(record.japaneseName);
    m_aliases->setText(record.aliases);
    m_detail->setPlainText(record.detail);
}

void LabelManagementWidget::beginNew()
{
    m_currentId = 0;
    m_table->clearSelection();
    m_chineseName->clear();
    m_japaneseName->clear();
    m_aliases->clear();
    m_detail->clear();
}

void LabelManagementWidget::saveCurrent()
{
    ReferenceRecord record = editorRecord();
    QString error;
    if (record.id == 0)
    {
        const auto id = m_repository.create(record, &error);
        if (id.has_value()) m_currentId = *id;
    }
    else if (!m_repository.update(record, &error))
    {
    }
    if (!error.isEmpty())
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    refresh();
    emit referencesChanged(m_kind);
}

void LabelManagementWidget::removeCurrent()
{
    if (m_currentId <= 0)
    {
        Toast::showWarning(window(), QStringLiteral("请先选择要删除的行"), &m_themes);
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("确定删除当前厂牌？")) !=
        QMessageBox::Yes)
        return;
    QString error;
    if (!m_repository.remove(m_kind, m_currentId, &error))
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    refresh();
    emit referencesChanged(m_kind);
}

void LabelManagementWidget::redirectCurrent()
{
    ReferenceRedirectDialog dialog(m_kind, m_records, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const qint64 source = dialog.sourceId();
    const qint64 target = dialog.targetId();
    if (source <= 0 || target <= 0 || source == target)
    {
        Toast::showWarning(window(), QStringLiteral("请选择两个不同的%1").arg(
                               m_kind == ReferenceKind::Series ? QStringLiteral("系列") : QStringLiteral("厂牌")),
                           &m_themes);
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认重定向"),
                              QStringLiteral("作品关联将转移到保留的%1。").arg(
                                  m_kind == ReferenceKind::Series ? QStringLiteral("系列") : QStringLiteral("厂牌"))) != QMessageBox::Yes)
        return;
    QString error;
    if (!m_repository.redirect(m_kind, source, target, &error))
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    refresh();
    emit referencesChanged(m_kind);
}

void LabelManagementWidget::importJson()
{
    const QString kindName = m_kind == ReferenceKind::Series ? QStringLiteral("系列") : QStringLiteral("厂牌");
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择%1 JSON 文件").arg(kindName), {},
                                                       QStringLiteral("JSON Files (*.json)"));
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("确认导入"),
                              QStringLiteral("导入会覆盖当前%1数据。是否继续？").arg(kindName)) != QMessageBox::Yes)
        return;
    QString error;
    if (!m_jsonService.importFromFile(m_kind, path, &error))
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    refresh();
    emit referencesChanged(m_kind);
}

void LabelManagementWidget::exportJson()
{
    const QString kindName = m_kind == ReferenceKind::Series ? QStringLiteral("系列") : QStringLiteral("厂牌");
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("选择导出 JSON 文件"),
                                                m_kind == ReferenceKind::Series ? QStringLiteral("series.json") : QStringLiteral("label.json"),
                                                QStringLiteral("JSON Files (*.json)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) path += QStringLiteral(".json");
    QString error;
    if (!m_jsonService.exportToFile(m_kind, path, &error))
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    Toast::showSuccess(window(), QStringLiteral("%1已导出").arg(kindName), &m_themes);
}

ReferenceRecord LabelManagementWidget::editorRecord() const
{
    QString extra;
    const auto found = std::find_if(m_records.cbegin(), m_records.cend(), [this](const ReferenceRecord &record)
                                    { return record.id == m_currentId; });
    if (found != m_records.cend())
        extra = found->extra;
    return {m_kind, m_currentId, m_chineseName->text(), m_japaneseName->text(),
            m_aliases->text(), m_detail->toPlainText(), extra};
}

} // namespace darkeye
