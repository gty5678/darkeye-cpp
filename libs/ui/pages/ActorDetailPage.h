#pragma once

#include "ui/pages/PersonDetailPage.h"

#include <utility>

namespace darkeye
{

class ActorDetailPage final : public PersonDetailPage
{
public:
    ActorDetailPage(QSqlDatabase publicDatabase, QSqlDatabase privateDatabase,
                    ThemeService &themes, QString imageDirectory = {},
                    QWidget *parent = nullptr, QString coverDirectory = {})
        : PersonDetailPage(PersonKind::Actor, std::move(publicDatabase),
                           std::move(privateDatabase), themes, std::move(imageDirectory), parent,
                           std::move(coverDirectory))
    {
    }
};

} // namespace darkeye
