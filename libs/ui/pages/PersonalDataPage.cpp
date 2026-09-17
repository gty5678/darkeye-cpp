#include "ui/pages/PersonalDataPage.h"

#include "database/repositories/PrivateRepository.h"
#include "database/repositories/StatisticsRepository.h"
#include "ui/components/AsyncImageLabel.h"
#include "darkeye_ui/components/Charts.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "ui/components/StatsOverviewCards.h"
#include "darkeye_ui/theme/ThemeService.h"

#include <QDate>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace darkeye
{

class TopActressCard final : public QFrame
{
public:
    TopActressCard(int days, QString imageDirectory, QWidget *parent = nullptr)
        : QFrame(parent), m_imageDirectory(std::move(imageDirectory))
    {
        setObjectName(QStringLiteral("MostLikeActressCard_%1").arg(days));
        setFrameShape(QFrame::StyledPanel);
        setFixedSize(170, 250);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 6, 10, 10);
        auto *title = new DesignLabel(QStringLiteral("过去%1天最喜欢的女优").arg(days), this);
        title->setAlignment(Qt::AlignCenter);
        m_image = new AsyncImageLabel(this);
        m_image->setFixedSize(130, 150);
        m_image->setFitMode(ImageFitMode::Cover);
        m_name = new DesignLabel(QStringLiteral("加载中..."), this);
        m_name->setAlignment(Qt::AlignCenter);
        m_detail = new DesignLabel({}, this);
        m_detail->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);
        layout->addWidget(m_image, 0, Qt::AlignCenter);
        layout->addWidget(m_name);
        layout->addWidget(m_detail);
    }

    void setStatistic(const std::optional<TopActressStatistic> &statistic)
    {
        if (!statistic.has_value())
        {
            m_image->clearSource();
            m_name->setText(QStringLiteral("暂无记录"));
            m_detail->clear();
            return;
        }
        QString imagePath = statistic->imagePath.trimmed();
        if (!imagePath.isEmpty() && QDir::isRelativePath(imagePath))
        {
            imagePath = QDir(m_imageDirectory).filePath(imagePath);
        }
        m_image->setSource(imagePath);
        m_name->setText(statistic->name);
        m_detail->setText(QStringLiteral("%1次 · %2")
                              .arg(statistic->recordCount)
                              .arg(statistic->latestRecordTime.left(10)));
    }

private:
    QString m_imageDirectory;
    AsyncImageLabel *m_image = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_detail = nullptr;
};

PersonalDataPage::PersonalDataPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                                   ThemeService &themeService, QString actressImageDirectory,
                                   QWidget *parent)
    : QWidget(parent), m_publicDatabase(std::move(publicDatabase)),
      m_privateDatabase(std::move(privateDatabase)), m_themeService(themeService),
      m_actressImageDirectory(std::move(actressImageDirectory))
{
    setObjectName(QStringLiteral("PersonalDataPage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 10, 0, 0);

    m_overview = new StatsOverviewCards(m_publicDatabase, m_privateDatabase, this);
    root->addWidget(m_overview);

    auto *summary = new QHBoxLayout;
    for (const int days : {30, 180, 365})
    {
        auto *card = new TopActressCard(days, m_actressImageDirectory, this);
        card->setProperty("days", days);
        m_topActressCards.append(card);
        summary->addWidget(card);
    }
    auto *cycleCard = new QFrame(this);
    cycleCard->setObjectName(QStringLiteral("WorkSaleCycle"));
    cycleCard->setFrameShape(QFrame::StyledPanel);
    cycleCard->setFixedSize(170, 250);
    auto *cycleLayout = new QVBoxLayout(cycleCard);
    auto *cycleTitle =
        new DesignLabel(QStringLiteral("收藏作品中未观看去化周期"), cycleCard);
    cycleTitle->setAlignment(Qt::AlignCenter);
    cycleTitle->setWordWrap(true);
    m_salesCycle = new DesignLabel(QStringLiteral("加载中..."), cycleCard);
    m_salesCycle->setObjectName(QStringLiteral("WorkSaleCycleValue"));
    m_salesCycle->setAlignment(Qt::AlignCenter);
    QFont cycleFont = m_salesCycle->font();
    cycleFont.setPointSize(30);
    m_salesCycle->setFont(cycleFont);
    cycleLayout->addWidget(cycleTitle);
    cycleLayout->addWidget(m_salesCycle, 1);
    summary->addWidget(cycleCard);
    summary->addStretch();
    root->addLayout(summary);

    auto *heatmapControls = new QHBoxLayout;
    m_recordKindSelector = new DesignComboBox(this);
    m_recordKindSelector->setObjectName(QStringLiteral("RecordKindSelector"));
    m_recordKindSelector->addItem(QStringLiteral("撸管"),
                                  static_cast<int>(PersonalRecordKind::Masturbation));
    m_recordKindSelector->addItem(QStringLiteral("做爱"),
                                  static_cast<int>(PersonalRecordKind::LoveMaking));
    m_recordKindSelector->addItem(QStringLiteral("晨勃"),
                                  static_cast<int>(PersonalRecordKind::SexualArousal));
    m_yearSelector = new DesignComboBox(this);
    m_yearSelector->setObjectName(QStringLiteral("RecordYearSelector"));
    StatisticsRepository statistics(m_publicDatabase, m_privateDatabase);
    const int currentYear = QDate::currentDate().year();
    const int earliestYear = qMin(currentYear, statistics.earliestRecordYear());
    for (int year = currentYear; year >= earliestYear; --year)
    {
        m_yearSelector->addItem(QString::number(year), year);
    }
    m_heatmapTitle = new DesignLabel({}, this);
    m_heatmapTitle->setObjectName(QStringLiteral("RecordHeatmapTitle"));
    heatmapControls->addWidget(m_heatmapTitle);
    heatmapControls->addStretch();
    heatmapControls->addWidget(m_recordKindSelector);
    heatmapControls->addWidget(m_yearSelector);
    root->addLayout(heatmapControls);

    m_heatmap = new CalendarHeatmap(currentYear, {}, &m_themeService, this);
    m_heatmap->setObjectName(QStringLiteral("PersonalRecordHeatmap"));
    root->addWidget(m_heatmap, 0, Qt::AlignCenter);
    root->addStretch();

    connect(m_recordKindSelector, &QComboBox::currentIndexChanged, this,
            [this] { refreshHeatmap(); });
    connect(m_yearSelector, &QComboBox::currentIndexChanged, this,
            [this] { refreshHeatmap(); });
    refresh();
}

void PersonalDataPage::refresh()
{
    m_overview->refresh();
    StatisticsRepository statistics(m_publicDatabase, m_privateDatabase);
    for (TopActressCard *card : m_topActressCards)
    {
        card->setStatistic(statistics.topActress(card->property("days").toInt()));
    }
    const int cycle = statistics.favoriteUnwatchedSalesCycle();
    m_salesCycle->setText(QStringLiteral("%1天").arg(cycle));
    m_salesCycle->setStyleSheet(cycle > 30 ? QStringLiteral("color:#FF0000;") : QString());
    refreshHeatmap();
}

void PersonalDataPage::refreshHeatmap()
{
    if (m_yearSelector->currentIndex() < 0 || m_recordKindSelector->currentIndex() < 0)
    {
        return;
    }
    const int year = m_yearSelector->currentData().toInt();
    const auto kind = static_cast<PersonalRecordKind>(m_recordKindSelector->currentData().toInt());
    PrivateRepository repository(m_privateDatabase);
    const QMap<QDate, int> counts = repository.dailyCounts(year, kind);
    QHash<QDate, int> hash;
    for (auto iterator = counts.cbegin(); iterator != counts.cend(); ++iterator)
    {
        hash.insert(iterator.key(), iterator.value());
    }
    int total = 0;
    for (const int count : counts)
    {
        total += count;
    }
    m_heatmapTitle->setText(QStringLiteral("%1%2次在当年中")
                                .arg(m_recordKindSelector->currentText())
                                .arg(total));
    m_heatmap->updateData(year, hash);
}

} // namespace darkeye
