#include "darkeye_ui/components/DesignButton.h"

#include <QStyle>

namespace darkeye {

DesignButton::DesignButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
    setObjectName(QStringLiteral("DesignButton"));
    setVariant(QStringLiteral("default"));
}

void DesignButton::setVariant(const QString &variant)
{
    setProperty("variant", variant.trimmed().isEmpty() ? QStringLiteral("default")
                                                       : variant.trimmed());
    style()->unpolish(this);
    style()->polish(this);
    update();
}

QString DesignButton::variant() const
{
    return property("variant").toString();
}

} // namespace darkeye
