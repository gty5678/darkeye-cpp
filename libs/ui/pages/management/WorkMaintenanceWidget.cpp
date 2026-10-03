#include "ui/pages/management/WorkMaintenanceWidget.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "services/TopActressSyncService.h"
#include "services/ActressSyncService.h"
#include "services/BatchTranslationService.h"
#include "settings/Settings.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/ToastNotification.h"

#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>

namespace darkeye
{

WorkMaintenanceWidget::WorkMaintenanceWidget(QSqlDatabase database, ThemeService &themes,
                                             QString coverDirectory, QString actressImageDirectory,
                                             QUrl imageFetchEndpoint, QUrl topActressesEndpoint,
                                             QWidget *parent)
    : QWidget(parent), m_database(database), m_service(database, std::move(coverDirectory)), m_themes(themes),
      m_actressImageDirectory(std::move(actressImageDirectory)),
      m_imageFetchEndpoint(std::move(imageFetchEndpoint))
{
    m_topActresses = new TopActressSyncService(database, std::move(topActressesEndpoint), this);
    m_actressSync = new ActressSyncService(database, settings::crawler().actressApiBaseUrl,
                                            m_actressImageDirectory, m_imageFetchEndpoint, this);
    m_translations = new BatchTranslationService(database, this);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);
    auto *panel = new QWidget(this);
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(8);

    const auto addAction = [this, panelLayout](const QString &text, const QString &toolTip,
                                                bool enabled)
    {
        auto *button = new DesignButton(text, this);
        button->setToolTip(toolTip);
        button->setEnabled(enabled);
        button->setMinimumWidth(300);
        panelLayout->addWidget(button, 0, Qt::AlignLeft);
        return button;
    };
    m_popularButton =
        addAction(QStringLiteral("更新热门女优"),
                  QStringLiteral("通过本地 Collector 与浏览器扩展解析前 50 名并写入女优库"), true);
    m_actressButton = addAction(QStringLiteral("更新标记需要更新的女优数据"),
                                QStringLiteral("逐条采集 Minnano AV 并合并所有 need_update 女优"), true);
    auto *maker = addAction(QStringLiteral("根据番号前缀判断片商"),
                            QStringLiteral("仅处理未删除作品；重复前缀沿用旧版后写规则优先"), true);
    m_translateButton = addAction(
        QStringLiteral("一键翻译标题/简介"),
        QStringLiteral("后台执行：仅在中文标题或简介为空时，翻译对应的日文内容并写入"), true);
    m_forceTranslateButton = addAction(
        QStringLiteral("覆盖翻译标题/简介"),
        QStringLiteral("后台执行：强制翻译日文标题和简介，覆盖现有中文内容；并发 4 路请求"), true);
    auto *covers = addAction(QStringLiteral("统一封面文件名为番号.jpg"),
                             QStringLiteral("目标文件已存在时跳过，不覆盖任何封面"), true);
    panelLayout->addStretch();
    root->addWidget(panel);
    connect(maker, &QPushButton::clicked, this, &WorkMaintenanceWidget::assignMakers);
    connect(covers, &QPushButton::clicked, this, &WorkMaintenanceWidget::normalizeCovers);
    connect(m_popularButton, &QPushButton::clicked, this,
            &WorkMaintenanceWidget::updatePopularActresses);
    connect(m_actressButton, &QPushButton::clicked, this,
            &WorkMaintenanceWidget::updateNeededActresses);
    connect(m_translateButton, &QPushButton::clicked, this,
            &WorkMaintenanceWidget::translateMissingFields);
    connect(m_forceTranslateButton, &QPushButton::clicked, this,
            &WorkMaintenanceWidget::forceTranslateFields);
    connect(m_topActresses, &TopActressSyncService::finished, this,
            [this](const TopActressSyncResult &result)
            {
                m_popularButton->setEnabled(true);
                m_popularButton->setText(QStringLiteral("更新热门女优"));
                if (!result.succeeded)
                {
                    Toast::showError(
                        window(), QStringLiteral("热门女优更新失败：%1").arg(result.errorMessage),
                        &m_themes);
                    return;
                }
                emit actressesChanged();
                Toast::showSuccess(window(), result.summary(), &m_themes);
            });
    connect(m_actressSync, &ActressSyncService::finished, this,
            [this](const ActressSyncResult &result)
            {
                if (result.succeeded)
                    ++m_syncedActresses;
                else
                    ++m_failedActresses;
                syncNextActress();
            });
    connect(m_translations, &BatchTranslationService::progress, this,
            [this](int done, int total, qint64 elapsedMilliseconds)
            {
                if (m_translationMode != BatchTranslationMode::ForceOverwrite)
                    return;
                m_forceTranslateButton->setText(QStringLiteral("覆盖翻译中 %1/%2 · %3s")
                                                    .arg(done)
                                                    .arg(total)
                                                    .arg(elapsedMilliseconds / 1000.0, 0, 'f', 1));
            });
    connect(m_translations, &BatchTranslationService::finished, this,
            [this](const BatchTranslationResult &result)
            {
                m_translateButton->setEnabled(true);
                m_forceTranslateButton->setEnabled(true);
                m_translateButton->setText(QStringLiteral("一键翻译标题/简介"));
                m_forceTranslateButton->setText(QStringLiteral("覆盖翻译标题/简介"));
                if (!result.succeeded)
                {
                    Toast::showError(window(), QStringLiteral("批量翻译失败：%1").arg(result.errorMessage),
                                     &m_themes);
                    return;
                }
                emit worksChanged();
                Toast::showSuccess(window(), result.summary(m_translationMode),
                                   &m_themes);
            });
}

void WorkMaintenanceWidget::assignMakers()
{
    const MakerAssignmentResult result = m_service.assignMakersFromPrefixes();
    if (!result.succeeded)
    {
        Toast::showError(window(), QStringLiteral("批量更新失败：%1").arg(result.errorMessage),
                         &m_themes);
        return;
    }
    emit worksChanged();
    Toast::showSuccess(window(), result.summary(), &m_themes);
}

void WorkMaintenanceWidget::normalizeCovers()
{
    const CoverNormalizationResult result = m_service.normalizeCoverFileNames();
    if (!result.succeeded)
    {
        Toast::showError(window(), QStringLiteral("封面整理失败：%1").arg(result.errorMessage),
                         &m_themes);
        return;
    }
    emit worksChanged();
    Toast::showSuccess(window(), result.summary(), &m_themes);
}

void WorkMaintenanceWidget::updatePopularActresses()
{
    if (!m_topActresses->start())
        return;
    m_popularButton->setEnabled(false);
    m_popularButton->setText(QStringLiteral("正在更新热门女优…"));
    Toast::showMessage(window(), QStringLiteral("已请求 Collector 解析热门女优"),
                       Toast::Level::Info, 2500, &m_themes);
}

void WorkMaintenanceWidget::updateNeededActresses()
{
    if (m_actressSync->isBusy())
        return;
    m_pendingActressIds.clear();
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT actress_id FROM actress WHERE need_update=1 ORDER BY actress_id")))
    {
        Toast::showError(window(), QStringLiteral("读取待更新女优失败：%1").arg(query.lastError().text()),
                         &m_themes);
        return;
    }
    while (query.next())
        m_pendingActressIds.append(query.value(0).toLongLong());
    if (m_pendingActressIds.isEmpty())
    {
        Toast::showMessage(window(), QStringLiteral("没有待更新的女优"), Toast::Level::Info, 2500,
                           &m_themes);
        return;
    }
    m_syncedActresses = 0;
    m_failedActresses = 0;
    m_actressButton->setEnabled(false);
    m_actressButton->setText(QStringLiteral("正在更新女优…"));
    syncNextActress();
}

void WorkMaintenanceWidget::translateMissingFields()
{
    m_translationMode = BatchTranslationMode::FillMissing;
    if (!m_translations->start(BatchTranslationMode::FillMissing))
        return;
    m_translateButton->setEnabled(false);
    m_forceTranslateButton->setEnabled(false);
    m_translateButton->setText(QStringLiteral("正在翻译标题/简介…"));
    Toast::showMessage(window(), QStringLiteral("正在后台翻译并写库，请稍候…"),
                       Toast::Level::Info, 2500, &m_themes);
}

void WorkMaintenanceWidget::forceTranslateFields()
{
    m_translationMode = BatchTranslationMode::ForceOverwrite;
    if (!m_translations->start(BatchTranslationMode::ForceOverwrite))
        return;
    m_translateButton->setEnabled(false);
    m_forceTranslateButton->setEnabled(false);
    m_forceTranslateButton->setText(QStringLiteral("覆盖翻译中 0/0 · 0.0s"));
    Toast::showMessage(window(), QStringLiteral("正在后台强制翻译并覆盖写库，请稍候…"),
                       Toast::Level::Info, 2500, &m_themes);
}

void WorkMaintenanceWidget::syncNextActress()
{
    while (!m_pendingActressIds.isEmpty())
    {
        const qint64 actressId = m_pendingActressIds.takeFirst();
        if (m_actressSync->start(actressId))
            return;
        ++m_failedActresses;
    }
    m_actressButton->setEnabled(true);
    m_actressButton->setText(QStringLiteral("更新标记需要更新的女优数据"));
    if (m_syncedActresses > 0)
        emit actressesChanged();
    Toast::showMessage(window(), QStringLiteral("女优更新完成：成功 %1，失败 %2")
                           .arg(m_syncedActresses).arg(m_failedActresses),
                       m_failedActresses == 0 ? Toast::Level::Success : Toast::Level::Warning,
                       4000, &m_themes);
}

} // namespace darkeye
