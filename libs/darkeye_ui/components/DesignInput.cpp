#include "darkeye_ui/components/DesignInput.h"

namespace darkeye {

DesignLineEdit::DesignLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setObjectName(QStringLiteral("DesignInput"));
}

DesignTextEdit::DesignTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setObjectName(QStringLiteral("DesignTextEdit"));
}

DesignPlainTextEdit::DesignPlainTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("DesignPlainTextEdit"));
}

} // namespace darkeye
