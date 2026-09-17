#pragma once

#include <QWidget>

namespace darkeye
{

class JsonTransferBar final : public QWidget
{
    Q_OBJECT

public:
    explicit JsonTransferBar(QString defaultFileName, QWidget *parent = nullptr);

signals:
    void importRequested(const QString &path);
    void exportRequested(const QString &path);

private:
    QString m_defaultFileName;
};

} // namespace darkeye
