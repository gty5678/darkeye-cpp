#pragma once

#include "domain/Work.h"

#include <QColor>
#include <QWidget>

namespace darkeye {

class WorkCard final : public QWidget
{
    Q_OBJECT

public:
    explicit WorkCard(const WorkSummary &work, const QString &coverDirectory,
                      bool largeCoverView = false,
                      QWidget *parent = nullptr);

    [[nodiscard]] qint64 workId() const noexcept;

signals:
    void activated(qint64 workId);
    void editRequested(qint64 workId);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    qint64 m_workId = 0;
    QColor m_backgroundColor;
    bool m_largeCoverView = false;
};

} // namespace darkeye
