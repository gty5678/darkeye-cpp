#include "darkeye_ui/components/TokenControls.h"

#include <QTabBar>

namespace darkeye {

TokenCheckBox::TokenCheckBox(const QString &text, QWidget *parent)
    : QCheckBox(text, parent)
{
    setObjectName(QStringLiteral("DesignCheckBox"));
}

TokenRadioButton::TokenRadioButton(const QString &text, QWidget *parent)
    : QRadioButton(text, parent)
{
    setObjectName(QStringLiteral("DesignRadioButton"));
}

TokenSpinBox::TokenSpinBox(QWidget *parent) : QSpinBox(parent)
{
    setObjectName(QStringLiteral("DesignSpinBox"));
}

TokenGroupBox::TokenGroupBox(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
{
    setObjectName(QStringLiteral("DesignGroupBox"));
}

TokenTabWidget::TokenTabWidget(QWidget *parent) : QTabWidget(parent)
{
    setObjectName(QStringLiteral("DesignTabWidget"));
    tabBar()->setObjectName(QStringLiteral("DesignTabBar"));
}

ProgressBar::ProgressBar(QWidget *parent) : QProgressBar(parent)
{
    setObjectName(QStringLiteral("DesignProgressBar"));
    setTextVisible(true);
}

IndeterminateProgressBar::IndeterminateProgressBar(QWidget *parent)
    : ProgressBar(parent)
{
    start();
}

void IndeterminateProgressBar::start() { setRange(0, 0); }

void IndeterminateProgressBar::stop(int value)
{
    setRange(0, 100);
    setValue(qBound(0, value, 100));
}

TransparentWidget::TransparentWidget(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("TransparentWidget"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(false);
}

} // namespace darkeye
