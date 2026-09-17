#pragma once

#include <QPushButton>

namespace darkeye {

class DesignButton : public QPushButton
{
public:
    explicit DesignButton(const QString &text = {}, QWidget *parent = nullptr);

    void setVariant(const QString &variant);
    QString variant() const;
};

using Button = DesignButton;

} // namespace darkeye
