#include "darkeye_ui/components/IconButton.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/theme/IconProvider.h"

namespace darkeye {

IconButton::IconButton(const QString &iconName, ThemeService *themeService,
                       QWidget *parent)
    : QPushButton(parent), m_themeService(themeService), m_iconName(iconName)
{
    setObjectName(QStringLiteral("DesignIconPushButton"));
    setCursor(Qt::PointingHandCursor);
    setFlat(true);
    setButtonPixelSize(m_buttonPixelSize);
    if (m_themeService != nullptr) {
        connect(m_themeService, &ThemeService::themeChanged, this,
                [this] { refreshIcon(); });
    }
    refreshIcon();
}

QString IconButton::iconName() const { return m_iconName; }

void IconButton::setIconName(const QString &name)
{
    m_iconName = name;
    m_iconPath.clear();
    refreshIcon();
}

void IconButton::setIconPath(const QString &path)
{
    m_iconPath = path;
    refreshIcon();
}

void IconButton::setIconPixelSize(int size)
{
    m_iconPixelSize = qMax(1, size);
    setIconSize(QSize(m_iconPixelSize, m_iconPixelSize));
    refreshIcon();
}

void IconButton::setButtonPixelSize(int size)
{
    m_buttonPixelSize = qMax(1, size);
    setFixedSize(m_buttonPixelSize, m_buttonPixelSize);
}

void IconButton::setInverted(bool inverted)
{
    m_inverted = inverted;
    refreshIcon();
}

void IconButton::refreshIcon()
{
    const ThemeTokens tokens = m_themeService != nullptr
                                   ? ThemeService::tokens(m_themeService->current(),
                                                          m_themeService->customPrimary())
                                   : ThemeService::tokens(ThemeId::Light);
    const QColor color(m_inverted ? tokens.textInverse : tokens.icon);
    const QSize size(m_iconPixelSize, m_iconPixelSize);
    setIcon(m_iconPath.isEmpty() ? IconProvider::builtIn(m_iconName, size, color)
                                 : IconProvider::fromSvg(m_iconPath, size, color));
    setIconSize(size);
}

} // namespace darkeye


