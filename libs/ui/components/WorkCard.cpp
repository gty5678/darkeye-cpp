#include "ui/components/WorkCard.h"

#include "ui/components/AsyncImageLabel.h"
#include "ui/components/ClickableLabel.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace {

QColor cardColor(int tagId)
{
    switch (tagId) {
    case 1:
        return QColor(QStringLiteral("#80B0F8"));
    case 2:
        return QColor(QStringLiteral("#FFA475"));
    case 3:
        return QColor(QStringLiteral("#FFEB28"));
    default:
        return QColor(Qt::transparent);
    }
}

} // namespace

namespace darkeye {

WorkCard::WorkCard(const WorkSummary &work, const QString &coverDirectory,
                   bool largeCoverView, QWidget *parent)
    : QWidget(parent), m_workId(work.id), m_backgroundColor(cardColor(work.highlightTagId)),
      m_largeCoverView(largeCoverView)
{
    setObjectName(QStringLiteral("WorkCard"));
    setProperty("workId", work.id);
    const int cardWidth = largeCoverView ? 250 : 220;
    const int imageWidth = largeCoverView ? 240 : 210;
    const int imageHeight = largeCoverView ? 162 : (work.standard ? 300 : 120);
    setFixedWidth(cardWidth);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(QStringLiteral("%1 %2").arg(work.serialNumber, work.chineseTitle));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, largeCoverView ? 6 : 10, 5,
                               largeCoverView ? 8 : 20);
    layout->setSpacing(largeCoverView ? 4 : 6);
    auto *serial = new ClickableLabel(work.serialNumber, false, this);
    serial->setStyleSheet(QStringLiteral(
        "font-size: 16px; font-family: 'Microsoft YaHei'; font-weight: bold;"));
    serial->ensurePolished();
    serial->setAlignment(Qt::AlignCenter);
    if (largeCoverView) {
        serial->setFixedSize(240, 22);
    }
    layout->addWidget(serial);

    auto *cover = new AsyncImageLabel(this);
    cover->setFixedSize(imageWidth, imageHeight);
    cover->setFitMode(largeCoverView ? ImageFitMode::Contain
                                     : (work.standard ? ImageFitMode::RightCover
                                                      : ImageFitMode::Contain));
    QString imagePath = work.imageUrl.trimmed();
    if (!imagePath.isEmpty() && !QFileInfo(imagePath).isAbsolute()) {
        imagePath = QDir(coverDirectory).filePath(imagePath);
    }
    cover->setSource(imagePath);
    cover->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(cover, 0, Qt::AlignCenter);

    QString displayedTitle = work.chineseTitle;
    if (largeCoverView) {
        displayedTitle = displayedTitle.left(40);
    }
    auto *title = new QLabel(displayedTitle, this);
    title->setStyleSheet(QStringLiteral(
        "font-size: 14px; font-family: 'Microsoft YaHei'; font-weight: bold;"));
    title->ensurePolished();
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true);
    title->setFixedWidth(largeCoverView ? 240 : 210);
    if (largeCoverView) {
        title->setFixedHeight(50);
    } else {
        title->setMaximumHeight(48);
    }
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(title);

    if (largeCoverView) {
        setFixedHeight(248);
    } else {
        const int titleHeight = displayedTitle.isEmpty() ? 0
            : QFontMetrics(title->font())
                  .boundingRect(QRect(0, 0, 210, 10000),
                                Qt::TextWordWrap | Qt::AlignCenter,
                                displayedTitle)
                  .height();
        title->setFixedHeight(titleHeight);
        const int bottomMargin = displayedTitle.isEmpty() ? 33 : 20;
        layout->setContentsMargins(5, 10, 5, bottomMargin);
        setFixedHeight(10 + serial->sizeHint().height() + 6 + imageHeight + 6
                       + titleHeight + bottomMargin);
    }
}

qint64 WorkCard::workId() const noexcept
{
    return m_workId;
}

void WorkCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setFocus();
        emit activated(m_workId);
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        emit editRequested(m_workId);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void WorkCard::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit activated(m_workId);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void WorkCard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    if (m_largeCoverView) {
        painter.fillRect(rect(), m_backgroundColor);
        painter.end();
        QWidget::paintEvent(event);
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal cut = width() * 0.15;
    QPainterPath path;
    path.moveTo(cut, 0);
    path.lineTo(width() - cut, 0);
    path.lineTo(width(), cut);
    path.lineTo(width(), height() - cut);
    path.lineTo(width() - cut, height());
    path.lineTo(cut, height());
    path.lineTo(0, height() - cut);
    path.lineTo(0, cut);
    path.closeSubpath();
    painter.fillPath(path, m_backgroundColor);
    painter.end();
    QWidget::paintEvent(event);
}

} // namespace darkeye
