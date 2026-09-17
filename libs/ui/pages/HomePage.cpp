#include "ui/pages/HomePage.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "ui/components/AsyncImageLabel.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/Pagination.h"
#include "darkeye_ui/components/RatingSelector.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "ui/layouts/myads/WorkspaceWidget.h"
#include "ui/layouts/myads/ThemeAdapter.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace darkeye {

HomePage::HomePage(ThemeService &themeService, QWidget *parent)
    : QWidget(parent), m_themeService(themeService)
{
    setObjectName(QStringLiteral("HomePage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 28, 40, 28);
    root->setSpacing(18);

    auto *heading = new QLabel(QStringLiteral("Darkeye C++"), this);
    heading->setObjectName(QStringLiteral("PageHeading"));
    QFont font = heading->font();
    font.setPointSize(24);
    font.setBold(true);
    heading->setFont(font);
    root->addWidget(heading);
    root->addWidget(new QLabel(QStringLiteral("阶段 3 · 设计系统与公共组件正在接入"), this));

    auto *content = new QHBoxLayout;
    content->setSpacing(24);
    auto *logo = new AsyncImageLabel(this);
    logo->setObjectName(QStringLiteral("HomeLogo"));
    logo->setFixedSize(180, 180);
    logo->setSource(QStringLiteral(":/icons/logo.svg"));
    content->addWidget(logo);

    auto *controls = new QVBoxLayout;
    auto *selector = new DesignComboBox(this);
    selector->addItems({QStringLiteral("全部作品"), QStringLiteral("收藏作品"),
                        QStringLiteral("最近添加")});
    controls->addWidget(selector);
    auto *search = new DesignLineEdit(this);
    search->setPlaceholderText(QStringLiteral("搜索作品、演员或标签"));
    controls->addWidget(search);
    auto *rating = new RatingSelector(this);
    rating->setRating(3);
    controls->addWidget(rating);

    auto *buttons = new QHBoxLayout;
    auto *noticeButton = new DesignButton(QStringLiteral("显示通知"), this);
    noticeButton->setVariant(QStringLiteral("primary"));
    auto *searchIcon = new IconButton(QStringLiteral("search"), &m_themeService, this);
    searchIcon->setToolTip(QStringLiteral("搜索"));
    buttons->addWidget(noticeButton);
    buttons->addWidget(searchIcon);
    buttons->addStretch();
    controls->addLayout(buttons);
    controls->addStretch();
    content->addLayout(controls, 1);
    root->addLayout(content);

    auto *workspace = new myads::WorkspaceWidget(this);
    myads::bindDarkeyeTheme(workspace, &m_themeService);
    workspace->setMinimumHeight(180);
    const QString detailPane = workspace->splitPane(
        QStringLiteral("pane_1"), Qt::Horizontal, false);
    workspace->addPanel(QStringLiteral("影片列表"),
                        new QLabel(QStringLiteral("可移动标签页；工作区支持拆分与恢复")),
                        QStringLiteral("pane_1"));
    workspace->addPanel(QStringLiteral("影片详情"),
                        new QLabel(QStringLiteral("详情面板")), detailPane);
    root->addWidget(workspace);

    auto *pagination = new Pagination(237, 20, this);
    root->addWidget(pagination);
    root->addStretch();

    connect(noticeButton, &QPushButton::clicked, this, [this] {
        ToastNotification::showMessage(window(), QStringLiteral("公共组件已接入界面"),
                                       ToastNotification::Level::Success, 2500,
                                       &m_themeService);
    });
}

} // namespace darkeye

