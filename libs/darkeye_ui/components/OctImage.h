#pragma once

#include <QImage>
#include <QLabel>

namespace darkeye {

class OctImage final : public QLabel
{
    Q_OBJECT

public:
    explicit OctImage(const QString &imagePath = {}, const QString &basePath = {},
                      int diameter = 150, bool shadow = true,
                      QWidget *parent = nullptr);
    QString source() const;
    void updateImage(const QString &imagePath);

signals:
    void imageReady(const QImage &image);
    void imageLoaded(const QString &path);

private:
    QString resolvedPath(const QString &imagePath) const;
    void startLoad();
    void applyImage(quint64 request, const QString &path, const QImage &image);

    int m_diameter = 150;
    QString m_basePath;
    QString m_source;
    quint64 m_request = 0;
};

} // namespace darkeye
