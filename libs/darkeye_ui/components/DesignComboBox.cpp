#include "darkeye_ui/components/DesignComboBox.h"

#include <QAbstractItemView>

namespace darkeye {

DesignComboBox::DesignComboBox(QWidget *parent) : QComboBox(parent)
{
    setObjectName(QStringLiteral("DesignComboBox"));
    view()->setObjectName(QStringLiteral("DesignComboBoxPopup"));
}

} // namespace darkeye
