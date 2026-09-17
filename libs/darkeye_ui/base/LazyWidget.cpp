#include "darkeye_ui/base/LazyWidget.h"

#include <QShowEvent>

namespace darkeye
{

LazyWidget::LazyWidget(QWidget *parent) : QWidget(parent)
{
}

bool LazyWidget::isInitialized() const noexcept
{
    return m_initialized;
}

void LazyWidget::initialize()
{
    if (m_initialized)
    {
        return;
    }
    lazyLoad();
    m_initialized = true;
}

void LazyWidget::showEvent(QShowEvent *event)
{
    initialize();
    QWidget::showEvent(event);
}

} // namespace darkeye
