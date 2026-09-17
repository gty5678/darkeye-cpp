#include "ui/components/ReferenceManagementWidget.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "ui/components/JsonTransferBar.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenControls.h"
#include "darkeye_ui/components/TokenViews.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{

QString kindName(darkeye::ReferenceKind kind)
{
    switch (kind)
    {
    case darkeye::ReferenceKind::Maker:
        return QStringLiteral("片商");
    case darkeye::ReferenceKind::Label:
        return QStringLiteral("厂牌");
    case darkeye::ReferenceKind::Series:
        return QStringLiteral("系列");
    }
    return {};
}

QString extraName(darkeye::ReferenceKind kind)
{
    switch (kind)
    {
    case darkeye::ReferenceKind::Maker:
        return QStringLiteral("Logo 路径");
    case darkeye::ReferenceKind::Series:
        return QStringLiteral("相关系列 ID");
    case darkeye::ReferenceKind::Label:
        return {};
    }
    return {};
}

QString defaultJsonFileName(darkeye::ReferenceKind kind)
{
    switch (kind)
    {
    case darkeye::ReferenceKind::Maker:
        return QStringLiteral("maker_prefix.json");
    case darkeye::ReferenceKind::Label:
        return QStringLiteral("label.json");
    case darkeye::ReferenceKind::Series:
        return QStringLiteral("series.json");
    }
    return {};
}

} // namespace

namespace darkeye
{

ReferenceManagementWidget::ReferenceManagementWidget(ReferenceKind kind, QSqlDatabase database,
                                                     ThemeService &themes, QWidget *parent)
    : QWidget(parent), m_kind(kind), m_repository(database), m_jsonService(std::move(database)),
      m_themes(themes)
{
    setObjectName(QStringLiteral("ReferenceManagement_%1").arg(static_cast<int>(kind)));
    buildUi();
    refresh();
}

void ReferenceManagementWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);
    auto *transfer = new JsonTransferBar(defaultJsonFileName(m_kind), this);
    connect(transfer, &JsonTransferBar::importRequested, this,
            &ReferenceManagementWidget::importJson);
    connect(transfer, &JsonTransferBar::exportRequested, this,
            &ReferenceManagementWidget::exportJson);
    root->addWidget(transfer);
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_table = new TokenTableWidget(0, 6, splitter);
    m_table->setObjectName(QStringLiteral("ReferenceTable_%1").arg(static_cast<int>(m_kind)));
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("中文名"),
                                        QStringLiteral("日文名"), QStringLiteral("别名"),
                                        QStringLiteral("说明"), extraName(m_kind)});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *editor = new TokenGroupBox(QStringLiteral("%1资料").arg(kindName(m_kind)), splitter);
    auto *editorLayout = new QVBoxLayout(editor);
    auto *form = new QFormLayout;
    m_chineseName = new DesignLineEdit(editor);
    m_chineseName->setObjectName(QStringLiteral("ReferenceChineseName"));
    m_japaneseName = new DesignLineEdit(editor);
    m_japaneseName->setObjectName(QStringLiteral("ReferenceJapaneseName"));
    m_aliases = new DesignLineEdit(editor);
    m_aliases->setObjectName(QStringLiteral("ReferenceAliases"));
    m_aliases->setPlaceholderText(QStringLiteral("多个别名使用英文逗号分隔"));
    m_detail = new DesignPlainTextEdit(editor);
    m_detail->setObjectName(QStringLiteral("ReferenceDetail"));
    m_detail->setMinimumHeight(120);
    m_extra = new DesignLineEdit(editor);
    m_extra->setObjectName(QStringLiteral("ReferenceExtra"));
    form->addRow(QStringLiteral("中文名"), m_chineseName);
    form->addRow(QStringLiteral("日文名"), m_japaneseName);
    form->addRow(QStringLiteral("别名"), m_aliases);
    form->addRow(QStringLiteral("说明"), m_detail);
    if (m_kind != ReferenceKind::Label)
        form->addRow(extraName(m_kind), m_extra);
    else
        m_extra->hide();
    editorLayout->addLayout(form);

    auto *editorButtons = new QHBoxLayout;
    auto *newButton = new DesignButton(QStringLiteral("新建"), editor);
    newButton->setObjectName(QStringLiteral("ReferenceNewButton"));
    auto *saveButton = new DesignButton(QStringLiteral("保存"), editor);
    saveButton->setObjectName(QStringLiteral("ReferenceSaveButton"));
    saveButton->setVariant(QStringLiteral("primary"));
    auto *removeButton = new DesignButton(QStringLiteral("删除"), editor);
    removeButton->setObjectName(QStringLiteral("ReferenceRemoveButton"));
    removeButton->setVariant(QStringLiteral("danger"));
    editorButtons->addWidget(newButton);
    editorButtons->addWidget(saveButton);
    editorButtons->addWidget(removeButton);
    editorButtons->addStretch();
    editorLayout->addLayout(editorButtons);
    editorLayout->addStretch();

    splitter->addWidget(m_table);
    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    auto *redirectGroup = new TokenGroupBox(QStringLiteral("合并并重定向"), this);
    auto *redirectLayout = new QHBoxLayout(redirectGroup);
    m_redirectSource = new DesignComboBox(redirectGroup);
    m_redirectSource->setObjectName(QStringLiteral("ReferenceRedirectSource"));
    m_redirectTarget = new DesignComboBox(redirectGroup);
    m_redirectTarget->setObjectName(QStringLiteral("ReferenceRedirectTarget"));
    auto *redirectButton = new DesignButton(QStringLiteral("重定向到 →"), redirectGroup);
    redirectButton->setObjectName(QStringLiteral("ReferenceRedirectButton"));
    redirectLayout->addWidget(m_redirectSource, 1);
    redirectLayout->addWidget(redirectButton);
    redirectLayout->addWidget(m_redirectTarget, 1);
    root->addWidget(redirectGroup);

    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) { selectRow(row); });
    connect(newButton, &QPushButton::clicked, this, &ReferenceManagementWidget::beginNewRecord);
    connect(saveButton, &QPushButton::clicked, this, &ReferenceManagementWidget::saveCurrent);
    connect(removeButton, &QPushButton::clicked, this, &ReferenceManagementWidget::removeCurrent);
    connect(redirectButton, &QPushButton::clicked, this,
            &ReferenceManagementWidget::redirectCurrent);
}

void ReferenceManagementWidget::refresh()
{
    QString errorMessage;
    m_records = m_repository.list(m_kind, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_table->setRowCount(m_records.size());
    m_redirectSource->clear();
    m_redirectTarget->clear();
    m_redirectSource->addItem(QStringLiteral("选择被重定向条目"), QVariant());
    m_redirectTarget->addItem(QStringLiteral("选择保留条目"), QVariant());
    for (qsizetype row = 0; row < m_records.size(); ++row)
    {
        const ReferenceRecord &record = m_records.at(row);
        const QStringList values{QString::number(record.id),
                                 record.chineseName,
                                 record.japaneseName,
                                 record.aliases,
                                 record.detail,
                                 record.extra};
        for (int column = 0; column < values.size(); ++column)
            m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        const QString name = displayName(record);
        m_redirectSource->addItem(name, record.id);
        m_redirectTarget->addItem(name, record.id);
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
    beginNewRecord();
}

ReferenceKind ReferenceManagementWidget::kind() const noexcept
{
    return m_kind;
}

void ReferenceManagementWidget::selectRow(int row)
{
    if (row < 0 || row >= m_records.size())
        return;
    const ReferenceRecord &record = m_records.at(row);
    m_currentId = record.id;
    m_chineseName->setText(record.chineseName);
    m_japaneseName->setText(record.japaneseName);
    m_aliases->setText(record.aliases);
    m_detail->setPlainText(record.detail);
    m_extra->setText(record.extra);
}

void ReferenceManagementWidget::beginNewRecord()
{
    m_currentId = 0;
    m_table->clearSelection();
    m_chineseName->clear();
    m_japaneseName->clear();
    m_aliases->clear();
    m_detail->clear();
    m_extra->clear();
    m_chineseName->setFocus();
}

void ReferenceManagementWidget::saveCurrent()
{
    ReferenceRecord record = editorRecord();
    QString errorMessage;
    if (record.id == 0)
    {
        const auto id = m_repository.create(record, &errorMessage);
        if (!id.has_value())
        {
            Toast::showError(window(), errorMessage, &m_themes);
            return;
        }
        m_currentId = *id;
    }
    else if (!m_repository.update(record, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    refresh();
    emit referencesChanged(m_kind);
    Toast::showSuccess(window(), QStringLiteral("%1资料已保存").arg(kindName(m_kind)), &m_themes);
}

void ReferenceManagementWidget::removeCurrent()
{
    if (m_currentId <= 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定删除当前%1资料？已被作品引用时会拒绝删除。")
                                  .arg(kindName(m_kind))) != QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.remove(m_kind, m_currentId, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_currentId = 0;
    refresh();
    emit referencesChanged(m_kind);
}

void ReferenceManagementWidget::redirectCurrent()
{
    const qint64 sourceId = m_redirectSource->currentData().toLongLong();
    const qint64 targetId = m_redirectTarget->currentData().toLongLong();
    if (sourceId <= 0 || targetId <= 0 || sourceId == targetId)
    {
        Toast::showWarning(window(), QStringLiteral("请选择两个不同的有效条目"), &m_themes);
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("确认重定向"),
            QStringLiteral("被重定向条目将合并为别名并删除，所有作品关联会改到保留条目。")) !=
        QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.redirect(m_kind, sourceId, targetId, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_currentId = targetId;
    refresh();
    emit referencesChanged(m_kind);
    Toast::showSuccess(window(), QStringLiteral("重定向完成"), &m_themes);
}

void ReferenceManagementWidget::importJson(const QString &path)
{
    if (QMessageBox::question(
            this, QStringLiteral("确认导入"),
            QStringLiteral("导入会同步当前%1资料，并自动重映射已有作品关联。是否继续？")
                .arg(kindName(m_kind))) != QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_jsonService.importFromFile(m_kind, path, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_currentId = 0;
    refresh();
    emit referencesChanged(m_kind);
    Toast::showSuccess(window(), QStringLiteral("%1资料导入完成").arg(kindName(m_kind)), &m_themes);
}

void ReferenceManagementWidget::exportJson(const QString &path)
{
    QString errorMessage;
    if (!m_jsonService.exportToFile(m_kind, path, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    Toast::showSuccess(window(), QStringLiteral("%1资料已导出").arg(kindName(m_kind)), &m_themes);
}

ReferenceRecord ReferenceManagementWidget::editorRecord() const
{
    return {m_kind,
            m_currentId,
            m_chineseName->text(),
            m_japaneseName->text(),
            m_aliases->text(),
            m_detail->toPlainText(),
            m_extra->text()};
}

QString ReferenceManagementWidget::displayName(const ReferenceRecord &record) const
{
    QString name = record.chineseName.trimmed();
    if (name.isEmpty())
        name = record.japaneseName.trimmed();
    return QStringLiteral("%1 (#%2)").arg(name).arg(record.id);
}

} // namespace darkeye


