#pragma once

#include "database/repositories/ReferenceRepository.h"
#include "darkeye_ui/base/LazyWidget.h"

#include <QUrl>

namespace darkeye
{

class ThemeService;

class ManagementPage final : public LazyWidget
{
    Q_OBJECT

public:
    explicit ManagementPage(
        QSqlDatabase database, ThemeService &themes, QString coverDirectory = {},
        QString fanartDirectory = {},
        QUrl imageFetchEndpoint = QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/image")),
        QUrl topActressesEndpoint =
            QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/top-actresses")),
        QWidget *parent = nullptr);

signals:
    void referencesChanged(ReferenceKind kind);
    void tagsChanged();
    void worksChanged();
    void actressesChanged();
    void workRequested(qint64 workId);
    void actressRequested(qint64 actressId);

private:
    void lazyLoad() override;

    QSqlDatabase m_database;
    ThemeService &m_themes;
    QString m_coverDirectory;
    QString m_fanartDirectory;
    QUrl m_imageFetchEndpoint;
    QUrl m_topActressesEndpoint;
};

} // namespace darkeye
