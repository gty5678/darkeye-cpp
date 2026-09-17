#include "services/ImageFetchService.h"
#include "ui/components/FanartStripWidget.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

class FakeImageProxy final : public QObject
{
public:
    explicit FakeImageProxy(QByteArray responseBody, bool respond = true)
        : m_responseBody(std::move(responseBody)), m_respond(respond)
    {
        connect(&m_server, &QTcpServer::newConnection, this,
                [this]
                {
                    QTcpSocket *socket = m_server.nextPendingConnection();
                    connect(
                        socket, &QTcpSocket::readyRead, this,
                        [this, socket]
                        {
                            m_request += socket->readAll();
                            const qsizetype headerEnd = m_request.indexOf("\r\n\r\n");
                            if (headerEnd < 0)
                                return;
                            qsizetype contentLength = 0;
                            const QList<QByteArray> headers = m_request.left(headerEnd).split('\n');
                            for (QByteArray header : headers)
                            {
                                header = header.trimmed();
                                if (header.toLower().startsWith("content-length:"))
                                    contentLength = header.mid(sizeof("content-length:") - 1)
                                                        .trimmed()
                                                        .toLongLong();
                            }
                            if (m_request.size() - headerEnd - 4 < contentLength || !m_respond)
                                return;
                            const QByteArray response =
                                QByteArrayLiteral(
                                    "HTTP/1.1 200 OK\r\nContent-Type: "
                                    "application/json\r\nConnection: close\r\nContent-Length: ") +
                                QByteArray::number(m_responseBody.size()) +
                                QByteArrayLiteral("\r\n\r\n") + m_responseBody;
                            socket->write(response);
                            socket->disconnectFromHost();
                        });
                });
        const bool listening = m_server.listen(QHostAddress::LocalHost, 0);
        Q_ASSERT(listening);
    }

    [[nodiscard]] QUrl endpoint() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/image").arg(m_server.serverPort()));
    }

    [[nodiscard]] QByteArray requestBody() const
    {
        const qsizetype separator = m_request.indexOf("\r\n\r\n");
        return separator < 0 ? QByteArray() : m_request.mid(separator + 4);
    }

    [[nodiscard]] QByteArray requestLine() const
    {
        return m_request.left(m_request.indexOf("\r\n"));
    }

private:
    QTcpServer m_server;
    QByteArray m_responseBody;
    QByteArray m_request;
    bool m_respond = true;
};

QByteArray pngPayload()
{
    QImage image(3, 2, QImage::Format_ARGB32);
    image.fill(QColor(20, 80, 160, 120));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QByteArray proxyResponse(bool success, const QByteArray &image = {}, const QString &message = {})
{
    QJsonObject object{{QStringLiteral("success"), success}};
    if (!image.isEmpty())
        object.insert(QStringLiteral("image"), QString::fromLatin1(image.toBase64()));
    if (!message.isEmpty())
        object.insert(QStringLiteral("message"), message);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

class ImageFetchServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void downloadsThroughProxyAsJpeg();
    void reportsProxyFailure();
    void preservesExistingFileForInvalidImage();
    void timesOutStalledRequest();
    void suggestsSafeJpegFileName();
    void updatesFanartEntryAfterDownload();
    void changingEntriesAbandonsOldDownload();
};

void ImageFetchServiceTest::downloadsThroughProxyAsJpeg()
{
    FakeImageProxy proxy(proxyResponse(true, pngPayload()));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString destination = directory.filePath(QStringLiteral("sample.jpg"));
    darkeye::ImageFetchService service(proxy.endpoint());
    QSignalSpy finished(&service, &darkeye::ImageFetchService::requestFinished);

    const QUrl source(QStringLiteral("https://images.example.test/a%20b.png?size=large"));
    const quint64 requestId = service.fetchToJpeg(source, destination);
    QVERIFY(finished.wait(2000));
    QCOMPARE(finished.count(), 1);
    const QList<QVariant> arguments = finished.takeFirst();
    QCOMPARE(arguments.at(0).toULongLong(), requestId);
    QVERIFY(arguments.at(1).toBool());
    QCOMPARE(arguments.at(2).toString(), destination);
    QVERIFY(arguments.at(3).toString().isEmpty());
    QVERIFY(proxy.requestLine().startsWith("POST /api/v1/image HTTP/1.1"));
    const QJsonObject request = QJsonDocument::fromJson(proxy.requestBody()).object();
    QCOMPARE(request.value(QStringLiteral("url")).toString(), source.toString());
    QImage saved(destination);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(3, 2));
    QCOMPARE(QFileInfo(destination).suffix().toLower(), QStringLiteral("jpg"));
}

void ImageFetchServiceTest::reportsProxyFailure()
{
    FakeImageProxy proxy(proxyResponse(false, {}, QStringLiteral("上游拒绝访问")));
    QTemporaryDir directory;
    darkeye::ImageFetchService service(proxy.endpoint());
    QSignalSpy finished(&service, &darkeye::ImageFetchService::requestFinished);

    QVERIFY(service.fetchToJpeg(QUrl(QStringLiteral("https://example.test/fanart.jpg")),
                                directory.filePath(QStringLiteral("fanart.jpg"))) > 0);
    QVERIFY(finished.wait(2000));
    QVERIFY(!finished.first().at(1).toBool());
    QCOMPARE(finished.first().at(3).toString(), QStringLiteral("上游拒绝访问"));
}

void ImageFetchServiceTest::preservesExistingFileForInvalidImage()
{
    const QByteArray invalidImage("this is not an image");
    FakeImageProxy proxy(proxyResponse(true, invalidImage));
    QTemporaryDir directory;
    const QString destination = directory.filePath(QStringLiteral("existing.jpg"));
    QImage original(2, 2, QImage::Format_RGB32);
    original.fill(Qt::red);
    QVERIFY(original.save(destination, "JPEG"));
    QFile beforeFile(destination);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const QByteArray before = beforeFile.readAll();
    beforeFile.close();
    darkeye::ImageFetchService service(proxy.endpoint());
    QSignalSpy finished(&service, &darkeye::ImageFetchService::requestFinished);

    QVERIFY(service.fetchToJpeg(QUrl(QStringLiteral("https://example.test/invalid.jpg")),
                                destination) > 0);
    QVERIFY(finished.wait(2000));
    QVERIFY(!finished.first().at(1).toBool());
    QVERIFY(finished.first().at(3).toString().contains(QStringLiteral("不是可读取")));
    QFile afterFile(destination);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), before);
}

void ImageFetchServiceTest::timesOutStalledRequest()
{
    FakeImageProxy proxy({}, false);
    QTemporaryDir directory;
    darkeye::ImageFetchService service(proxy.endpoint());
    service.setTimeoutMilliseconds(30);
    QSignalSpy finished(&service, &darkeye::ImageFetchService::requestFinished);

    QVERIFY(service.fetchToJpeg(QUrl(QStringLiteral("https://example.test/slow.jpg")),
                                directory.filePath(QStringLiteral("slow.jpg"))) > 0);
    QVERIFY(finished.wait(2000));
    QVERIFY(!finished.first().at(1).toBool());
    QVERIFY(finished.first().at(3).toString().contains(QStringLiteral("超时")));
    QVERIFY(!service.isBusy());
}

void ImageFetchServiceTest::suggestsSafeJpegFileName()
{
    QCOMPARE(darkeye::ImageFetchService::suggestedJpegFileName(
                 QUrl(QStringLiteral("https://example.test/path/my%20fanart.webp?size=large"))),
             QStringLiteral("my fanart.jpg"));
    QCOMPARE(darkeye::ImageFetchService::suggestedJpegFileName(
                 QUrl(QStringLiteral("https://example.test/path/CON%3Acover.png"))),
             QStringLiteral("CON_cover.jpg"));
    QVERIFY(darkeye::ImageFetchService::suggestedJpegFileName(
                QUrl(QStringLiteral("https://example.test/")))
                .isEmpty());
}

void ImageFetchServiceTest::updatesFanartEntryAfterDownload()
{
    FakeImageProxy proxy(proxyResponse(true, pngPayload()));
    QTemporaryDir directory;
    darkeye::FanartStripWidget strip(directory.path(), {}, proxy.endpoint());
    strip.setEntries({{QStringLiteral("https://images.example.test/remote.png"), {}, {}}});
    QSignalSpy changed(&strip, &darkeye::FanartStripWidget::fanartChanged);
    QSignalSpy downloadState(&strip, &darkeye::FanartStripWidget::downloadStateChanged);

    QVERIFY(strip.downloadEntry(0));
    QVERIFY(strip.downloadInProgress());
    QVERIFY(changed.wait(2000));
    QVERIFY(!strip.downloadInProgress());
    QCOMPARE(strip.entries().at(0).file, QStringLiteral("remote.jpg"));
    QVERIFY(QFileInfo::exists(directory.filePath(QStringLiteral("remote.jpg"))));
    QCOMPARE(downloadState.count(), 2);
    QVERIFY(downloadState.at(0).at(0).toBool());
    QVERIFY(!downloadState.at(1).at(0).toBool());
}

void ImageFetchServiceTest::changingEntriesAbandonsOldDownload()
{
    FakeImageProxy proxy({}, false);
    QTemporaryDir directory;
    darkeye::FanartStripWidget strip(directory.path(), {}, proxy.endpoint());
    strip.setEntries({{QStringLiteral("https://images.example.test/old.png"), {}, {}}});
    QSignalSpy rejected(&strip, &darkeye::FanartStripWidget::imageRejected);

    QVERIFY(strip.downloadEntry(0));
    strip.setEntries({{QStringLiteral("https://images.example.test/new.png"), {}, {}}});
    QVERIFY(!strip.downloadInProgress());
    QTest::qWait(100);
    QCOMPARE(strip.entries().size(), 1);
    QCOMPARE(strip.entries().at(0).url, QStringLiteral("https://images.example.test/new.png"));
    QCOMPARE(rejected.count(), 0);
}

QTEST_MAIN(ImageFetchServiceTest)
#include "ImageFetchServiceTest.moc"


