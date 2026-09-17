#include "ui/components/TagManagementWidget.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/ColorPicker.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenControls.h"
#include "darkeye_ui/components/TokenViews.h"
#include "darkeye_ui/components/VerticalText.h"

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

#include <algorithm>

namespace darkeye
{

TagManagementWidget::TagManagementWidget(QSqlDatabase database, ThemeService &themes,
                                         QWidget *parent)
    : QWidget(parent), m_repository(std::move(database)), m_themes(themes)
{
    setObjectName(QStringLiteral("TagManagementWidget"));
    buildUi();
    refreshTagTypes();
    refresh();
}

void TagManagementWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_table = new TokenTableWidget(0, 6, splitter);
    m_table->setObjectName(QStringLiteral("TagManagementTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("标签"),
                                        QStringLiteral("类型"), QStringLiteral("颜色"),
                                        QStringLiteral("说明"), QStringLiteral("别名")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *editor = new TokenGroupBox(QStringLiteral("标签详情"), splitter);
    auto *editorLayout = new QVBoxLayout(editor);
    auto *previewRow = new QHBoxLayout;
    m_preview = new TokenVLabel({}, &m_themes, editor);
    m_preview->setObjectName(QStringLiteral("TagLivePreview"));
    previewRow->addWidget(m_preview, 0, Qt::AlignHCenter);
    editorLayout->addLayout(previewRow);
    auto *form = new QFormLayout;
    m_name = new DesignLineEdit(editor);
    m_name->setObjectName(QStringLiteral("TagNameInput"));
    m_type = new DesignComboBox(editor);
    m_type->setObjectName(QStringLiteral("TagTypeSelector"));
    m_color = new ColorPicker(QColor(QStringLiteral("#cccccc")), true, ColorPicker::Shape::Circle,
                              editor);
    m_color->setObjectName(QStringLiteral("TagColorPicker"));
    m_color->setMinimumHeight(36);
    m_detail = new DesignPlainTextEdit(editor);
    m_detail->setObjectName(QStringLiteral("TagDetailInput"));
    m_detail->setMinimumHeight(100);
    m_aliases = new DesignLineEdit(editor);
    m_aliases->setObjectName(QStringLiteral("TagAliasesInput"));
    m_aliases->setPlaceholderText(QStringLiteral("多个别名使用英文逗号分隔"));
    form->addRow(QStringLiteral("标签名称"), m_name);
    form->addRow(QStringLiteral("标签类型"), m_type);
    form->addRow(QStringLiteral("颜色"), m_color);
    form->addRow(QStringLiteral("说明"), m_detail);
    form->addRow(QStringLiteral("别名"), m_aliases);
    editorLayout->addLayout(form);
    auto *buttons = new QHBoxLayout;
    auto *newButton = new DesignButton(QStringLiteral("新建"), editor);
    newButton->setObjectName(QStringLiteral("TagNewButton"));
    auto *saveButton = new DesignButton(QStringLiteral("保存"), editor);
    saveButton->setObjectName(QStringLiteral("TagSaveButton"));
    saveButton->setVariant(QStringLiteral("primary"));
    auto *removeButton = new DesignButton(QStringLiteral("删除"), editor);
    removeButton->setObjectName(QStringLiteral("TagRemoveButton"));
    removeButton->setVariant(QStringLiteral("danger"));
    buttons->addWidget(newButton);
    buttons->addWidget(saveButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    editorLayout->addLayout(buttons);
    editorLayout->addStretch();
    splitter->addWidget(m_table);
    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    auto *redirectGroup = new TokenGroupBox(QStringLiteral("标签重定向"), this);
    auto *redirectLayout = new QHBoxLayout(redirectGroup);
    m_redirectSource = new DesignComboBox(redirectGroup);
    m_redirectSource->setObjectName(QStringLiteral("TagRedirectSource"));
    m_redirectTarget = new DesignComboBox(redirectGroup);
    m_redirectTarget->setObjectName(QStringLiteral("TagRedirectTarget"));
    auto *redirectButton = new DesignButton(QStringLiteral("重定向到 →"), redirectGroup);
    redirectButton->setObjectName(QStringLiteral("TagRedirectButton"));
    redirectLayout->addWidget(m_redirectSource, 1);
    redirectLayout->addWidget(redirectButton);
    redirectLayout->addWidget(m_redirectTarget, 1);
    root->addWidget(redirectGroup);

    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) { selectRow(row); });
    connect(newButton, &QPushButton::clicked, this, &TagManagementWidget::beginNewTag);
    connect(saveButton, &QPushButton::clicked, this, &TagManagementWidget::saveCurrent);
    connect(removeButton, &QPushButton::clicked, this, &TagManagementWidget::removeCurrent);
    connect(redirectButton, &QPushButton::clicked, this, &TagManagementWidget::redirectCurrent);
    connect(m_name, &QLineEdit::textChanged, this, &TagManagementWidget::updatePreview);
    connect(m_color, &ColorPicker::colorChanged, this, &TagManagementWidget::updatePreview);
    connect(m_detail, &QPlainTextEdit::textChanged, this, &TagManagementWidget::updatePreview);
}

void TagManagementWidget::refresh()
{
    QString errorMessage;
    m_records = m_repository.listTags(&errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_table->setRowCount(m_records.size());
    m_redirectSource->clear();
    m_redirectTarget->clear();
    m_redirectSource->addItem(QStringLiteral("选择被重定向标签"), QVariant());
    m_redirectTarget->addItem(QStringLiteral("选择保留标签"), QVariant());
    for (qsizetype row = 0; row < m_records.size(); ++row)
    {
        const TagRecord &record = m_records.at(row);
        const QStringList values{
            QString::number(record.id), record.name, record.typeName, record.color, record.detail,
            record.aliases.join(',')};
        for (int column = 0; column < values.size(); ++column)
            m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        const QString display = QStringLiteral("%1 (#%2)").arg(record.name).arg(record.id);
        m_redirectSource->addItem(display, record.id);
        m_redirectTarget->addItem(display, record.id);
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
    beginNewTag();
}

void TagManagementWidget::refreshTagTypes()
{
    const std::optional<qint64> selected =
        m_type != nullptr && m_type->currentData().isValid()
            ? std::optional<qint64>(m_type->currentData().toLongLong())
            : std::nullopt;
    QString errorMessage;
    const QList<TagTypeRecord> types = m_repository.listTagTypes(&errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_type->clear();
    m_type->addItem(QStringLiteral("未分类"), QVariant());
    for (const TagTypeRecord &type : types)
        m_type->addItem(type.name, type.id);
    if (selected.has_value())
    {
        const int index = m_type->findData(*selected);
        m_type->setCurrentIndex(index >= 0 ? index : 0);
    }
}

void TagManagementWidget::selectRow(int row)
{
    if (row < 0 || row >= m_records.size())
        return;
    const TagRecord &record = m_records.at(row);
    m_currentId = record.id;
    m_name->setText(record.name);
    const int typeIndex = record.typeId.has_value() ? m_type->findData(*record.typeId) : 0;
    m_type->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    m_color->setColor(record.color);
    m_detail->setPlainText(record.detail);
    m_aliases->setText(record.aliases.join(','));
    updatePreview();
}

void TagManagementWidget::beginNewTag()
{
    m_currentId = 0;
    m_table->clearSelection();
    m_name->clear();
    m_type->setCurrentIndex(0);
    m_color->setColor(QStringLiteral("#cccccc"));
    m_detail->clear();
    m_aliases->clear();
    updatePreview();
    m_name->setFocus();
}

void TagManagementWidget::saveCurrent()
{
    TagRecord record = editorRecord();
    QString errorMessage;
    if (record.id == 0)
    {
        const auto id = m_repository.createTag(record, &errorMessage);
        if (!id.has_value())
        {
            Toast::showError(window(), errorMessage, &m_themes);
            return;
        }
        m_currentId = *id;
    }
    else if (!m_repository.updateTag(record, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    refresh();
    emit tagsChanged();
    Toast::showSuccess(window(), QStringLiteral("标签已保存"), &m_themes);
}

void TagManagementWidget::removeCurrent()
{
    if (m_currentId <= 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定删除当前标签及其别名？已被作品使用时会拒绝。")) !=
        QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.removeTag(m_currentId, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_currentId = 0;
    refresh();
    emit tagsChanged();
}

void TagManagementWidget::redirectCurrent()
{
    const qint64 sourceId = m_redirectSource->currentData().toLongLong();
    const qint64 targetId = m_redirectTarget->currentData().toLongLong();
    if (sourceId <= 0 || targetId <= 0 || sourceId == targetId)
    {
        Toast::showWarning(window(), QStringLiteral("请选择两个不同的有效标签"), &m_themes);
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("确认重定向"),
            QStringLiteral("来源标签及其别名会指向保留标签，作品关联会自动合并。")) !=
        QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.redirectTag(sourceId, targetId, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_currentId = targetId;
    refresh();
    emit tagsChanged();
    Toast::showSuccess(window(), QStringLiteral("标签重定向完成"), &m_themes);
}

void TagManagementWidget::updatePreview()
{
    const QColor background(m_color->color());
    const QColor text = background.lightnessF() < 0.55 ? Qt::white : Qt::black;
    m_preview->setTextDynamic(m_name->text().trimmed().isEmpty() ? QStringLiteral("标签预览")
                                                                 : m_name->text().trimmed());
    m_preview->setColors(background, text);
    m_preview->setToolTip(m_detail->toPlainText().trimmed());
}

TagRecord TagManagementWidget::editorRecord() const
{
    TagRecord record;
    for (const TagRecord &candidate : m_records)
    {
        if (candidate.id == m_currentId)
        {
            record = candidate;
            break;
        }
    }
    record.id = m_currentId;
    record.name = m_name->text();
    if (m_type->currentData().isValid())
        record.typeId = m_type->currentData().toLongLong();
    else
        record.typeId.reset();
    record.color = m_color->color();
    record.detail = m_detail->toPlainText();
    record.aliases = m_aliases->text().split(',', Qt::SkipEmptyParts);
    return record;
}

TagTypeManagementWidget::TagTypeManagementWidget(QSqlDatabase database, ThemeService &themes,
                                                 QWidget *parent)
    : QWidget(parent), m_repository(std::move(database)), m_themes(themes)
{
    setObjectName(QStringLiteral("TagTypeManagementWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    m_table = new TokenTableWidget(0, 3, this);
    m_table->setObjectName(QStringLiteral("TagTypeTable"));
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("ID"), QStringLiteral("标签类型"), QStringLiteral("顺序")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);
    auto *form = new QFormLayout;
    m_name = new DesignLineEdit(this);
    m_name->setObjectName(QStringLiteral("TagTypeNameInput"));
    form->addRow(QStringLiteral("类型名称"), m_name);
    root->addLayout(form);
    auto *buttons = new QHBoxLayout;
    auto *newButton = new DesignButton(QStringLiteral("新建"), this);
    newButton->setObjectName(QStringLiteral("TagTypeNewButton"));
    auto *saveButton = new DesignButton(QStringLiteral("保存"), this);
    saveButton->setObjectName(QStringLiteral("TagTypeSaveButton"));
    auto *removeButton = new DesignButton(QStringLiteral("删除"), this);
    removeButton->setObjectName(QStringLiteral("TagTypeRemoveButton"));
    auto *upButton = new DesignButton(QStringLiteral("上移"), this);
    upButton->setObjectName(QStringLiteral("TagTypeMoveUpButton"));
    auto *downButton = new DesignButton(QStringLiteral("下移"), this);
    downButton->setObjectName(QStringLiteral("TagTypeMoveDownButton"));
    for (QPushButton *button : {newButton, saveButton, removeButton, upButton, downButton})
        buttons->addWidget(button);
    buttons->addStretch();
    root->addLayout(buttons);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) { selectRow(row); });
    connect(newButton, &QPushButton::clicked, this, &TagTypeManagementWidget::beginNewType);
    connect(saveButton, &QPushButton::clicked, this, &TagTypeManagementWidget::saveCurrent);
    connect(removeButton, &QPushButton::clicked, this, &TagTypeManagementWidget::removeCurrent);
    connect(upButton, &QPushButton::clicked, this, [this] { moveCurrent(-1); });
    connect(downButton, &QPushButton::clicked, this, [this] { moveCurrent(1); });
    refresh();
}

void TagTypeManagementWidget::refresh()
{
    QString errorMessage;
    m_records = m_repository.listTagTypes(&errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_table->setRowCount(m_records.size());
    for (qsizetype row = 0; row < m_records.size(); ++row)
    {
        const TagTypeRecord &record = m_records.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(record.id)));
        m_table->setItem(row, 1, new QTableWidgetItem(record.name));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(record.order)));
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
    beginNewType();
}

void TagTypeManagementWidget::selectRow(int row)
{
    if (row < 0 || row >= m_records.size())
        return;
    m_currentId = m_records.at(row).id;
    m_name->setText(m_records.at(row).name);
}

void TagTypeManagementWidget::beginNewType()
{
    m_currentId = 0;
    m_table->clearSelection();
    m_name->clear();
    m_name->setFocus();
}

void TagTypeManagementWidget::saveCurrent()
{
    QString errorMessage;
    if (m_currentId == 0)
    {
        const auto id =
            m_repository.createTagType(m_name->text(), m_records.size() + 1, &errorMessage);
        if (!id.has_value())
        {
            Toast::showError(window(), errorMessage, &m_themes);
            return;
        }
        m_currentId = *id;
    }
    else
    {
        const auto found =
            std::find_if(m_records.cbegin(), m_records.cend(),
                         [this](const TagTypeRecord &record) { return record.id == m_currentId; });
        const int order = found == m_records.cend() ? m_records.size() + 1 : found->order;
        if (!m_repository.updateTagType({m_currentId, m_name->text(), order}, &errorMessage))
        {
            Toast::showError(window(), errorMessage, &m_themes);
            return;
        }
    }
    refresh();
    emit typesChanged();
    Toast::showSuccess(window(), QStringLiteral("标签类型已保存"), &m_themes);
}

void TagTypeManagementWidget::removeCurrent()
{
    if (m_currentId <= 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定删除当前标签类型？仍有标签使用时会拒绝。")) !=
        QMessageBox::Yes)
        return;
    QString errorMessage;
    if (!m_repository.removeTagType(m_currentId, &errorMessage))
    {
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    m_currentId = 0;
    refresh();
    emit typesChanged();
}

void TagTypeManagementWidget::moveCurrent(int offset)
{
    if (m_currentId <= 0)
        return;
    QString errorMessage;
    if (!m_repository.moveTagType(m_currentId, offset, &errorMessage))
    {
        Toast::showWarning(window(), errorMessage, &m_themes);
        return;
    }
    refresh();
    emit typesChanged();
}

} // namespace darkeye


