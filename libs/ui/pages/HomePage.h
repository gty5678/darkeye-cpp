#pragma once

#include "darkeye_ui/base/LazyWidget.h"

namespace darkeye {

class ThemeService;

class HomePage final : public LazyWidget
{
    Q_OBJECT

public:
    explicit HomePage(ThemeService &themeService, QWidget *parent = nullptr);

private:
    void lazyLoad() override;
};

} // namespace darkeye
