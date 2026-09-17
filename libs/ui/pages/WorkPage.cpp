#include "ui/pages/WorkPage.h"

#include "darkeye_ui/components/CompleterLineEdit.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/InteractionEffects.h"
#include "darkeye_ui/components/LazyScrollArea.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenControls.h"
#include "darkeye_ui/components/TokenViews.h"
#include "ui/components/WorkCard.h"
#include "ui/components/WorkEditorWidget.h"
#include "ui/components/WorkTagSelector.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace darkeye
{
WorkPage::WorkPage(QSqlDatabase database, ThemeService &themeService, Settings &settings,
                   QString coverDirectory, QSqlDatabase privateDatabase, QString fanartDirectory,
                   QUrl imageFetchEndpoint, QWidget *parent)
    : LazyWidget(parent), m_themeService(themeService), m_settings(settings), m_database(database),
      m_repository(std::move(database)), m_privateRepository(std::move(privateDatabase)),
      m_coverDirectory(std::move(coverDirectory)), m_fanartDirectory(std::move(fanartDirectory)),
      m_imageFetchEndpoint(std::move(imageFetchEndpoint))
{
    setObjectName(QStringLiteral("WorkPage"));
}

void WorkPage::lazyLoad()
{
    m_randomSeed = QRandomGenerator::global()->bounded(1, 1'000'001);
    m_randomSeed2 = QRandomGenerator::global()->bounded(1, 1'000'001);
    const AppSettings appSettings = m_settings.app();
    m_largeCoverView = appSettings.workLargeCoverView;
    m_tagPanelVisible = appSettings.workTagSelectorVisible;
    buildUi();
    refreshData();
    m_lazyArea->setLoader([this](int pageIndex, int pageSize)
                          { return loadCardPage(pageIndex, pageSize); });
}

void WorkPage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    // Python WorkPage keeps Qt's six-pixel spacing between the 44px filter bar
    // and the waterfall body. Preserve it so the first card baseline matches.
    root->setSpacing(6);

    auto *filterBar = new QWidget(this);

    filterBar->setFixedHeight(44);
    auto *filterBarLayout = new QHBoxLayout(filterBar);
    filterBarLayout->setContentsMargins(10, 0, 10, 0);
    filterBarLayout->setSpacing(6);

    m_tagPanelButton = new IconButton(m_tagPanelVisible ? QStringLiteral("panel_left_close")
                                                        : QStringLiteral("panel_left_open"),
                                      &m_themeService, filterBar);

    m_tagPanelButton->setToolTip(m_tagPanelVisible ? QStringLiteral("隐藏标签筛选栏")
                                                   : QStringLiteral("显示标签筛选栏"));
    filterBarLayout->addWidget(m_tagPanelButton);

    auto *filterScroll = new QScrollArea(filterBar);

    filterScroll->setWidgetResizable(true);
    filterScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    filterScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    filterScroll->setFixedHeight(44);
    filterScroll->setFrameShape(QFrame::NoFrame);
    auto *filterContent = new QWidget(filterScroll);
    auto *filters = new QHBoxLayout(filterContent);
    filters->setContentsMargins(0, 0, 0, 0);
    filters->setSpacing(6);
    filters->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    const auto addFilter =
        [filters, filterContent](const QString &label, DesignLineEdit *input, int width)
    {
        filters->addWidget(new DesignLabel(label, filterContent));
        input->setFixedWidth(width);
        filters->addWidget(input);
    };
    const QStringList serialSuggestions = m_repository.serialSuggestions();
    const QStringList actressSuggestions = m_repository.actressSuggestions();
    const QStringList directorSuggestions = m_repository.directorSuggestions();
    const QStringList actorSuggestions = m_repository.actorSuggestions();
    m_serialFilter =
        new CompleterLineEdit([serialSuggestions] { return serialSuggestions; }, filterContent);
    m_actressFilter =
        new CompleterLineEdit([actressSuggestions] { return actressSuggestions; }, filterContent);
    m_titleFilter = new DesignLineEdit(filterContent);

    m_searchInput = new DesignLineEdit(filterContent);

    m_storyFilter = new DesignLineEdit(filterContent);

    m_directorFilter =
        new CompleterLineEdit([directorSuggestions] { return directorSuggestions; }, filterContent);
    m_actorFilter =
        new CompleterLineEdit([actorSuggestions] { return actorSuggestions; }, filterContent);
    const auto addComboFilter =
        [filters, filterContent](const QString &label, QComboBox *combo, int width)
    {
        filters->addWidget(new DesignLabel(label, filterContent));
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setFixedWidth(width);
        filters->addWidget(combo);
    };
    addFilter(QStringLiteral("番号："), m_serialFilter, 120);
    addFilter(QStringLiteral("女优"), m_actressFilter, 120);
    addFilter(QStringLiteral("标题包含："), m_titleFilter, 100);
    addFilter(QStringLiteral("故事包含："), m_storyFilter, 100);
    addFilter(QStringLiteral("简单笔记包含："), m_searchInput, 100);
    addFilter(QStringLiteral("导演"), m_directorFilter, 150);
    addFilter(QStringLiteral("男优"), m_actorFilter, 120);
    m_makerFilter = new DesignComboBox(filterContent);

    m_labelFilter = new DesignComboBox(filterContent);

    m_seriesFilter = new DesignComboBox(filterContent);

    for (auto *combo : {m_makerFilter, m_labelFilter, m_seriesFilter})
    {
        combo->addItem(QString());
    }
    const auto addOptions = [](QComboBox *combo, const QList<NamedIdOption> &options)
    {
        for (const NamedIdOption &option : options) combo->addItem(option.name, option.id);
    };
    addOptions(m_makerFilter, m_repository.makerOptions());
    addOptions(m_labelFilter, m_repository.labelOptions());
    addOptions(m_seriesFilter, m_repository.seriesOptions());
    addComboFilter(QStringLiteral("片商"), m_makerFilter, 160);
    addComboFilter(QStringLiteral("厂牌"), m_labelFilter, 160);
    addComboFilter(QStringLiteral("系列"), m_seriesFilter, 160);
    filterScroll->setWidget(filterContent);
    filterBarLayout->addWidget(filterScroll, 1);

    auto *refreshButton = new RotateButton(QStringLiteral("refresh_cw"), &m_themeService,
                                           filterBar);

    refreshButton->setToolTip(QStringLiteral("刷新作品"));

    auto *clearButton = new ShakeButton(QStringLiteral("eraser"), &m_themeService, filterBar);
    clearButton->setToolTip(QStringLiteral("清空筛选"));

    m_countLabel = new DesignLabel({}, filterBar);

    m_countLabel->setFixedWidth(100);

    m_sortSelector = new DesignComboBox(filterBar);

    m_sortSelector->addItem(QStringLiteral("随机顺序"), static_cast<int>(WorkSortOrder::Random));
    m_sortSelector->addItem(QStringLiteral("添加逆序"),
                            static_cast<int>(WorkSortOrder::CreatedDescending));
    m_sortSelector->addItem(QStringLiteral("添加顺序"),
                            static_cast<int>(WorkSortOrder::CreatedAscending));
    m_sortSelector->addItem(QStringLiteral("番号顺序"),
                            static_cast<int>(WorkSortOrder::SerialAscending));
    m_sortSelector->addItem(QStringLiteral("番号逆序"),
                            static_cast<int>(WorkSortOrder::SerialDescending));
    m_sortSelector->addItem(QStringLiteral("制作商顺序"),
                            static_cast<int>(WorkSortOrder::MakerAscending));
    m_sortSelector->addItem(QStringLiteral("制作商逆序"),
                            static_cast<int>(WorkSortOrder::MakerDescending));
    m_sortSelector->addItem(QStringLiteral("更新时间逆序"),
                            static_cast<int>(WorkSortOrder::UpdatedDescending));
    m_sortSelector->addItem(QStringLiteral("更新时间顺序"),
                            static_cast<int>(WorkSortOrder::UpdatedAscending));
    m_sortSelector->addItem(QStringLiteral("发布时间逆序"),
                            static_cast<int>(WorkSortOrder::ReleaseDateDescending));
    m_sortSelector->addItem(QStringLiteral("发布时间顺序"),
                            static_cast<int>(WorkSortOrder::ReleaseDateAscending));
    m_sortSelector->addItem(QStringLiteral("拍摄年龄顺序"),
                            static_cast<int>(WorkSortOrder::ActressAgeAscending));
    m_sortSelector->addItem(QStringLiteral("拍摄年龄逆序"),
                            static_cast<int>(WorkSortOrder::ActressAgeDescending));
    m_sortSelector->setCurrentIndex(1);

    m_scopeSelector = new DesignComboBox(filterBar);

    m_scopeSelector->addItems({QStringLiteral("公共库范围"), QStringLiteral("收藏库范围"),
                               QStringLiteral("收藏未观看"), QStringLiteral("已撸过")});
    m_scopeSelector->setCurrentIndex(0);
    m_scopeSelector->setToolTip(QStringLiteral("选择公共库、收藏或观看记录范围"));
    m_viewButton = new IconButton(m_largeCoverView ? QStringLiteral("layout_grid")
                                                   : QStringLiteral("layout_waterfall"),
                                  &m_themeService, filterBar);

    m_viewButton->setToolTip(m_largeCoverView
                                 ? QStringLiteral("当前：大图卡片视图，点击切换为标准瀑布流")
                                 : QStringLiteral("当前：标准卡片视图，点击切换为大图卡片"));

    filterBarLayout->addWidget(refreshButton);
    filterBarLayout->addWidget(clearButton);
    filterBarLayout->addWidget(m_countLabel);
    filterBarLayout->addWidget(m_scopeSelector);
    filterBarLayout->addWidget(m_sortSelector);
    filterBarLayout->addWidget(m_viewButton);
    root->addWidget(filterBar);

    m_lazyArea = new LazyScrollArea(m_largeCoverView ? 250 : 220, this);
    m_lazyArea->setPageSize(70);
    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    m_tagPanel = new QWidget(this);

    auto *tagLayout = new QHBoxLayout(m_tagPanel);
    tagLayout->setContentsMargins(0, 0, 0, 0);
    m_tagSelector = new WorkTagSelector(m_repository.tagOptions(), &m_themeService, m_tagPanel);
    m_tagSelector->setLoader([this] { return m_repository.tagOptions(); });
    tagLayout->addWidget(m_tagSelector);
    m_tagPanel->setVisible(m_tagPanelVisible);
    body->addWidget(m_tagPanel);
    body->addWidget(m_lazyArea, 1);
    root->addLayout(body, 1);

    m_table = new TokenTableWidget(0, 4, this);

    m_table->hide();
    buildEditor();

    connect(refreshButton, &QPushButton::clicked, this,
            [this]
            {
                m_randomSeed = QRandomGenerator::global()->generate();
                m_randomSeed2 = QRandomGenerator::global()->generate();
                applyFilters();
            });
    connect(
        m_tagPanelButton, &QPushButton::clicked, this,
        [this]
        {
            m_tagPanelVisible = !m_tagPanel->isVisible();
            m_tagPanel->setVisible(m_tagPanelVisible);
            m_tagPanelButton->setIconName(m_tagPanelVisible ? QStringLiteral("panel_left_close")
                                                            : QStringLiteral("panel_left_open"));
            m_tagPanelButton->setToolTip(m_tagPanelVisible ? QStringLiteral("隐藏标签筛选栏")
                                                           : QStringLiteral("显示标签筛选栏"));
            AppSettings settings = m_settings.app();
            settings.workTagSelectorVisible = m_tagPanelVisible;
            m_settings.saveApp(settings);
        });
    connect(clearButton, &QPushButton::clicked, this, &WorkPage::clearFilters);

    connect(m_viewButton, &QPushButton::clicked, this, &WorkPage::toggleCoverSize);
    m_filterTimer = new QTimer(this);
    m_filterTimer->setSingleShot(true);
    m_filterTimer->setInterval(50);
    connect(m_filterTimer, &QTimer::timeout, this, &WorkPage::applyFilters);
    const QList<QLineEdit *> filterInputs{m_searchInput,
                                          m_serialFilter,
                                          m_titleFilter,
                                          m_storyFilter,
                                          m_directorFilter,
                                          m_actressFilter,
                                          m_actorFilter,
                                          m_makerFilter->lineEdit(),
                                          m_labelFilter->lineEdit(),
                                          m_seriesFilter->lineEdit()};
    for (QLineEdit *input : filterInputs)
    {
        connect(input, &QLineEdit::textChanged, m_filterTimer, qOverload<>(&QTimer::start));
    }
    connect(m_tagSelector, &WorkTagSelector::selectionChanged, m_filterTimer,
            qOverload<>(&QTimer::start));
    connect(m_sortSelector, &QComboBox::currentIndexChanged, this, [this](int) { applyFilters(); });
    connect(m_scopeSelector, &QComboBox::currentIndexChanged, this,
            [this](int) { applyFilters(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &WorkPage::loadSelectedWork);
}

void WorkPage::applyFilters()
{
    refresh();
}

void WorkPage::clearFilters()
{
    m_filterTimer->stop();
    const QList<QLineEdit *> filterInputs{m_searchInput,
                                          m_serialFilter,
                                          m_titleFilter,
                                          m_storyFilter,
                                          m_directorFilter,
                                          m_actressFilter,
                                          m_actorFilter,
                                          m_makerFilter->lineEdit(),
                                          m_labelFilter->lineEdit(),
                                          m_seriesFilter->lineEdit()};
    for (QLineEdit *input : filterInputs)
    {
        input->clear();
    }
    m_tagSelector->clearSelection();
    applyFilters();
}

void WorkPage::toggleCoverSize()
{
    m_largeCoverView = !m_largeCoverView;
    m_lazyArea->setColumnWidth(m_largeCoverView ? 250 : 220);
    m_viewButton->setIconName(m_largeCoverView ? QStringLiteral("layout_grid")
                                               : QStringLiteral("layout_waterfall"));
    m_viewButton->setToolTip(m_largeCoverView
                                 ? QStringLiteral("当前：大图卡片视图，点击切换为标准瀑布流")
                                 : QStringLiteral("当前：标准卡片视图，点击切换为大图卡片"));
    AppSettings settings = m_settings.app();
    settings.workLargeCoverView = m_largeCoverView;
    m_settings.saveApp(settings);
    refresh();
}

void WorkPage::buildEditor()
{
    m_editorDialog = new QDialog(this);

    m_editorDialog->setWindowTitle(QStringLiteral("修改作品"));
    m_editorDialog->setWindowModality(Qt::NonModal);
    m_editorDialog->resize(620, 900);
    auto *layout = new QVBoxLayout(m_editorDialog);
    layout->setContentsMargins(0, 0, 0, 0);
    m_editor = new WorkEditorWidget(m_database, m_themeService, m_coverDirectory, m_fanartDirectory,
                                    m_imageFetchEndpoint, m_editorDialog);
    layout->addWidget(m_editor);
    connect(m_editor, &WorkEditorWidget::workSaved, this, [this](qint64, bool) { refresh(); });
    connect(m_editor, &WorkEditorWidget::workLinkRequested,
            this, &WorkPage::detailRequested);
}

void WorkPage::refresh()
{
    initialize();
    refreshData();
}

void WorkPage::refreshData()
{
    QString errorMessage;
    const std::optional<int> total = m_repository.count(currentSearch(), &errorMessage);
    if (!total.has_value())
    {
        ToastNotification::showMessage(window(),
                                       QStringLiteral("统计作品失败：%1").arg(errorMessage),
                                       ToastNotification::Level::Error, 4000, &m_themeService);
        return;
    }
    m_countLabel->setText(QStringLiteral("过滤总数:%1").arg(*total));
    qInfo() << "WorkPage query returned" << *total << "rows";
    m_lazyArea->reset();
    if (*total == 0)
    {
        m_table->setRowCount(0);
    }
}

void WorkPage::refreshReferences()
{
    initialize();
    const auto repopulate = [](QComboBox *combo, const QList<NamedIdOption> &items)
    {
        const QVariant selected = combo->currentData();
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(QString());
        for (const NamedIdOption &item : items) combo->addItem(item.name, item.id);
        const int index = combo->findData(selected);
        combo->setCurrentIndex(index >= 0 ? index : 0);
    };
    repopulate(m_makerFilter, m_repository.makerOptions());
    repopulate(m_labelFilter, m_repository.labelOptions());
    repopulate(m_seriesFilter, m_repository.seriesOptions());
    m_editor->refreshReferences();
    refresh();
}

void WorkPage::refreshTags()
{
    initialize();
    m_tagSelector->reloadTags();
    m_editor->refreshAssociations();
    refresh();
}

QList<QWidget *> WorkPage::loadCardPage(int pageIndex, int pageSize)
{
    QString errorMessage;
    WorkSearch search = currentSearch();
    search.limit = pageSize;
    search.offset = pageIndex * pageSize;
    const QList<WorkSummary> works = m_repository.search(search, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        ToastNotification::showMessage(window(),
                                       QStringLiteral("加载作品失败：%1").arg(errorMessage),
                                       ToastNotification::Level::Error, 4000, &m_themeService);
        return {};
    }

    if (pageIndex == 0)
    {
        m_table->setRowCount(0);
    }
    QList<QWidget *> cards;
    cards.reserve(works.size());
    for (const WorkSummary &work : works)
    {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *serialItem = new QTableWidgetItem(work.serialNumber);
        serialItem->setData(Qt::UserRole, work.id);
        m_table->setItem(row, 0, serialItem);
        m_table->setItem(row, 1, new QTableWidgetItem(work.chineseTitle));
        m_table->setItem(row, 2, new QTableWidgetItem(work.releaseDate));
        m_table->setItem(row, 3, new QTableWidgetItem(work.director));
        auto *card = new WorkCard(work, m_coverDirectory, m_largeCoverView);
        connect(card, &WorkCard::activated, this, &WorkPage::showWorkDetails);
        connect(card, &WorkCard::editRequested, this, &WorkPage::loadWork);
        cards.append(card);
    }
    return cards;
}

void WorkPage::loadSelectedWork()
{
    const int row = m_table->currentRow();
    if (row < 0 || m_table->item(row, 0) == nullptr)
    {
        return;
    }
    loadWork(m_table->item(row, 0)->data(Qt::UserRole).toLongLong());
}

void WorkPage::showWorkDetails(qint64 workId)
{
    emit detailRequested(workId);
}

void WorkPage::openEditor(qint64 workId)
{
    initialize();
    loadWork(workId);
}

void WorkPage::loadWork(qint64 workId)
{
    if (!m_editor->loadWork(workId))
        return;
    m_editorDialog->setWindowTitle(
        QStringLiteral("修改作品 · %1").arg(m_editor->currentSerialNumber()));
    m_editorDialog->show();
    m_editorDialog->raise();
    m_editorDialog->activateWindow();
}

void WorkPage::openCreateEditor()
{
    initialize();
    m_editor->beginCreate();
    m_editorDialog->setWindowTitle(QStringLiteral("新增作品"));
    m_editorDialog->show();
    m_editorDialog->raise();
    m_editorDialog->activateWindow();
}

WorkSortOrder WorkPage::selectedSortOrder() const
{
    return static_cast<WorkSortOrder>(m_sortSelector->currentData().toInt());
}

WorkSearch WorkPage::currentSearch() const
{
    WorkSearch search;
    search.serialNumber = m_serialFilter->text();
    search.title = m_titleFilter->text();
    search.chineseStory = m_storyFilter->text();
    search.notes = m_searchInput->text();
    search.director = m_directorFilter->text();
    search.actressName = m_actressFilter->text();
    search.actorName = m_actorFilter->text();
    if (m_makerFilter->currentData().isValid())
        search.makerId = m_makerFilter->currentData().toLongLong();
    if (m_labelFilter->currentData().isValid())
        search.labelId = m_labelFilter->currentData().toLongLong();
    if (m_seriesFilter->currentData().isValid())
        search.seriesId = m_seriesFilter->currentData().toLongLong();
    search.randomSeed = m_randomSeed;
    search.randomSeed2 = m_randomSeed2;
    QString privateError;
    switch (m_scopeSelector->currentIndex())
    {
    case 1:
        search.restrictToIncludedWorkIds = true;
        search.includedWorkIds = m_privateRepository.favoriteWorkIds(&privateError);
        break;
    case 2:
        search.restrictToIncludedWorkIds = true;
        search.includedWorkIds = m_privateRepository.favoriteUnwatchedWorkIds(&privateError);
        break;
    case 3:
        search.restrictToIncludedWorkIds = true;
        search.includedWorkIds = m_privateRepository.masturbationWorkIds(&privateError);
        break;
    default:
        break;
    }
    search.tagIds = m_tagSelector->selectedIds();
    search.order = selectedSortOrder();
    return search;
}

} // namespace darkeye
