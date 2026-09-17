#include "ui/components/WorkMaintenanceWidget.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "services/TopActressSyncService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignLabel.h"
#include "darkeye_ui/components/ToastNotification.h"

#include <QPushButton>
#include <QVBoxLayout>

namespace darkeye
{

WorkMaintenanceWidget::WorkMaintenanceWidget(QSqlDatabase database, ThemeService &themes,
                                             QString coverDirectory, QUrl topActressesEndpoint,
                                             QWidget *parent)
    : QWidget(parent), m_service(database, std::move(coverDirectory)), m_themes(themes)
{
    m_topActresses = new TopActressSyncService(database, std::move(topActressesEndpoint), this);
    setObjectName(QStringLiteral("WorkMaintenanceWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);
    auto *heading = new DesignLabel(QStringLiteral("批量操作"), this);
    heading->setObjectName(QStringLiteral("WorkMaintenanceHeading"));
    QFont headingFont = heading->font();
    headingFont.setPointSize(16);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    root->addWidget(heading);
    root->addWidget(new DesignLabel(
        QStringLiteral("库级维护会跳过缺失、冲突和越界文件，并在数据库失败时回滚已移动文件。"),
        this));

    const auto addAction = [this, root](const QString &objectName, const QString &text,
                                        const QString &toolTip, bool enabled)
    {
        auto *button = new DesignButton(text, this);
        button->setObjectName(objectName);
        button->setToolTip(toolTip);
        button->setEnabled(enabled);
        button->setMinimumWidth(300);
        root->addWidget(button, 0, Qt::AlignLeft);
        return button;
    };
    m_popularButton =
        addAction(QStringLiteral("UpdatePopularActressesButton"), QStringLiteral("更新热门女优"),
                  QStringLiteral("通过本地 Collector 与浏览器扩展解析前 50 名并写入女优库"), true);
    auto *actress = addAction(QStringLiteral("UpdateMarkedActressesButton"),
                              QStringLiteral("更新标记需要更新的女优数据"),
                              QStringLiteral("等待女优采集队列迁移"), false);
    Q_UNUSED(actress);
    auto *maker = addAction(QStringLiteral("AssignMakerFromPrefixButton"),
                            QStringLiteral("根据番号前缀判断片商"),
                            QStringLiteral("仅处理未删除作品；重复前缀沿用旧版后写规则优先"), true);
    auto *translate =
        addAction(QStringLiteral("TranslateMissingWorkFieldsButton"),
                  QStringLiteral("一键翻译标题/简介"), QStringLiteral("等待翻译服务迁移"), false);
    Q_UNUSED(translate);
    auto *forceTranslate =
        addAction(QStringLiteral("ForceTranslateWorkFieldsButton"),
                  QStringLiteral("覆盖翻译标题/简介"), QStringLiteral("等待翻译服务迁移"), false);
    Q_UNUSED(forceTranslate);
    auto *covers = addAction(QStringLiteral("NormalizeCoverFileNamesButton"),
                             QStringLiteral("统一封面文件名为番号.jpg"),
                             QStringLiteral("目标文件已存在时跳过，不覆盖任何封面"), true);
    root->addStretch();
    connect(maker, &QPushButton::clicked, this, &WorkMaintenanceWidget::assignMakers);
    connect(covers, &QPushButton::clicked, this, &WorkMaintenanceWidget::normalizeCovers);
    connect(m_popularButton, &QPushButton::clicked, this,
            &WorkMaintenanceWidget::updatePopularActresses);
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

} // namespace darkeye


