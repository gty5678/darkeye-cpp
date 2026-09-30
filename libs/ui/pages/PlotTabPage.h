#pragma once

#include "darkeye_ui/base/LazyWidget.h"

#include <QSqlDatabase>

namespace darkeye
{

class PlotTabPage final : public LazyWidget
{
public:
    explicit PlotTabPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                         QWidget *parent = nullptr);

    void refresh();

private:
    void lazyLoad() override;

    QSqlDatabase m_publicDatabase;
    QSqlDatabase m_privateDatabase;
};

} // namespace darkeye
