#pragma once

#include "darkeye_ui/base/LazyWidget.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/repositories/PrivateRepository.h"
#include "database/repositories/WorkRepository.h"

#include <optional>

class QHBoxLayout;
class QVBoxLayout;

namespace darkeye {

class HeartLabel;
class TokenVLabel;
class VerticalTextLabel;

class WorkDetailPage final : public LazyWidget
{
    Q_OBJECT

public:
    explicit WorkDetailPage(QSqlDatabase publicDatabase,
                            QSqlDatabase privateDatabase,
                            ThemeService &themes,
                            QString coverDirectory = {},
                            QWidget *parent = nullptr);

    bool showWork(qint64 workId);
    qint64 currentWorkId() const;

signals:
    void editRequested(qint64 workId);
    void workDeleted(qint64 workId);
    void favoriteChanged(qint64 workId);
    void fanartRequested(qint64 workId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void lazyLoad() override;
    void buildUi();
    void applyDetails(const WorkDetails &details);
    void rebuildPeople(const WorkDetails &details);
    void rebuildTags(const WorkDetails &details);
    void toggleFavorite(bool favorite);
    void deleteCurrentWork();
    void playCurrentWork();

    ThemeService &m_themes;
    WorkRepository m_repository;
    PrivateRepository m_privateRepository;
    QString m_coverDirectory;
    std::optional<WorkDetails> m_details;
    QWidget *m_backdrop = nullptr;
    QHBoxLayout *m_rootLayout = nullptr;
    HeartLabel *m_heart = nullptr;
    QWidget *m_people = nullptr;
    QWidget *m_tags = nullptr;
    QVBoxLayout *m_studioLayout = nullptr;
    TokenVLabel *m_serial = nullptr;
    TokenVLabel *m_releaseDate = nullptr;
    TokenVLabel *m_director = nullptr;
    TokenVLabel *m_maker = nullptr;
    VerticalTextLabel *m_story = nullptr;
    VerticalTextLabel *m_title = nullptr;
};

} // namespace darkeye
