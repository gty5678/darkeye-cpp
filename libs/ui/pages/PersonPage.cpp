#include "ui/pages/PersonPage.h"

#include "darkeye_ui/components/CompleterLineEdit.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/InteractionEffects.h"
#include "darkeye_ui/components/LazyScrollArea.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "ui/components/PersonCard.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>

namespace darkeye
{

PersonPage::PersonPage(PersonKind kind, QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                       ThemeService &themeService, QString imageDirectory, QWidget *parent)
    : QWidget(parent), m_kind(kind), m_repository(std::move(publicDatabase)),
      m_privateRepository(std::move(privateDatabase)), m_themeService(themeService),
      m_imageDirectory(std::move(imageDirectory))
{
    setObjectName(kind == PersonKind::Actress ? QStringLiteral("ActressPage")
                                              : QStringLiteral("ActorPage"));
    m_randomSeed = QRandomGenerator::global()->generate();
    m_randomSeed2 = QRandomGenerator::global()->generate();
    buildUi();
    m_lazyArea->setLoader([this](int pageIndex, int pageSize)
                          { return loadCardPage(pageIndex, pageSize); });
    updateCount();
}

void PersonPage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    auto *filterBar = new QWidget(this);
    filterBar->setObjectName(m_kind == PersonKind::Actress ? QStringLiteral("ActressFilterBar")
                                                           : QStringLiteral("ActorFilterBar"));
    filterBar->setFixedHeight(44);
    auto *filters = new QHBoxLayout(filterBar);
    filters->setContentsMargins(10, 0, 10, 0);
    filters->setSpacing(6);

    filters->addWidget(new DesignLabel(m_kind == PersonKind::Actress ? QStringLiteral("女优")
                                                                     : QStringLiteral("男优"),
                                       filterBar));
    const QStringList nameSuggestions = m_repository.nameSuggestions(m_kind);
    m_nameInput = new CompleterLineEdit([nameSuggestions] { return nameSuggestions; }, filterBar);
    m_nameInput->setObjectName(QStringLiteral("PersonNameFilter"));
    m_nameInput->setFixedWidth(180);
    filters->addWidget(m_nameInput);

    m_cupSelector = new DesignComboBox(filterBar);
    m_cupSelector->setObjectName(QStringLiteral("ActressCupFilter"));
    m_cupSelector->addItem(QString());
    m_cupSelector->addItems(m_repository.cupOptions());
    if (m_kind == PersonKind::Actress)
    {
        filters->addWidget(new DesignLabel(QStringLiteral("罩杯"), filterBar));
        filters->addWidget(m_cupSelector);
    }
    else
    {
        m_cupSelector->hide();
    }

    auto *refreshButton =
        new RotateButton(QStringLiteral("refresh_cw"), &m_themeService, filterBar);
    refreshButton->setObjectName(QStringLiteral("PersonRefreshButton"));
    refreshButton->setToolTip(QStringLiteral("刷新人物"));
    auto *clearButton = new ShakeButton(QStringLiteral("eraser"), &m_themeService, filterBar);
    clearButton->setObjectName(QStringLiteral("PersonClearButton"));
    clearButton->setToolTip(QStringLiteral("清空筛选"));
    filters->addWidget(refreshButton);
    filters->addWidget(clearButton);

    m_countLabel = new DesignLabel({}, filterBar);
    m_countLabel->setObjectName(QStringLiteral("PersonCountLabel"));
    m_countLabel->setFixedWidth(110);
    filters->addWidget(m_countLabel);
    filters->addStretch();

    m_scopeSelector = new DesignComboBox(filterBar);
    m_scopeSelector->setObjectName(QStringLiteral("PersonScopeSelector"));
    m_scopeSelector->addItem(QStringLiteral("公共库范围"), false);
    m_scopeSelector->addItem(QStringLiteral("收藏库范围"), true);
    if (m_kind == PersonKind::Actress)
    {
        filters->addWidget(m_scopeSelector);
    }
    else
    {
        m_scopeSelector->hide();
    }

    m_sortSelector = new DesignComboBox(filterBar);
    m_sortSelector->setObjectName(QStringLiteral("PersonSortSelector"));
    m_sortSelector->addItem(QStringLiteral("随机顺序"), static_cast<int>(PersonSortOrder::Random));
    m_sortSelector->addItem(QStringLiteral("添加顺序"),
                            static_cast<int>(PersonSortOrder::CreatedAscending));
    m_sortSelector->addItem(QStringLiteral("添加逆序"),
                            static_cast<int>(PersonSortOrder::CreatedDescending));
    if (m_kind == PersonKind::Actress)
    {
        m_sortSelector->addItem(QStringLiteral("年龄顺序"),
                                static_cast<int>(PersonSortOrder::BirthdayAscending));
        m_sortSelector->addItem(QStringLiteral("年龄逆序"),
                                static_cast<int>(PersonSortOrder::BirthdayDescending));
        m_sortSelector->addItem(QStringLiteral("出道顺序"),
                                static_cast<int>(PersonSortOrder::DebutAscending));
        m_sortSelector->addItem(QStringLiteral("出道逆序"),
                                static_cast<int>(PersonSortOrder::DebutDescending));
        m_sortSelector->addItem(QStringLiteral("身高顺序"),
                                static_cast<int>(PersonSortOrder::HeightAscending));
        m_sortSelector->addItem(QStringLiteral("身高逆序"),
                                static_cast<int>(PersonSortOrder::HeightDescending));
        m_sortSelector->addItem(QStringLiteral("罩杯顺序"),
                                static_cast<int>(PersonSortOrder::CupAscending));
        m_sortSelector->addItem(QStringLiteral("罩杯逆序"),
                                static_cast<int>(PersonSortOrder::CupDescending));
        m_sortSelector->addItem(QStringLiteral("腰臀比顺序"),
                                static_cast<int>(PersonSortOrder::WaistHipRatioAscending));
        m_sortSelector->addItem(QStringLiteral("腰臀比逆序"),
                                static_cast<int>(PersonSortOrder::WaistHipRatioDescending));
    }
    else
    {
        m_sortSelector->addItem(QStringLiteral("封面优先"),
                                static_cast<int>(PersonSortOrder::ImageFirst));
    }
    m_sortSelector->setCurrentIndex(
        m_sortSelector->findData(static_cast<int>(PersonSortOrder::CreatedDescending)));
    filters->addWidget(m_sortSelector);
    root->addWidget(filterBar);

    m_lazyArea = new LazyScrollArea(150, this);
    m_lazyArea->setObjectName(QStringLiteral("PersonLazyScrollArea"));
    root->addWidget(m_lazyArea, 1);

    m_filterTimer = new QTimer(this);
    m_filterTimer->setSingleShot(true);
    m_filterTimer->setInterval(50);
    connect(m_filterTimer, &QTimer::timeout, this, &PersonPage::applyFilters);
    connect(m_nameInput, &CompleterLineEdit::textChanged, m_filterTimer,
            qOverload<>(&QTimer::start));
    connect(m_cupSelector, &QComboBox::currentIndexChanged, this,
            [this] { m_filterTimer->start(); });
    connect(m_scopeSelector, &QComboBox::currentIndexChanged, this,
            [this] { m_filterTimer->start(); });
    connect(m_sortSelector, &QComboBox::currentIndexChanged, this,
            [this] { m_filterTimer->start(); });
    connect(refreshButton, &QPushButton::clicked, this, &PersonPage::refresh);
    connect(clearButton, &QPushButton::clicked, this, &PersonPage::clearFilters);
}

void PersonPage::refresh()
{
    if (static_cast<PersonSortOrder>(m_sortSelector->currentData().toInt()) ==
        PersonSortOrder::Random)
    {
        m_randomSeed = QRandomGenerator::global()->generate();
        m_randomSeed2 = QRandomGenerator::global()->generate();
    }
    m_lazyArea->reset();
    updateCount();
}

PersonKind PersonPage::kind() const noexcept
{
    return m_kind;
}

void PersonPage::applyFilters()
{
    m_lazyArea->reset();
    updateCount();
}

void PersonPage::clearFilters()
{
    m_nameInput->clear();
    m_cupSelector->setCurrentIndex(0);
    m_scopeSelector->setCurrentIndex(0);
    m_sortSelector->setCurrentIndex(
        m_sortSelector->findData(static_cast<int>(PersonSortOrder::CreatedDescending)));
    applyFilters();
}

void PersonPage::updateCount()
{
    QString errorMessage;
    const std::optional<int> total = m_repository.count(currentSearch(), &errorMessage);
    if (!total.has_value())
    {
        m_countLabel->setText(QStringLiteral("查询失败"));
        Toast::showError(this, errorMessage, &m_themeService);
        return;
    }
    m_countLabel->setText(QStringLiteral("过滤总数:%1").arg(*total));
}

PersonSearch PersonPage::currentSearch() const
{
    PersonSearch search;
    search.kind = m_kind;
    search.name = m_nameInput->text();
    search.cup = m_kind == PersonKind::Actress ? m_cupSelector->currentText() : QString();
    search.sortOrder = static_cast<PersonSortOrder>(m_sortSelector->currentData().toInt());
    search.randomSeed = m_randomSeed;
    search.randomSeed2 = m_randomSeed2;
    if (m_kind == PersonKind::Actress && m_scopeSelector->currentData().toBool())
    {
        search.restrictToIncludedIds = true;
        search.includedIds = m_privateRepository.favoriteActressIds();
    }
    return search;
}

QList<QWidget *> PersonPage::loadCardPage(int pageIndex, int pageSize)
{
    PersonSearch search = currentSearch();
    search.limit = pageSize;
    search.offset = pageIndex * pageSize;
    QString errorMessage;
    const QList<PersonSummary> people = m_repository.search(search, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        Toast::showError(this, errorMessage, &m_themeService);
        return {};
    }
    QList<QWidget *> cards;
    cards.reserve(people.size());
    for (const PersonSummary &person : people)
    {
        auto *card = new PersonCard(person.id, person.name, person.imagePath, m_imageDirectory);
        connect(card, &PersonCard::activated, this,
                [this](qint64 personId) { emit detailRequested(m_kind, personId); });
        connect(card, &PersonCard::editRequested, this,
                [this](qint64 personId) { emit editRequested(m_kind, personId); });
        cards.append(card);
    }
    return cards;
}

} // namespace darkeye


