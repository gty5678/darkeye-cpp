#include "ui/components/PersonCard.h"

#include "darkeye_ui/components/OctImage.h"
#include "ui/components/ClickableLabel.h"

#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace darkeye
{

PersonCard::PersonCard(qint64 personId, const QString &name, const QString &imagePath,
                       const QString &imageDirectory, QWidget *parent)
    : QWidget(parent), m_personId(personId),
      m_avatar(new OctImage(imagePath, imageDirectory, 150, true, this)),
      m_nameLabel(new ClickableLabel(name, false, this))
{
    setObjectName(QStringLiteral("PersonCard"));
    setFixedWidth(150);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(name);

    m_avatar->setObjectName(QStringLiteral("PersonCardAvatar"));
    m_nameLabel->setObjectName(QStringLiteral("PersonCardName"));
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(false);
    QFont font = m_nameLabel->font();
    font.setPointSize(12);
    font.setBold(true);
    m_nameLabel->setFont(font);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(m_avatar);
    layout->addWidget(m_nameLabel, 0, Qt::AlignCenter);

    installEventFilter(this);
    m_avatar->installEventFilter(this);
}

qint64 PersonCard::personId() const noexcept
{
    return m_personId;
}

QString PersonCard::name() const
{
    return m_nameLabel->text();
}

OctImage *PersonCard::avatar() const
{
    return m_avatar;
}

void PersonCard::updateData(qint64 personId, const QString &name, const QString &imagePath)
{
    m_personId = personId;
    m_nameLabel->setText(name);
    setAccessibleName(name);
    m_avatar->updateImage(imagePath);
}

bool PersonCard::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == this || watched == m_avatar) &&
        event->type() == QEvent::MouseButtonRelease)
    {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            emit activated(m_personId);
            return true;
        }
        if (mouseEvent->button() == Qt::RightButton)
        {
            emit editRequested(m_personId);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace darkeye

