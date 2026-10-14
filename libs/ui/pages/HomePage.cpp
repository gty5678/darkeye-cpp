#include "ui/pages/HomePage.h"

#include "app/Resources.h"
#include "database/repositories/WorkRepository.h"
#include "darkeye_ui/components/InteractionEffects.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/ToastNotification.h"

#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QQuickItem>
#include <QQuickWidget>
#include <QRandomGenerator>
#include <QUrl>

namespace darkeye {

HomePage::HomePage(QSqlDatabase database, const QString &coverDirectory, ThemeService &themes,
                   QWidget *parent)
    : LazyWidget(parent), m_themes(themes), m_database(std::move(database)),
      m_coverDirectory(coverDirectory)
{
    setObjectName(QStringLiteral("HomePage"));
    connect(&m_themes, &ThemeService::themeChanged, this, &HomePage::updateOutlineColor);
}

void HomePage::lazyLoad()
{
    Resources::ensureInitialized();
    auto *root = new QGridLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(10, 10, 10, 10);
    bar->addStretch();
    auto *reload = new RotateButton(QStringLiteral("refresh_cw"), {}, 24, 24, true,
                                    &m_themes, this);
    reload->setToolTip(QStringLiteral("刷新"));
    reload->setAccessibleName(QStringLiteral("刷新"));
    reload->setObjectName(QStringLiteral("HomeRefreshButton"));
    reload->setStyleSheet(QStringLiteral(
        "QPushButton#HomeRefreshButton { background: transparent; border: none; }"));
    bar->addWidget(reload);

    m_view = new QQuickWidget(this);
    m_view->setObjectName(QStringLiteral("HomeShopView"));
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->setMinimumHeight(360);
    m_view->setClearColor(QColor(QStringLiteral("#18181c")));
    m_view->setSource(QUrl(QStringLiteral("qrc:/qml/shop/shop_viewport.qml")));
    root->addWidget(m_view, 0, 0);
    root->addLayout(bar, 0, 0, Qt::AlignTop);
    reload->raise();
    connect(reload, &QPushButton::clicked, this, &HomePage::refresh);
    if (QObject *scene = m_view->rootObject())
        connect(scene, SIGNAL(coverClicked(int)), this, SLOT(openCover(int)));
    updateOutlineColor();
    refresh();
}

void HomePage::updateOutlineColor()
{
    if (m_view != nullptr && m_view->rootObject() != nullptr)
        m_view->rootObject()->setProperty("outlineColor", QColor(m_themes.currentTokens().primary));
}

void HomePage::openCover(int index)
{
    if (index >= 0 && index < m_works.size()) emit detailRequested(m_works.at(index).id);
}

void HomePage::refresh()
{
    WorkSearch search;
    search.limit = 18;
    search.order = WorkSortOrder::Random;
    search.randomSeed = QRandomGenerator::global()->generate();
    search.randomSeed2 = QRandomGenerator::global()->generate();
    QString error;
    const auto works = WorkRepository(m_database).search(search, &error);
    if (!error.isEmpty())
    {
        ToastNotification::showMessage(window(), QStringLiteral("刷新失败：%1").arg(error),
                                       ToastNotification::Level::Error);
        return;
    }
    m_works = works;
    QVariantList textures;
    for (const WorkSummary &work : m_works)
    {
        const QString path = QDir(m_coverDirectory).filePath(work.imageUrl);
        textures.append(!work.imageUrl.isEmpty() && QFileInfo::exists(path)
            ? QUrl::fromLocalFile(path).toString()
            : QStringLiteral("qrc:/assets/maps/0.png"));
    }
    if (QObject *scene = m_view->rootObject())
        scene->setProperty("coverTextureSources", textures);
}

} // namespace darkeye
