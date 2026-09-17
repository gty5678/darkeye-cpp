#pragma once

#include "services/WorkMaintenanceService.h"

#include <QUrl>
#include <QWidget>

class QPushButton;

namespace darkeye
{

class ThemeService;
class TopActressSyncService;

class WorkMaintenanceWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WorkMaintenanceWidget(QSqlDatabase database, ThemeService &themes,
                                   QString coverDirectory = {},
                                   QUrl topActressesEndpoint = QUrl(QStringLiteral(
                                       "http://127.0.0.1:56790/api/v1/top-actresses")),
                                   QWidget *parent = nullptr);

signals:
    void worksChanged();
    void actressesChanged();

private:
    void assignMakers();
    void normalizeCovers();
    void updatePopularActresses();

    WorkMaintenanceService m_service;
    ThemeService &m_themes;
    TopActressSyncService *m_topActresses = nullptr;
    QPushButton *m_popularButton = nullptr;
};

} // namespace darkeye
