#pragma once

#include <QComboBox>

namespace darkeye {

class DesignComboBox : public QComboBox
{
public:
    explicit DesignComboBox(QWidget *parent = nullptr);
};

using ComboBox = DesignComboBox;

} // namespace darkeye
