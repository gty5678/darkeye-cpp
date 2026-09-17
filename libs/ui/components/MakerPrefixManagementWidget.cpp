#include "ui/components/MakerPrefixManagementWidget.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenViews.h"

#include <QComboBox>
#include <QFormLayout>
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

MakerPrefixManagementWidget::MakerPrefixManagementWidget(QSqlDatabase database,
                                                         ThemeService &themes, QWidget *parent)
    : QWidget(parent), m_repository(std::move(database)), m_themes(themes)
{
    setObjectName(QStringLiteral("MakerPrefixManagementWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    m_table = new TokenTableWidget(0, 4, this);
    m_table->setObjectName(QStringLiteral("MakerPrefixTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("番号前缀"),
                                        QStringLiteral("片商 ID"), QStringLiteral("片商")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);
    auto *form = new QFormLayout;
    m_prefix = new DesignLineEdit(this);
    m_prefix->setObjectName(QStringLiteral("MakerPrefixInput"));
    m_maker = new DesignComboBox(this);
    m_maker->setObjectName(QStringLiteral("MakerPrefixMakerSelector"));
    form->addRow(QStringLiteral("番号前缀"), m_prefix);
    form->addRow(QStringLiteral("对应片商"), m_maker);
    root->addLayout(form);
    auto *buttons = new QHBoxLayout;
    auto *newButton = new DesignButton(QStringLiteral("新建"), this);
    newButton->setObjectName(QStringLiteral("MakerPrefixNewButton"));
    auto *saveButton = new DesignButton(QStringLiteral("保存"), this);
    saveButton->setObjectName(QStringLiteral("MakerPrefixSaveButton"));
    saveButton->setVariant(QStringLiteral("primary"));
    auto *removeButton = new DesignButton(QStringLiteral("删除"), this);
    removeButton->setObjectName(QStringLiteral("MakerPrefixRemoveButton"));
    removeButton->setVariant(QStringLiteral("danger"));
    buttons->addWidget(newButton);
    buttons->addWidget(saveButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    root->addLayout(buttons);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) { selectRow(row); });
    connect(newButton, &QPushButton::clicked, this, &MakerPrefixManagementWidget::beginNewPrefix);
    connect(saveButton, &QPushButton::clicked, this, &MakerPrefixManagementWidget::saveCurrent);
    connect(removeButton, &QPushButton::clicked, this, &MakerPrefixManagementWidget::removeCurrent);
    refreshMakers();
    refresh();
}

void MakerPrefixManagementWidget::refresh()
{
    QString errorMessage;
    m_records = m_repository.listMakerPrefixes(&errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_table->setRowCount(m_records.size());
    for (qsizetype row = 0; row < m_records.size(); ++row)
    {
        const MakerPrefixRecord &record = m_records.at(row);
        const QStringList values{QString::number(record.id), record.prefix,
                                 QString::number(record.makerId), record.makerName};
        for (int column = 0; column < values.size(); ++column)
            m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
    if (m_currentId > 0)
    {
        for (qsizetype row = 0; row < m_records.size(); ++row)
        {
            if (m_records.at(row).id == m_currentId)
            {
                m_table->selectRow(row);
                selectRow(row);
                return;
            }
        }
    }
    beginNewPrefix();
}

void MakerPrefixManagementWidget::refreshMakers()
{
    const qint64 selectedId = m_maker->currentData().toLongLong();
    QString errorMessage;
    const QList<ReferenceRecord> makers = m_repository.list(ReferenceKind::Maker, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_maker->clear();
    m_maker->addItem(QStringLiteral("选择片商"), QVariant());
    for (const ReferenceRecord &maker : makers)
    {
        const QString name =
            maker.chineseName.trimmed().isEmpty() ? maker.japaneseName : maker.chineseName;
        m_maker->addItem(QStringLiteral("%1 (#%2)").arg(name).arg(maker.id), maker.id);
    }
    const int index = m_maker->findData(selectedId);
    m_maker->setCurrentIndex(index >= 0 ? index : 0);
}

void MakerPrefixManagementWidget::selectRow(int row)
{
    if (row < 0 || row >= m_records.size())
        return;
    const MakerPrefixRecord &record = m_records.at(row);
    m_currentId = record.id;
    m_prefix->setText(record.prefix);
    const int makerIndex = m_maker->findData(record.makerId);
    m_maker->setCurrentIndex(makerIndex >= 0 ? makerIndex : 0);
}

void MakerPrefixManagementWidget::beginNewPrefix()
{
    m_currentId = 0;
    m_table->clearSelection();
    m_prefix->clear();
    m_maker->setCurrentIndex(0);
    m_prefix->setFocus();
}

void MakerPrefixManagementWidget::saveCurrent()
{
    QString errorMessage;
    const qint64 makerId = m_maker->currentData().toLongLong();
    if (m_currentId == 0)
    {
        const auto id = m_repository.createMakerPrefix(m_prefix->text(), makerId, &errorMessage);
        if (!id.has_value())
        {
            Toast::showError(window(), errorMessage, &m_themes);
            return;
        }
        m_currentId = *id;
    }
    else if (!m_repository.updateMakerPrefix({m_currentId, m_prefix->text(), makerId, {}},
                                             &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    refresh();
    emit prefixesChanged();
    Toast::showSuccess(window(), QStringLiteral("番号前缀已保存"), &m_themes);
}

void MakerPrefixManagementWidget::removeCurrent()
{
    if (m_currentId <= 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定删除当前番号前缀映射？")) != QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.removeMakerPrefix(m_currentId, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_currentId = 0;
    refresh();
    emit prefixesChanged();
}

} // namespace darkeye


