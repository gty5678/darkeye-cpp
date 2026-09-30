#include "ui/components/PersonTransferSelector.h"

#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/TokenViews.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QSet>
#include <QVBoxLayout>
#include <algorithm>

namespace darkeye
{

PersonTransferSelector::PersonTransferSelector(QString personLabel, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *searchRow = new QHBoxLayout;
    m_search = new DesignLineEdit(this);
    m_search->setObjectName(QStringLiteral("PersonSelectorSearch"));
    m_search->setPlaceholderText(QStringLiteral("搜索中文名或日文名"));
    auto *add = new IconButton(QStringLiteral("circle_plus"), nullptr, this);
    add->setObjectName(QStringLiteral("PersonSelectorAddButton"));
    add->setToolTip(QStringLiteral("添加%1并选择").arg(personLabel));
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(add);
    layout->addLayout(searchRow);

    m_available = new TokenListWidget(this);
    m_available->setObjectName(QStringLiteral("PersonSelectorAvailableList"));
    layout->addWidget(m_available, 1);
    auto *actions = new QHBoxLayout;
    auto *choose = new IconButton(QStringLiteral("arrow_down"), nullptr, this);
    choose->setObjectName(QStringLiteral("PersonSelectorChooseButton"));
    choose->setToolTip(QStringLiteral("选择参演%1").arg(personLabel));
    auto *label = new DesignLabel(QStringLiteral("参演%1").arg(personLabel), this);
    label->setAlignment(Qt::AlignCenter);
    auto *remove = new IconButton(QStringLiteral("arrow_up"), nullptr, this);
    remove->setObjectName(QStringLiteral("PersonSelectorRemoveButton"));
    remove->setToolTip(QStringLiteral("移除参演%1").arg(personLabel));
    actions->addWidget(choose);
    actions->addWidget(label, 1);
    actions->addWidget(remove);
    layout->addLayout(actions);
    m_selected = new TokenListWidget(this);
    m_selected->setObjectName(QStringLiteral("PersonSelectorSelectedList"));
    layout->addWidget(m_selected, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildAvailable(); });
    connect(choose, &QPushButton::clicked, this, &PersonTransferSelector::chooseCurrent);
    connect(remove, &QPushButton::clicked, this, &PersonTransferSelector::removeCurrent);
    connect(add, &QPushButton::clicked, this, &PersonTransferSelector::addRequested);
    connect(m_available, &QListWidget::itemSelectionChanged, this, [this] {
        if (!m_available->selectedItems().isEmpty()) m_selected->clearSelection();
    });
    connect(m_selected, &QListWidget::itemSelectionChanged, this, [this] {
        if (!m_selected->selectedItems().isEmpty()) m_available->clearSelection();
    });
}

void PersonTransferSelector::setOptions(const QList<IdLabelOption> &options)
{
    m_options = options;
    QSet<qint64> available;
    for (const auto &option : m_options) available.insert(option.id);
    for (auto it = m_selectedIds.begin(); it != m_selectedIds.end();) {
        if (!available.contains(*it)) it = m_selectedIds.erase(it); else ++it;
    }
    rebuildAvailable();
    rebuildSelected();
}

QList<qint64> PersonTransferSelector::selectedIds() const { return m_selectedIds; }

void PersonTransferSelector::setSelectedIds(const QList<qint64> &ids)
{
    QSet<qint64> available;
    for (const auto &option : m_options) available.insert(option.id);
    QList<qint64> selected;
    for (qint64 id : ids) if (available.contains(id) && !selected.contains(id)) selected.append(id);
    m_selectedIds = selected;
    rebuildAvailable();
    rebuildSelected();
}

void PersonTransferSelector::selectId(qint64 id)
{
    if (id <= 0 || m_selectedIds.contains(id)) return;
    const auto found = std::find_if(m_options.cbegin(), m_options.cend(),
                                   [id](const IdLabelOption &item) { return item.id == id; });
    if (found == m_options.cend()) return;
    m_selectedIds.append(id);
    rebuildAvailable();
    rebuildSelected();
    emit selectionChanged();
}

void PersonTransferSelector::clearSelection()
{
    if (m_selectedIds.isEmpty()) return;
    m_selectedIds.clear();
    rebuildAvailable();
    rebuildSelected();
    emit selectionChanged();
}

void PersonTransferSelector::rebuildAvailable()
{
    const QString query = m_search->text().trimmed();
    m_available->clear();
    for (const auto &option : m_options) {
        if (m_selectedIds.contains(option.id) ||
            (!query.isEmpty() && !optionText(option).contains(query, Qt::CaseInsensitive))) continue;
        auto *item = new QListWidgetItem(optionText(option), m_available);
        item->setData(Qt::UserRole, option.id);
    }
}

void PersonTransferSelector::rebuildSelected()
{
    m_selected->clear();
    for (qint64 id : m_selectedIds) {
        const auto found = std::find_if(m_options.cbegin(), m_options.cend(),
                                        [id](const IdLabelOption &item) { return item.id == id; });
        if (found == m_options.cend()) continue;
        auto *item = new QListWidgetItem(optionText(*found), m_selected);
        item->setData(Qt::UserRole, id);
    }
}

void PersonTransferSelector::chooseCurrent()
{
    const auto items = m_available->selectedItems();
    if (items.isEmpty()) return;
    selectId(items.constFirst()->data(Qt::UserRole).toLongLong());
}

void PersonTransferSelector::removeCurrent()
{
    const auto items = m_selected->selectedItems();
    if (items.isEmpty()) return;
    m_selectedIds.removeAll(items.constFirst()->data(Qt::UserRole).toLongLong());
    rebuildAvailable();
    rebuildSelected();
    emit selectionChanged();
}

QString PersonTransferSelector::optionText(const IdLabelOption &option) const
{
    return option.group.isEmpty() ? option.label
                                  : QStringLiteral("%1（%2）").arg(option.label, option.group);
}

} // namespace darkeye
