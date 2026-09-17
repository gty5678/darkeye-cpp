#pragma once

#include <QWidget>

namespace darkeye {

class ThemeService;

class HomePage final : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(ThemeService &themeService, QWidget *parent = nullptr);

private:
    ThemeService &m_themeService;
};

} // namespace darkeye
