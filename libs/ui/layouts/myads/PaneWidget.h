#pragma once

#include "ui/layouts/myads/DockTheme.h"

#include <QIcon>
#include <QHash>
#include <QPoint>
#include <QWidget>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QStackedWidget;
class QTabBar;

namespace darkeye::myads {

struct PaneContent
{
    QString contentId;
    QString title;
    QWidget *widget = nullptr;
    QIcon icon;
    bool closeable = true;
    int index = -1;
    QString currentContentId;
};

class PaneWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit PaneWidget(const QString &paneId, QWidget *parent = nullptr);

    [[nodiscard]] QString paneId() const;
    void applyTheme(const DockTheme &theme);
    void setIconOnly(bool iconOnly);
    [[nodiscard]] bool iconOnly() const noexcept;
    bool addContent(const QString &contentId, const QString &title, QWidget *widget,
                    const QIcon &icon = {}, bool closeable = true, int index = -1);
    [[nodiscard]] PaneContent takeContent(const QString &contentId);
    bool restoreContent(const PaneContent &content);
    bool removeContent(const QString &contentId);
    [[nodiscard]] QStringList contentIds() const;
    [[nodiscard]] int contentCount() const;
    [[nodiscard]] QString currentContentId() const;
    bool setCurrentContentId(const QString &contentId);
    [[nodiscard]] QString contentTitle(const QString &contentId) const;
    [[nodiscard]] QWidget *contentWidget(const QString &contentId) const;
    [[nodiscard]] QIcon contentIcon(const QString &contentId) const;
    [[nodiscard]] bool isContentCloseable(const QString &contentId) const;

signals:
    void paneEmpty(darkeye::myads::PaneWidget *pane);
    void dragMoved(darkeye::myads::PaneWidget *target, const QPoint &globalPosition);
    void tabDropped(darkeye::myads::PaneWidget *target, const QPoint &globalPosition,
                    const QString &sourcePaneId, const QString &contentId);
    void dragFinished();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:
    [[nodiscard]] int indexOf(const QString &contentId) const;
    void updateCloseButton(int index);

    QString m_paneId;
    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    bool m_iconOnly = false;
    QHash<QString, QString> m_titles;
    QHash<QString, bool> m_closeable;
    DockTheme m_theme;
};

inline constexpr auto MyAdsTabMimeType = "application/x-darkeye-myads-tab";

} // namespace darkeye::myads


