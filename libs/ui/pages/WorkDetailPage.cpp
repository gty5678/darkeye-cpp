#include "ui/pages/WorkDetailPage.h"

#include "darkeye_ui/components/HeartLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/VerticalText.h"
#include "darkeye_ui/layouts/VerticalFlowLayout.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>
#include <QUrl>
#include <QVBoxLayout>

namespace {

class WorkBackdrop final : public QWidget
{
public:
    using QWidget::QWidget;

    void setCover(const QString &path)
    {
        m_cover = QImage(path);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#141414")));
        if (!m_cover.isNull()) {
            const QImage scaled = m_cover.scaledToHeight(height(),
                                                         Qt::SmoothTransformation);
            painter.setOpacity(0.7);
            painter.drawImage(width() - scaled.width(), 0, scaled);
            painter.setOpacity(1.0);
        }
        QLinearGradient shade(width() - height() * 1.5, 0, width(), 0);
        shade.setColorAt(0.0, QColor(20, 20, 20, 255));
        shade.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(rect(), shade);
        QLinearGradient edge(width(), 0, width() - qMax(1, height() / 10), 0);
        edge.setColorAt(0.0, QColor(20, 20, 20, 255));
        edge.setColorAt(1.0, QColor(20, 20, 20, 0));
        painter.fillRect(rect(), edge);
    }

private:
    QImage m_cover;
};

darkeye::TokenVLabel *token(const QString &text, darkeye::ThemeService &themes,
                            QWidget *parent)
{
    auto *label = new darkeye::TokenVLabel(text, &themes, parent);
    label->setColors(Qt::transparent, Qt::white, QColor(QStringLiteral("#8fc7ff")));
    return label;
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) widget->deleteLater();
        delete item;
    }
}

} // namespace

namespace darkeye {

WorkDetailPage::WorkDetailPage(QSqlDatabase publicDatabase,
                               QSqlDatabase privateDatabase,
                               ThemeService &themes, QString coverDirectory,
                               QWidget *parent)
    : QWidget(parent), m_themes(themes),
      m_repository(std::move(publicDatabase)),
      m_privateRepository(std::move(privateDatabase)),
      m_coverDirectory(std::move(coverDirectory))
{
    setObjectName(QStringLiteral("WorkDetailPage"));
    setAttribute(Qt::WA_StyledBackground, true);
    buildUi();
}

void WorkDetailPage::buildUi()
{
    m_backdrop = new WorkBackdrop(this);
    m_backdrop->setObjectName(QStringLiteral("WorkDetailBackdrop"));
    m_backdrop->lower();

    m_rootLayout = new QHBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);
    m_rootLayout->addStretch();

    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("WorkDetailContent"));
    content->setFixedHeight(550);
    content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(5);
    contentLayout->addStretch();

    auto *tools = new QWidget(content);
    tools->setFixedHeight(550);
    auto *toolLayout = new QVBoxLayout(tools);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    m_heart = new HeartLabel(tools);
    auto *trash = new IconButton(QStringLiteral("trash_2"), &m_themes, tools);
    auto *modify = new IconButton(QStringLiteral("square_pen"), &m_themes, tools);
    auto *watch = new IconButton(QStringLiteral("tv"), &m_themes, tools);
    for (IconButton *button : {trash, modify, watch}) {
        button->setInverted(true);
        button->setButtonPixelSize(32);
    }
    trash->setToolTip(QStringLiteral("标记删除作品"));
    modify->setToolTip(QStringLiteral("修改作品"));
    watch->setToolTip(QStringLiteral("播放本地视频"));
    toolLayout->addWidget(m_heart, 0, Qt::AlignHCenter);
    toolLayout->addWidget(trash, 0, Qt::AlignHCenter);
    toolLayout->addWidget(modify, 0, Qt::AlignHCenter);
    toolLayout->addWidget(watch, 0, Qt::AlignHCenter);
    toolLayout->addStretch();
    contentLayout->addWidget(tools);

    m_tags = new QWidget(content);
    m_tags->setFixedHeight(550);
    m_tags->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    new VerticalFlowLayout(m_tags, 0, 5);
    contentLayout->addWidget(m_tags);

    m_people = new QWidget(content);
    m_people->setFixedHeight(550);
    m_people->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    new VerticalFlowLayout(m_people, 0, 5);
    contentLayout->addWidget(m_people);

    auto *studio = new QWidget(content);
    studio->setFixedHeight(550);
    m_studioLayout = new QVBoxLayout(studio);
    m_studioLayout->setContentsMargins(0, 0, 0, 0);
    m_director = token(QStringLiteral("----"), m_themes, studio);
    m_maker = token(QStringLiteral("----"), m_themes, studio);
    m_studioLayout->addWidget(token(QStringLiteral("导演"), m_themes, studio));
    m_studioLayout->addWidget(m_director);
    m_studioLayout->addWidget(token(QStringLiteral("制作商"), m_themes, studio));
    m_studioLayout->addWidget(m_maker);
    m_studioLayout->addStretch();
    contentLayout->addWidget(studio);

    m_story = new VerticalTextLabel({}, QStringLiteral("inverse"), &m_themes, content);
    m_story->setFixedHeight(550);
    m_story->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QFont storyFont(QStringLiteral("Microsoft YaHei"), 12);
    m_story->setFont(storyFont);
    m_story->setTextColor(Qt::white);
    contentLayout->addWidget(m_story, 0, Qt::AlignTop);

    auto *identity = new QWidget(content);
    identity->setFixedHeight(550);
    auto *identityLayout = new QVBoxLayout(identity);
    identityLayout->setContentsMargins(0, 0, 0, 0);
    m_serial = token(QStringLiteral("----"), m_themes, identity);
    m_releaseDate = token(QStringLiteral("----"), m_themes, identity);
    identityLayout->addWidget(token(QStringLiteral("番号"), m_themes, identity));
    identityLayout->addWidget(m_serial);
    identityLayout->addWidget(token(QStringLiteral("发行日期"), m_themes, identity));
    identityLayout->addWidget(m_releaseDate);
    identityLayout->addStretch();
    contentLayout->addWidget(identity);

    m_title = new VerticalTextLabel({}, QStringLiteral("inverse"), &m_themes, content);
    m_title->setFixedHeight(550);
    m_title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QFont titleFont(QStringLiteral("Microsoft YaHei"), 24);
    m_title->setFont(titleFont);
    m_title->setTextColor(Qt::white);
    contentLayout->addWidget(m_title, 0, Qt::AlignTop);
    m_rootLayout->addWidget(content, 0, Qt::AlignVCenter);

    connect(m_heart, &HeartLabel::clicked, this, &WorkDetailPage::toggleFavorite);
    connect(trash, &QPushButton::clicked, this, &WorkDetailPage::deleteCurrentWork);
    connect(modify, &QPushButton::clicked, this, [this] {
        if (m_details.has_value()) emit editRequested(m_details->work.id);
    });
    connect(watch, &QPushButton::clicked, this, &WorkDetailPage::playCurrentWork);
}

bool WorkDetailPage::showWork(qint64 workId)
{
    QString errorMessage;
    const std::optional<WorkDetails> details =
        m_repository.findDetailsById(workId, &errorMessage);
    if (!details.has_value()) {
        ToastNotification::showMessage(window(),
            errorMessage.isEmpty() ? QStringLiteral("作品不存在") : errorMessage,
            ToastNotification::Level::Error, 3500, &m_themes);
        return false;
    }
    m_details = details;
    applyDetails(*details);
    m_heart->setState(m_privateRepository.isFavoriteWork(workId));
    return true;
}

qint64 WorkDetailPage::currentWorkId() const
{
    return m_details.has_value() ? m_details->work.id : 0;
}

void WorkDetailPage::applyDetails(const WorkDetails &details)
{
    const Work &work = details.work;
    const QString title = work.chineseTitle.size() <= 35
        ? work.chineseTitle : work.chineseTitle.left(35) + QStringLiteral("...");
    const QString story = work.chineseStory.size() <= 120
        ? work.chineseStory : work.chineseStory.left(120) + QStringLiteral("...");
    m_title->setText(title);
    m_story->setText(story);
    m_serial->setTextDynamic(work.serialNumber.isEmpty() ? QStringLiteral("----")
                                                          : work.serialNumber);
    m_releaseDate->setTextDynamic(work.releaseDate.isEmpty()
        ? QStringLiteral("----") : work.releaseDate);
    m_director->setTextDynamic(work.director.isEmpty() ? QStringLiteral("----")
                                                        : work.director);
    m_maker->setTextDynamic(details.makerName.isEmpty() ? QStringLiteral("----")
                                                        : details.makerName);
    rebuildPeople(details);
    rebuildTags(details);
    QString coverPath = work.imageUrl.trimmed();
    if (!coverPath.isEmpty() && !QFileInfo(coverPath).isAbsolute()) {
        coverPath = QDir(m_coverDirectory).filePath(coverPath);
    }
    static_cast<WorkBackdrop *>(m_backdrop)->setCover(coverPath);
}

void WorkDetailPage::rebuildPeople(const WorkDetails &details)
{
    QLayout *layout = m_people->layout();
    clearLayout(layout);
    if (!details.actresses.isEmpty()) {
        layout->addWidget(token(QStringLiteral("女优"), m_themes, m_people));
        for (const WorkPersonReference &person : details.actresses) {
            auto *label = token(person.name, m_themes, m_people);
            label->setColors(Qt::white, Qt::black,
                             QColor(QStringLiteral("#8fc7ff")));
            layout->addWidget(label);
        }
    }
    if (!details.actors.isEmpty()) {
        layout->addWidget(token(QStringLiteral("男优"), m_themes, m_people));
        for (const WorkPersonReference &person : details.actors) {
            auto *label = token(person.name, m_themes, m_people);
            label->setColors(Qt::white, Qt::black,
                             QColor(QStringLiteral("#8fc7ff")));
            layout->addWidget(label);
        }
    }
}

void WorkDetailPage::rebuildTags(const WorkDetails &details)
{
    QLayout *layout = m_tags->layout();
    clearLayout(layout);
    layout->addWidget(token(QStringLiteral("作品标签"), m_themes, m_tags));
    for (const TagOption &tag : details.tags) {
        auto *label = token(tag.name, m_themes, m_tags);
        const QColor background(tag.color);
        if (background.isValid()) {
            label->setColors(background, background.lightness() < 135
                ? QColor(Qt::white) : QColor(Qt::black));
        }
        label->setToolTip(tag.detail);
        layout->addWidget(label);
    }
}

void WorkDetailPage::toggleFavorite(bool favorite)
{
    if (!m_details.has_value()) return;
    QString errorMessage;
    const bool success = favorite
        ? m_privateRepository.addFavoriteWork(m_details->work.id,
                                              m_details->work.serialNumber,
                                              &errorMessage)
        : m_privateRepository.removeFavoriteWork(m_details->work.id, &errorMessage);
    if (!success) {
        m_heart->setState(!favorite);
        ToastNotification::showMessage(window(), errorMessage,
            ToastNotification::Level::Error, 3500, &m_themes);
    }
}

void WorkDetailPage::deleteCurrentWork()
{
    if (!m_details.has_value()) return;
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要将该作品标记为删除吗？"))
        != QMessageBox::Yes) return;
    QString errorMessage;
    if (!m_repository.setDeleted(m_details->work.id, true, &errorMessage)) {
        ToastNotification::showMessage(window(), errorMessage,
            ToastNotification::Level::Error, 3500, &m_themes);
        return;
    }
    const qint64 workId = m_details->work.id;
    emit workDeleted(workId);
}

void WorkDetailPage::playCurrentWork()
{
    if (!m_details.has_value()) return;
    const QString path = m_details->work.videoUrl.trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        ToastNotification::showMessage(window(), QStringLiteral("没有可播放的本地视频"),
            ToastNotification::Level::Info, 2500, &m_themes);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void WorkDetailPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_backdrop->setGeometry(rect());
    m_rootLayout->setContentsMargins(0, 0, qRound(height() * 0.8), 0);
}

} // namespace darkeye


