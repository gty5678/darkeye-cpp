#pragma once

#include "darkeye_ui/theme/ThemeService.h"
#include "database/repositories/PersonRepository.h"
#include "database/repositories/PrivateRepository.h"

#include <QWidget>
#include <optional>

namespace darkeye
{

class PersonInfoPanel;

class PersonDetailPage final : public QWidget
{
    Q_OBJECT

public:
    explicit PersonDetailPage(PersonKind kind, QSqlDatabase publicDatabase,
                              QSqlDatabase privateDatabase, ThemeService &themes,
                              QString imageDirectory = {}, QWidget *parent = nullptr,
                              QString coverDirectory = {});

    bool showPerson(qint64 personId);
    [[nodiscard]] qint64 currentPersonId() const noexcept;
    [[nodiscard]] PersonKind kind() const noexcept;

signals:
    void editRequested(PersonKind kind, qint64 personId);
    void workRequested(qint64 workId);
    void favoriteChanged(qint64 personId, bool favorite);

private:
    void toggleFavorite(bool favorite);

    PersonKind m_kind;
    PersonRepository m_repository;
    PrivateRepository m_privateRepository;
    ThemeService &m_themes;
    std::optional<PersonDetails> m_details;
    PersonInfoPanel *m_panel = nullptr;
};

} // namespace darkeye

