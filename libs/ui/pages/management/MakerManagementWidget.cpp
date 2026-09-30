#include "ui/pages/management/MakerManagementWidget.h"

#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenControls.h"
#include "darkeye_ui/components/TokenViews.h"
#include "darkeye_ui/theme/ThemeService.h"

#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
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

namespace darkeye
{

namespace
{

class MakerRedirectDialog final : public QDialog
{
public:
    MakerRedirectDialog(const QList<ReferenceRecord> &makers, qint64 selectedSourceId,
                        QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("重定向片商"));
        setModal(true);
        setMinimumWidth(620);

        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        m_source = new DesignComboBox(this);
        m_target = new DesignComboBox(this);
        m_source->addItem(QStringLiteral("请选择被重定向片商"), QVariant());
        m_target->addItem(QStringLiteral("请选择保留片商"), QVariant());
        for (const ReferenceRecord &maker : makers)
        {
            const QString name = maker.chineseName.trimmed().isEmpty()
                                     ? maker.japaneseName.trimmed()
                                     : maker.chineseName.trimmed();
            const QString displayName = QStringLiteral("%1 (#%2)").arg(name).arg(maker.id);
            m_source->addItem(displayName, maker.id);
            m_target->addItem(displayName, maker.id);
        }
        m_source->setCurrentIndex(qMax(0, m_source->findData(selectedSourceId)));
        form->addRow(QStringLiteral("被重定向片商"), m_source);
        form->addRow(QStringLiteral("保留片商"), m_target);
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

MakerManagementWidget::MakerManagementWidget(QSqlDatabase database, ThemeService &themes,
                                             QWidget *parent)
    : QWidget(parent), m_database(std::move(database)), m_repository(m_database),
      m_jsonService(m_database), m_themes(themes)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_prefixTable = new TokenTableWidget(0, 3, splitter);
    m_prefixTable->setHorizontalHeaderLabels(
        {QStringLiteral("番号前缀"), QStringLiteral("制作商"), QStringLiteral("ID")});
    m_makerTable = new TokenTableWidget(0, 4, splitter);
    m_makerTable->setHorizontalHeaderLabels(
        {QStringLiteral("中文名"), QStringLiteral("日文名"), QStringLiteral("别名"), QStringLiteral("ID")});
    for (QTableWidget *table : {m_prefixTable, m_makerTable})
    {
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }
    splitter->addWidget(m_prefixTable);
    splitter->addWidget(m_makerTable);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    auto *buttons = new QHBoxLayout;
    const auto iconButton = [this](const QString &icon, const QString &toolTip) {
        auto *button = new IconButton(icon, &m_themes, this);
        button->setToolTip(toolTip);
        return button;
    };
    auto *newButton = iconButton(QStringLiteral("list_plus"), QStringLiteral("新增行"));
    auto *removeButton = iconButton(QStringLiteral("list_x"), QStringLiteral("删除行"));
    auto *saveButton = iconButton(QStringLiteral("save"), QStringLiteral("保存修改"));
    auto *revertButton = iconButton(QStringLiteral("eraser"), QStringLiteral("撤销修改"));
    auto *refreshButton = iconButton(QStringLiteral("refresh"), QStringLiteral("读数据库数据"));
    auto *exportButton = iconButton(QStringLiteral("arrow_up_to_line"),
                                    QStringLiteral("导出片商前缀到 JSON 文件"));
    auto *importButton = iconButton(QStringLiteral("arrow_down_to_line"),
                                    QStringLiteral("从 JSON 文件导入片商前缀"));
    auto *redirectButton = iconButton(QStringLiteral("share_2"), QStringLiteral("重定向片商"));
    for (QPushButton *button : {newButton, removeButton, saveButton, revertButton, refreshButton,
                                exportButton, importButton})
        buttons->addWidget(button);
    buttons->addStretch();
    buttons->addWidget(redirectButton);
    root->addLayout(buttons);

    auto *forms = new QHBoxLayout;
    auto *prefixForm = new QFormLayout;
    m_prefix = new DesignLineEdit(this);
    m_prefixMaker = new DesignComboBox(this);
    prefixForm->addRow(new DesignLabel(QStringLiteral("番号前缀"), this), m_prefix);
    prefixForm->addRow(new DesignLabel(QStringLiteral("制作商"), this), m_prefixMaker);
    auto *makerForm = new QFormLayout;
    m_chineseName = new DesignLineEdit(this);
    m_japaneseName = new DesignLineEdit(this);
    m_aliases = new DesignLineEdit(this);
    m_detail = new DesignPlainTextEdit(this);
    m_detail->setMaximumHeight(80);
    makerForm->addRow(new DesignLabel(QStringLiteral("中文名"), this), m_chineseName);
    makerForm->addRow(new DesignLabel(QStringLiteral("日文名"), this), m_japaneseName);
    makerForm->addRow(new DesignLabel(QStringLiteral("别名"), this), m_aliases);
    makerForm->addRow(new DesignLabel(QStringLiteral("说明"), this), m_detail);
    forms->addLayout(prefixForm, 1);
    forms->addLayout(makerForm, 1);
    root->addLayout(forms);

    connect(m_prefixTable, &QTableWidget::cellClicked, this,
            [this](int row, int) { selectPrefix(row); });
    connect(m_makerTable, &QTableWidget::cellClicked, this,
            [this](int row, int) { selectMaker(row); });
    m_prefixTable->installEventFilter(this);
    m_makerTable->installEventFilter(this);
    connect(newButton, &QPushButton::clicked, this, &MakerManagementWidget::beginNew);
    connect(saveButton, &QPushButton::clicked, this, &MakerManagementWidget::saveCurrent);
    connect(removeButton, &QPushButton::clicked, this, &MakerManagementWidget::removeCurrent);
    connect(revertButton, &QPushButton::clicked, this, &MakerManagementWidget::refresh);
    connect(refreshButton, &QPushButton::clicked, this, &MakerManagementWidget::refresh);
    connect(redirectButton, &QPushButton::clicked, this, &MakerManagementWidget::redirectMaker);
    connect(exportButton, &QPushButton::clicked, this, &MakerManagementWidget::exportJson);
    connect(importButton, &QPushButton::clicked, this, &MakerManagementWidget::importJson);
    connect(&m_themes, &ThemeService::themeChanged, this,
            [this](ThemeId) { updateActiveTableIndicator(); });
    refresh();
}

void MakerManagementWidget::refresh()
{
    QString error;
    m_prefixes = m_repository.listMakerPrefixes(&error);
    if (error.isEmpty())
        m_makers = m_repository.list(ReferenceKind::Maker, &error);
    if (!error.isEmpty())
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    m_prefixTable->setRowCount(m_prefixes.size());
    for (qsizetype row = 0; row < m_prefixes.size(); ++row)
    {
        const MakerPrefixRecord &record = m_prefixes.at(row);
        m_prefixTable->setItem(row, 0, new QTableWidgetItem(record.prefix));
        m_prefixTable->setItem(row, 1, new QTableWidgetItem(record.makerName));
        m_prefixTable->setItem(row, 2, new QTableWidgetItem(QString::number(record.id)));
    }
    m_makerTable->setRowCount(m_makers.size());
    for (qsizetype row = 0; row < m_makers.size(); ++row)
    {
        const ReferenceRecord &record = m_makers.at(row);
        m_makerTable->setItem(row, 0, new QTableWidgetItem(record.chineseName));
        m_makerTable->setItem(row, 1, new QTableWidgetItem(record.japaneseName));
        m_makerTable->setItem(row, 2, new QTableWidgetItem(record.aliases));
        m_makerTable->setItem(row, 3, new QTableWidgetItem(QString::number(record.id)));
    }
    refreshMakerChoices();
    beginNew();
}

void MakerManagementWidget::refreshMakerChoices()
{
    const qint64 prefixMakerId = m_prefixMaker->currentData().toLongLong();
    for (QComboBox *combo : {m_prefixMaker})
    {
        combo->clear();
        combo->addItem(QStringLiteral("请选择制作商"), QVariant());
        for (const ReferenceRecord &maker : m_makers)
        {
            const QString name = maker.chineseName.trimmed().isEmpty() ? maker.japaneseName
                                                                        : maker.chineseName;
            combo->addItem(QStringLiteral("%1 (#%2)").arg(name).arg(maker.id), maker.id);
        }
    }
    m_prefixMaker->setCurrentIndex(qMax(0, m_prefixMaker->findData(prefixMakerId)));
}

void MakerManagementWidget::updateActiveTableIndicator()
{
    const auto applyBorder = [](QTableWidget *table, bool active) {
        table->setStyleSheet(active ? QStringLiteral("QTableWidget { border: 2px solid orange; }")
                                    : QString());
    };
    applyBorder(m_prefixTable, m_activeTable == ActiveTable::Prefixes);
    applyBorder(m_makerTable, m_activeTable == ActiveTable::Makers);
}

bool MakerManagementWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        if (watched == m_prefixTable)
        {
            m_activeTable = ActiveTable::Prefixes;
            updateActiveTableIndicator();
        }
        else if (watched == m_makerTable)
        {
            m_activeTable = ActiveTable::Makers;
            updateActiveTableIndicator();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MakerManagementWidget::selectPrefix(int row)
{
    if (row < 0 || row >= m_prefixes.size())
        return;
    m_activeTable = ActiveTable::Prefixes;
    updateActiveTableIndicator();
    const MakerPrefixRecord &record = m_prefixes.at(row);
    m_currentPrefixId = record.id;
    m_prefix->setText(record.prefix);
    m_prefixMaker->setCurrentIndex(qMax(0, m_prefixMaker->findData(record.makerId)));
}

void MakerManagementWidget::selectMaker(int row)
{
    if (row < 0 || row >= m_makers.size())
        return;
    m_activeTable = ActiveTable::Makers;
    updateActiveTableIndicator();
    const ReferenceRecord &record = m_makers.at(row);
    m_currentMakerId = record.id;
    m_chineseName->setText(record.chineseName);
    m_japaneseName->setText(record.japaneseName);
    m_aliases->setText(record.aliases);
    m_detail->setPlainText(record.detail);
}

void MakerManagementWidget::beginNew()
{
    updateActiveTableIndicator();
    if (m_activeTable == ActiveTable::Prefixes)
    {
        m_currentPrefixId = 0;
        m_prefixTable->clearSelection();
        m_prefix->clear();
        m_prefixMaker->setCurrentIndex(0);
        m_prefix->setFocus();
        return;
    }
    m_currentMakerId = 0;
    m_makerTable->clearSelection();
    m_chineseName->clear();
    m_japaneseName->clear();
    m_aliases->clear();
    m_detail->clear();
    m_chineseName->setFocus();
}

void MakerManagementWidget::saveCurrent()
{
    QString error;
    if (m_activeTable == ActiveTable::Prefixes)
    {
        const qint64 makerId = m_prefixMaker->currentData().toLongLong();
        if (m_currentPrefixId == 0)
        {
            const auto id = m_repository.createMakerPrefix(m_prefix->text(), makerId, &error);
            if (id.has_value()) m_currentPrefixId = *id;
        }
        else if (!m_repository.updateMakerPrefix(
                     {m_currentPrefixId, m_prefix->text(), makerId, {}}, &error))
        {
        }
        if (!error.isEmpty())
        {
            Toast::showError(window(), error, &m_themes);
            return;
        }
        refresh();
        emit prefixesChanged();
        return;
    }
    ReferenceRecord record = makerRecord();
    if (record.id == 0)
    {
        const auto id = m_repository.create(record, &error);
        if (id.has_value()) m_currentMakerId = *id;
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
    emit referencesChanged(ReferenceKind::Maker);
}

void MakerManagementWidget::removeCurrent()
{
    const bool prefixes = m_activeTable == ActiveTable::Prefixes;
    const qint64 id = prefixes ? m_currentPrefixId : m_currentMakerId;
    if (id <= 0)
    {
        Toast::showWarning(window(), QStringLiteral("请先选择要删除的行"), &m_themes);
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定删除当前行？")) != QMessageBox::Yes)
        return;
    QString error;
    const bool success = prefixes ? m_repository.removeMakerPrefix(id, &error)
                                  : m_repository.remove(ReferenceKind::Maker, id, &error);
    if (!success)
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    refresh();
    if (prefixes) emit prefixesChanged();
    else emit referencesChanged(ReferenceKind::Maker);
}

void MakerManagementWidget::redirectMaker()
{
    MakerRedirectDialog dialog(m_makers, m_currentMakerId, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const qint64 source = dialog.sourceId();
    const qint64 target = dialog.targetId();
    if (source <= 0 || target <= 0 || source == target)
    {
        Toast::showWarning(window(), QStringLiteral("请选择两个不同的制作商"), &m_themes);
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认重定向"),
                              QStringLiteral("作品和前缀映射将转移到保留的制作商。")) != QMessageBox::Yes)
        return;
    QString error;
    if (!m_repository.redirect(ReferenceKind::Maker, source, target, &error))
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    refresh();
    emit referencesChanged(ReferenceKind::Maker);
    emit prefixesChanged();
}

void MakerManagementWidget::importJson()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择片商前缀 JSON 文件"),
                                                       {}, QStringLiteral("JSON Files (*.json)"));
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("确认导入"),
                              QStringLiteral("导入会覆盖当前片商和前缀映射。是否继续？")) != QMessageBox::Yes)
        return;
    QString error;
    if (!m_jsonService.importFromFile(ReferenceKind::Maker, path, &error))
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    refresh();
    emit referencesChanged(ReferenceKind::Maker);
    emit prefixesChanged();
}

void MakerManagementWidget::exportJson()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("选择导出 JSON 文件"),
                                                QStringLiteral("maker_prefix.json"),
                                                QStringLiteral("JSON Files (*.json)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) path += QStringLiteral(".json");
    QString error;
    if (!m_jsonService.exportToFile(ReferenceKind::Maker, path, &error))
    {
        Toast::showError(window(), error, &m_themes);
        return;
    }
    Toast::showSuccess(window(), QStringLiteral("片商前缀已导出"), &m_themes);
}

ReferenceRecord MakerManagementWidget::makerRecord() const
{
    return {ReferenceKind::Maker, m_currentMakerId, m_chineseName->text(), m_japaneseName->text(),
            m_aliases->text(), m_detail->toPlainText(), {}};
}

} // namespace darkeye
