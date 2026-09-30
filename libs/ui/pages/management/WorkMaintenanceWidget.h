#pragma once

#include "services/BatchTranslationService.h"
#include "services/WorkMaintenanceService.h"

#include <QUrl>
#include <QWidget>
#include <QList>

class QPushButton;

namespace darkeye
{

class ThemeService;
class TopActressSyncService;
class ActressSyncService;

class WorkMaintenanceWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WorkMaintenanceWidget(QSqlDatabase database, ThemeService &themes,
                                   QString coverDirectory = {},
                                   QString actressImageDirectory = {},
                                   QUrl imageFetchEndpoint = QUrl(QStringLiteral(
                                       "http://127.0.0.1:56790/api/v1/image")),
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
    void updateNeededActresses();
    void translateMissingFields();
    void forceTranslateFields();
    void syncNextActress();

    QSqlDatabase m_database;
    WorkMaintenanceService m_service;
    ThemeService &m_themes;
    QString m_actressImageDirectory;
    QUrl m_imageFetchEndpoint;
    TopActressSyncService *m_topActresses = nullptr;
    ActressSyncService *m_actressSync = nullptr;
    BatchTranslationService *m_translations = nullptr;
    QPushButton *m_popularButton = nullptr;
    QPushButton *m_actressButton = nullptr;
    QPushButton *m_translateButton = nullptr;
    QPushButton *m_forceTranslateButton = nullptr;
    QList<qint64> m_pendingActressIds;
    int m_syncedActresses = 0;
    int m_failedActresses = 0;
    BatchTranslationMode m_translationMode = BatchTranslationMode::FillMissing;
};

} // namespace darkeye
