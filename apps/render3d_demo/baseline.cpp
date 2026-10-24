// Runs the unchanged production DVD QML with deterministic synthetic data.
// This optional recorder is deliberately separate from the new QRhi renderer.
#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QScreen>
#include <QSGRendererInterface>
#include <QTest>
#include <algorithm>
#include <ctime>
#include <vector>
#include <atomic>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <Windows.h>
#endif

namespace {
double cpuSeconds()
{
#ifdef Q_OS_WIN
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        const auto ticks = [](const FILETIME &time) {
            return (quint64(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        };
        return double(ticks(kernel) + ticks(user)) / 1.0e7;
    }
    return 0;
#else
    return double(std::clock()) / CLOCKS_PER_SEC;
#endif
}

bool writeJson(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(QJsonDocument(object).toJson()) > 0;
}

QString fileHash(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
}
}

int main(int argc, char **argv)
{
    // Qt Quick's Windows default and the new renderer both use D3D11.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"output", "Output directory for baseline artifacts.", "directory"});
    parser.process(app);
    if (!parser.isSet("output"))
        return 2;
    const QDir output(QFileInfo(parser.value("output")).absoluteFilePath());
    if (!QDir().mkpath(output.path()) || !QDir().mkpath(output.filePath("covers"))
        || !QDir().mkpath(output.filePath("interaction")))
        return 3;
    const QDir source(QStringLiteral(DARKEYE_BASELINE_SOURCE_DIR));
    QVariantList textures;
    QJsonArray items;
    for (int i = 0; i < 8; ++i) {
        QImage cover(512, 320, QImage::Format_RGBA8888);
        cover.fill(QColor::fromHsv((i * 47) % 360, 160, 180));
        QPainter painter(&cover);
        painter.fillRect(QRect(246, 0, 20, 320), QColor("#17202f"));
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 20, QFont::Bold));
        painter.drawText(QRect(8, 20, 230, 240), Qt::AlignCenter,
                         QString("BACK\n%1\nTOP ↑").arg(i + 1));
        painter.drawText(QRect(274, 20, 230, 240), Qt::AlignCenter,
                         QString("FRONT\n%1\nTOP ↑").arg(i + 1));
        painter.end();
        const auto path = output.filePath(QString("covers/cover-%1.png").arg(i));
        if (!cover.save(path))
            return 4;
        textures.append(QUrl::fromLocalFile(path).toString());
    }
    for (int i = 0; i < 1000; ++i)
        items.append(QJsonObject{{"stableId", QString("synthetic-%1").arg(i, 4, 10, QChar('0'))},
                                  {"displayOrder", i}, {"title", QString("DVD %1").arg(i + 1)},
                                  {"coverPath", QString("covers/cover-%1.png").arg(i % 8)}});
    if (!writeJson(output.filePath("items.json"), {{"items", items}, {"seed", 20261015}}))
        return 5;

    QQuickWidget view;
    view.setResizeMode(QQuickWidget::SizeRootObjectToView);
    view.resize(1280, 720);
    auto *context = view.rootContext();
    context->setContextProperty("modelScale", 1.0);
    context->setContextProperty("dvdQmlUrl", QUrl::fromLocalFile(source.filePath("resources/qml/dvd/Dvd.qml")));
    context->setContextProperty("meshesPath", QUrl::fromLocalFile(source.filePath("resources/meshes/")).toString());
    context->setContextProperty("hdrPath", QUrl::fromLocalFile(source.filePath("resources/hdr/")).toString());
    context->setContextProperty("showWireframe", false);
    context->setContextProperty("dvdSpacing", 0.0145);
    context->setContextProperty("cameraDistance", 0.25);
    context->setContextProperty("selectedDvdDistance", 0.2);
    context->setContextProperty("dvdVisibleStart", QVariant(0));
    context->setContextProperty("dvdCount", QVariant(0));
    context->setContextProperty("dvdShelfLength", 0.0);
    context->setContextProperty("dvdTextureSources", QVariantList{});
    view.setSource(QUrl::fromLocalFile(source.filePath("resources/qml/dvd/dvd_scene.qml")));
    if (view.status() != QQuickWidget::Ready) {
        for (const auto &error : view.errors())
            qCritical().noquote() << error.toString();
        return 6;
    }
    view.show();
    if (!QTest::qWaitForWindowExposed(&view))
        return 7;
    auto *root = view.rootObject();
    std::atomic<quint64> renderedFrames{0};
    const auto renderingConnection=QObject::connect(view.quickWindow(),&QQuickWindow::afterRendering,&view,
        [&renderedFrames] { ++renderedFrames; },Qt::DirectConnection);
    QJsonArray measurements;
    for (int count : {1, 10, 100, 500, 1000}) {
        // Match production virtualization, rather than inventing a 1000-draw
        // baseline that the existing shelf does not actually render.
        const int visible = std::min(count, 60);
        QVariantList visibleTextures;
        for (int i = 0; i < visible; ++i)
            visibleTextures.append(textures[i % textures.size()]);
        context->setContextProperty("dvdCount", visible);
        context->setContextProperty("dvdShelfLength", count * 0.0145);
        context->setContextProperty("dvdTextureSources", visibleTextures);
        root->setProperty("cameraX", (visible - 1) * 0.0145 / 2.0);
        root->setProperty("selectedDelegateIndex", -1);
        root->setProperty("expandedDelegateIndex", -1);
        QTest::qWait(1200);
        const QString screenshot = QString("old-%1.png").arg(count);
        const auto image = view.grabFramebuffer();
        if (image.isNull() || !image.save(output.filePath(screenshot)))
            return 8;
        std::vector<double> frameTimes;
        QElapsedTimer clock;
        const auto cpuStart = cpuSeconds();
        const auto startFrames=renderedFrames.load();
        clock.start();
        // Measure an explicit camera interaction; a static scene is on demand
        // and its frame rate is not a useful performance comparison.
        // Use the same precise timer cadence as the new demo. qWait(16) has
        // event polling granularity and would bias a cross-renderer comparison.
        QEventLoop sampling;
        QTimer sampleTimer;
        sampleTimer.setTimerType(Qt::PreciseTimer);
        sampleTimer.setInterval(16);
        int step=0;
        qint64 lastTick=0;
        QObject::connect(&sampleTimer,&QTimer::timeout,&sampling,[&] {
            const auto now=clock.nsecsElapsed();
            if(step>0) frameTimes.push_back(double(now-lastTick)/1.0e6);
            lastTick=now;
            if(step>=30) { sampleTimer.stop(); sampling.quit(); return; }
            root->setProperty("orbitRotationY", (step % 15 - 7) * 0.4);
            ++step;
        });
        sampleTimer.start();
        sampling.exec();
        const double elapsed = double(clock.nsecsElapsed()) / 1.0e9;
        const double processCpu = cpuSeconds() - cpuStart;
        std::sort(frameTimes.begin(), frameTimes.end());
        measurements.append(QJsonObject{
            {"totalDvdCount", count}, {"visibleDvdCount", visible}, {"screenshot", screenshot},
            {"pixelWidth", image.width()}, {"pixelHeight", image.height()},
            {"dpr", view.devicePixelRatioF()}, {"logicalDpi", view.screen()->logicalDotsPerInch()},
            {"physicalDpi", view.screen()->physicalDotsPerInch()},
            {"interactionStepHz", 30.0 / elapsed},
            {"p95InteractionStepMs", frameTimes[28]},
            {"processCpuSeconds", processCpu},
            {"renderedFrames",double(renderedFrames.load()-startFrames)},
            {"sceneRenderHz",double(renderedFrames.load()-startFrames)/elapsed},
            {"gpuTimeMs", QJsonValue::Null}, {"gpuMemoryBytes", QJsonValue::Null}});
    }
    root->setProperty("orbitRotationY", 0);
    // Match the new demo replay: select global item 500 in a 60-item window.
    context->setContextProperty("dvdVisibleStart", 470);
    QVariantList replayTextures;
    for (int i = 0; i < 60; ++i)
        replayTextures.append(textures[(470+i) % textures.size()]);
    context->setContextProperty("dvdTextureSources", replayTextures);
    root->setProperty("cameraX", 500 * 0.0145);
    QJsonArray frames;
    QJsonArray actions;
    for (int i = 0; i < 48; ++i) {
        if (i == 2) {
            root->setProperty("selectedDelegateIndex", 30);
            actions.append(QJsonObject{{"frame", i}, {"action", "select"}, {"delegateIndex", 30}, {"globalIndex", 500}});
        }
        if (i == 14) {
            root->setProperty("expandedDelegateIndex", 30);
            actions.append(QJsonObject{{"frame", i}, {"action", "expand"}, {"delegateIndex", 30}});
        }
        if (i == 32) {
            root->setProperty("expandedDelegateIndex", -1);
            actions.append(QJsonObject{{"frame", i}, {"action", "close"}});
        }
        if (i == 44) {
            root->setProperty("selectedDelegateIndex", -1);
            actions.append(QJsonObject{{"frame", i}, {"action", "return"}});
        }
        QTest::qWait(50);
        const auto relative = QString("interaction/frame-%1.png").arg(i, 3, 10, QChar('0'));
        if (!view.grabFramebuffer().save(output.filePath(relative)))
            return 9;
        frames.append(relative);
    }
    QJsonObject hashes;
    for (const auto &relative : {"resources/qml/dvd/Dvd.qml", "resources/qml/dvd/dvd_scene.qml",
                                 "resources/meshes/back.mesh", "resources/meshes/spine.mesh",
                                 "resources/meshes/front.mesh", "resources/meshes/cD.mesh"})
        hashes.insert(QLatin1String(relative), fileHash(source.filePath(QLatin1String(relative))));
    const bool saved=writeJson(output.filePath("baseline.json"), {
        {"qtVersion", qVersion()}, {"backend", "D3D11"}, {"sourceHashes", hashes},
        {"logicalSize", QJsonArray{1280, 720}}, {"measurements", measurements},
        {"frames", frames}, {"frameDurationMs", 50}, {"actions", actions},
        {"measurementNote", "Interaction steps use a 16ms precise timer; durations are not GPU frame times. GPU counters unavailable."},
        {"recordingNote", "Deterministic state-transition replay using unchanged production QML; not a recording of physical mouse input."}
    });
    QObject::disconnect(renderingConnection);
    view.setSource(QUrl());
    return saved?0:10;
}
