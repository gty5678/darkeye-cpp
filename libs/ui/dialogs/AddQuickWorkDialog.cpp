#include "ui/dialogs/AddQuickWorkDialog.h"

#include "crawler/CrawlerScheduler.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/TokenViews.h"
#include "darkeye_ui/theme/IconProvider.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "ui/components/CrawlerFieldSelector.h"

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace darkeye
{

namespace
{

// Mirrors MessageBoxService._messagebox_qss_from_tokens in the Python app.
void showThemedInformation(QWidget *parent, ThemeService &themes, const QString &title,
                           const QString &message)
{
    const ThemeTokens tokens = themes.currentTokens();
    QMessageBox box(QMessageBox::Information, title, message, QMessageBox::Ok, parent);
    box.setStyleSheet(QStringLiteral(R"(
QMessageBox { background-color: %1; color: %2; font-family: "%3"; font-size: %4; }
QMessageBox QLabel { color: %2; }
QMessageBox QPushButton {
    background-color: %5; color: %6; border: none; border-radius: 4px;
    padding: 6px 16px; min-width: 70px;
}
QMessageBox QPushButton:hover { background-color: %7; }
QMessageBox QPushButton:disabled { background-color: %8; color: %2; }
)")
                          .arg(tokens.background, tokens.text, tokens.fontFamilyBase,
                               tokens.fontSizeBase, tokens.primary, tokens.textInverse,
                               tokens.primaryHover, tokens.textDisabled));
    box.exec();
}

} // namespace

AddQuickWorkDialog::AddQuickWorkDialog(CrawlerScheduler &crawlerScheduler, ThemeService &themes,
                                       QWidget *parent)
    : QDialog(parent), m_crawlerScheduler(crawlerScheduler), m_themes(themes)
{
    setObjectName(QStringLiteral("AddQuickWorkDialog"));
    setWindowTitle(QStringLiteral("快速记录作品番号(W)"));
    setWindowIcon(IconProvider::builtIn(QStringLiteral("film")));
    setMinimumSize(780, 520);
    resize(800, 540);

    auto *add = new DesignButton(QStringLiteral("添加"), this);
    auto *remove = new DesignButton(QStringLiteral("删除"), this);
    auto *removeAll = new DesignButton(QStringLiteral("删除全部"), this);
    auto *clean = new DesignButton(QStringLiteral("去后缀"), this);
    auto *cleanPrefix = new DesignButton(QStringLiteral("删前缀行"), this);
    auto *sort = new DesignButton(QStringLiteral("排序"), this);
    auto *csv = new DesignButton(QStringLiteral("从 CSV 导入"), this);
    auto *toolbar = new QHBoxLayout;
    for (QWidget *button : {static_cast<QWidget *>(add), static_cast<QWidget *>(remove),
                            static_cast<QWidget *>(removeAll), static_cast<QWidget *>(clean),
                            static_cast<QWidget *>(cleanPrefix), static_cast<QWidget *>(sort),
                            static_cast<QWidget *>(csv)})
        toolbar->addWidget(button);
    toolbar->addStretch();

    m_table = new TokenTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("选择"), QStringLiteral("番号")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto *commit = new DesignButton(QStringLiteral("快速添加"), this);
    commit->setMinimumHeight(40);
    commit->setToolTip(QStringLiteral("对左侧勾选番号全量爬取并写入库。"));

    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addLayout(toolbar);
    leftLayout->addWidget(m_table);
    leftLayout->addWidget(commit);

    m_fields = new CrawlerFieldSelector(this);
    auto *filter = new DesignButton(QStringLiteral("按空字段筛选"), this);
    filter->setToolTip(QStringLiteral("启用选择性补充：仅更新右侧勾选字段。"));
    auto *hint = new QLabel(QStringLiteral("未启用筛选时提交：对左侧番号全量爬取。\n"
                                           "勾选字段后点击筛选：提交时仅补充所选字段。"), this);
    hint->setWordWrap(true);
    auto *dropHint = new QLabel(QStringLiteral("可在表格中直接粘贴或编辑多个番号。"), this);
    dropHint->setWordWrap(true);
    auto *right = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(8);
    rightLayout->addWidget(hint);
    rightLayout->addWidget(filter);
    rightLayout->addWidget(m_fields);
    rightLayout->addStretch();
    rightLayout->addWidget(dropHint);

    auto *layout = new QHBoxLayout(this);
    layout->addWidget(left, 3);
    layout->addWidget(right, 2);

    connect(add, &QPushButton::clicked, this, [this] { addRow(); });
    connect(remove, &QPushButton::clicked, this, &AddQuickWorkDialog::deleteSelectedRows);
    connect(removeAll, &QPushButton::clicked, m_table, [this] { m_table->setRowCount(0); });
    connect(clean, &QPushButton::clicked, this, &AddQuickWorkDialog::cleanSuffixes);
    connect(cleanPrefix, &QPushButton::clicked, this, &AddQuickWorkDialog::deletePrefixRows);
    connect(sort, &QPushButton::clicked, this, &AddQuickWorkDialog::sortRows);
    connect(csv, &QPushButton::clicked, this, &AddQuickWorkDialog::importCsv);
    connect(filter, &QPushButton::clicked, this, [this, commit] {
        if (m_fields->selectedFields().isEmpty())
        {
            QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先勾选至少一个字段"));
            return;
        }
        m_selectiveCrawl = true;
        commit->setText(QStringLiteral("选择性补充信息"));
        commit->setToolTip(QStringLiteral("仅爬取右侧勾选字段。"));
    });
    connect(commit, &QPushButton::clicked, this, &AddQuickWorkDialog::submit);
    addRow();
}

void AddQuickWorkDialog::addRow(const QString &serial)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *checked = new QTableWidgetItem;
    checked->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    checked->setCheckState(Qt::Checked);
    m_table->setItem(row, 0, checked);
    auto *text = new QTableWidgetItem(serial);
    m_table->setItem(row, 1, text);
    if (serial.isEmpty())
    {
        m_table->setCurrentItem(text);
        m_table->editItem(text);
    }
}

QStringList AddQuickWorkDialog::checkedSerials() const
{
    QStringList serials;
    QSet<QString> seen;
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        const auto *checked = m_table->item(row, 0);
        const auto *text = m_table->item(row, 1);
        const QString serial = text == nullptr ? QString() : text->text().trimmed().toUpper();
        if (checked != nullptr && checked->checkState() == Qt::Checked && !serial.isEmpty() && !seen.contains(serial))
        {
            seen.insert(serial);
            serials.append(serial);
        }
    }
    return serials;
}

void AddQuickWorkDialog::deleteSelectedRows()
{
    QList<int> rows;
    for (const QTableWidgetSelectionRange &range : m_table->selectedRanges())
        for (int row = range.topRow(); row <= range.bottomRow(); ++row) rows.append(row);
    std::sort(rows.begin(), rows.end(), std::greater<>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int row : rows) m_table->removeRow(row);
}

void AddQuickWorkDialog::cleanSuffixes()
{
    const QRegularExpression suffix(QStringLiteral("(-C|-h|_uncensored|ch|pl)$"),
                                    QRegularExpression::CaseInsensitiveOption);
    for (int row = 0; row < m_table->rowCount(); ++row)
        if (auto *checked = m_table->item(row, 0); checked != nullptr && checked->checkState() == Qt::Checked)
            if (auto *text = m_table->item(row, 1); text != nullptr) text->setText(text->text().trimmed().remove(suffix));
}

void AddQuickWorkDialog::deletePrefixRows()
{
    bool accepted = false;
    const QString prefix = QInputDialog::getText(this, QStringLiteral("删前缀行"),
        QStringLiteral("输入前缀（删除已勾选且以此前缀开头的番号行）："), QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || prefix.isEmpty()) return;
    for (int row = m_table->rowCount() - 1; row >= 0; --row)
    {
        const auto *checked = m_table->item(row, 0);
        const auto *text = m_table->item(row, 1);
        if (checked != nullptr && text != nullptr && checked->checkState() == Qt::Checked && text->text().startsWith(prefix, Qt::CaseInsensitive))
            m_table->removeRow(row);
    }
}

void AddQuickWorkDialog::sortRows()
{
    struct Row { Qt::CheckState checked; QString serial; };
    QList<Row> rows;
    for (int row = 0; row < m_table->rowCount(); ++row)
        rows.append({m_table->item(row, 0)->checkState(), m_table->item(row, 1)->text()});
    std::sort(rows.begin(), rows.end(), [this](const Row &a, const Row &b) {
        return m_sortAscending ? a.serial.compare(b.serial, Qt::CaseInsensitive) < 0 : a.serial.compare(b.serial, Qt::CaseInsensitive) > 0;
    });
    m_sortAscending = !m_sortAscending;
    m_table->setRowCount(0);
    for (const Row &row : rows) { addRow(row.serial); m_table->item(m_table->rowCount() - 1, 0)->setCheckState(row.checked); }
}

void AddQuickWorkDialog::importCsv()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 CSV 文件"), {}, QStringLiteral("CSV (*.csv);;All Files (*.*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { QMessageBox::warning(this, QStringLiteral("导入失败"), file.errorString()); return; }
    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        const QString serial = stream.readLine().section(',', 0, 0).trimmed();
        if (!serial.isEmpty()) addRow(serial);
    }
}

void AddQuickWorkDialog::submit()
{
    const QStringList serials = checkedSerials();
    if (serials.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("没有选中任何有效的番号")); return; }
    const QSet<QString> fields = m_selectiveCrawl ? m_fields->selectedFields() : QSet<QString>{};
    if (m_selectiveCrawl && fields.isEmpty()) { QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先勾选至少一个要爬取的字段")); return; }
    m_crawlerScheduler.enqueue(serials, false, fields);
    showThemedInformation(this, m_themes, QStringLiteral("提示"),
                          QStringLiteral("已转入后台处理，您可以继续其他操作。"));
    accept();
}

} // namespace darkeye
