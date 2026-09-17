#pragma once

#include <QPoint>
#include <QStringList>
#include <QSize>

namespace darkeye
{

struct AppSettings final
{
    QString themeId = QStringLiteral("LIGHT");
    QString customPrimary;
    bool greenMode = false;

    bool firstLaunch = true;
    bool maximized = false;
    QSize windowSize{800, 600};
    QPoint windowPosition{100, 100};

    bool workLargeCoverView = false;
    bool workTagSelectorVisible = true;
    bool shelfTagSelectorVisible = true;

    QString localVideoPlayer;
    QStringList videoPaths;
};

} // namespace darkeye
