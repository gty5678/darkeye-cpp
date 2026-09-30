#include "ui/pages/HomePage.h"

#include "darkeye_ui/components/EmptyState.h"

#include <QVBoxLayout>

namespace darkeye {

HomePage::HomePage(ThemeService &themeService, QWidget *parent)
    : LazyWidget(parent)
{
    Q_UNUSED(themeService);
    setObjectName(QStringLiteral("HomePage"));
}

void HomePage::lazyLoad()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->addWidget(new EmptyState(
        QStringLiteral("首页内容即将上线"),
        QStringLiteral("后续将在这里添加新的首页内容。"), {}, this));
}

} // namespace darkeye
