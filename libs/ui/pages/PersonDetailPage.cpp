#include "ui/pages/PersonDetailPage.h"

#include "darkeye_ui/components/ToastNotification.h"
#include "ui/components/PersonInfoPanel.h"

#include <QScrollArea>
#include <QDesktopServices>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace darkeye
{

PersonDetailPage::PersonDetailPage(PersonKind kind, QSqlDatabase publicDatabase,
                                   QSqlDatabase privateDatabase, ThemeService &themes,
                                   QString imageDirectory, QWidget *parent,
                                   QString coverDirectory)
    : QWidget(parent), m_kind(kind), m_repository(std::move(publicDatabase)),
      m_privateRepository(std::move(privateDatabase)), m_themes(themes)
{
    setObjectName(kind == PersonKind::Actress ? QStringLiteral("ActressDetailPage")
                                              : QStringLiteral("ActorDetailPage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("PersonDetailScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_panel = new PersonInfoPanel(m_themes, std::move(imageDirectory), scroll,
                                  std::move(coverDirectory));
    scroll->setWidget(m_panel);
    root->addWidget(scroll);

    connect(m_panel, &PersonInfoPanel::favoriteChanged, this, &PersonDetailPage::toggleFavorite);
    connect(m_panel, &PersonInfoPanel::editRequested, this,
            [this](qint64 personId) { emit editRequested(m_kind, personId); });
    connect(m_panel, &PersonInfoPanel::workRequested, this, &PersonDetailPage::workRequested);
    connect(m_panel, &PersonInfoPanel::actressExternalSearchRequested, this,
            [](const QString &name) {
        QUrl url(QStringLiteral("https://www.minnano-av.com/search_result.php"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("search_scope"), QStringLiteral("actress"));
        query.addQueryItem(QStringLiteral("search_word"), name);
        query.addQueryItem(QStringLiteral("search"), QStringLiteral(" Go"));
        url.setQuery(query);
        QDesktopServices::openUrl(url);
    });
}

bool PersonDetailPage::showPerson(qint64 personId)
{
    QString errorMessage;
    const auto details = m_repository.findDetails(m_kind, personId, &errorMessage);
    if (!details.has_value())
    {
        Toast::showError(window(),
                         errorMessage.isEmpty() ? QStringLiteral("人物不存在") : errorMessage,
                         &m_themes);
        return false;
    }
    m_details = details;
    const bool favorite = m_kind == PersonKind::Actress &&
                          m_privateRepository.isFavoriteActress(personId, &errorMessage);
    m_panel->setDetails(*details, favorite);
    return true;
}

qint64 PersonDetailPage::currentPersonId() const noexcept
{
    return m_details.has_value() ? m_details->id : 0;
}

PersonKind PersonDetailPage::kind() const noexcept
{
    return m_kind;
}

void PersonDetailPage::toggleFavorite(bool favorite)
{
    if (m_kind != PersonKind::Actress || !m_details.has_value())
        return;
    QString errorMessage;
    bool success = false;
    if (favorite)
    {
        const QString japaneseName =
            m_details->names.isEmpty() ? QString() : m_details->names.first().japanese.trimmed();
        if (japaneseName.isEmpty())
        {
            errorMessage = QStringLiteral("收藏女演员前需要填写日文名");
        }
        else
        {
            success =
                m_privateRepository.addFavoriteActress(m_details->id, japaneseName, &errorMessage);
        }
    }
    else
    {
        success = m_privateRepository.removeFavoriteActress(m_details->id, &errorMessage);
    }
    if (!success)
    {
        m_panel->setDetails(*m_details, !favorite);
        Toast::showError(window(), errorMessage, &m_themes);
        return;
    }
    emit favoriteChanged(m_details->id, favorite);
}

} // namespace darkeye
