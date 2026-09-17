#pragma once

#include "domain/Person.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace darkeye
{

class HeartLabel;
class IconButton;
class OctImage;
class ThemeService;
class ActressWorkTimeline;
class RadarChartWidget;

class PersonInfoPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PersonInfoPanel(ThemeService &themes, QString imageDirectory = {},
                             QWidget *parent = nullptr, QString coverDirectory = {});

    void setDetails(const PersonDetails &details, bool favorite = false);
    [[nodiscard]] qint64 personId() const noexcept;
    [[nodiscard]] PersonKind kind() const noexcept;

signals:
    void favoriteChanged(bool favorite);
    void editRequested(qint64 personId);
    void workRequested(qint64 workId);
    void actressExternalSearchRequested(const QString &name);

private:
    void rebuildNames(const PersonDetails &details);
    void rebuildFacts(const PersonDetails &details);
    void rebuildWorks(const PersonDetails &details);
    void rebuildRadar(const PersonDetails &details);

    ThemeService &m_themes;
    QString m_imageDirectory;
    PersonKind m_kind = PersonKind::Actress;
    qint64 m_personId = 0;
    OctImage *m_avatar = nullptr;
    QWidget *m_names = nullptr;
    QWidget *m_facts = nullptr;
    HeartLabel *m_heart = nullptr;
    IconButton *m_edit = nullptr;
    QLabel *m_notes = nullptr;
    QTableWidget *m_aliases = nullptr;
    QTableWidget *m_works = nullptr;
    ActressWorkTimeline *m_timeline = nullptr;
    RadarChartWidget *m_radar = nullptr;
};

} // namespace darkeye
