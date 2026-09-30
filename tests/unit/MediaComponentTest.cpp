#include "darkeye_ui/theme/ThemeService.h"
#include "ui/components/AsyncImageLabel.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/theme/IconProvider.h"
#include "ui/components/CrawlerFieldSelector.h"
#include "ui/components/FanartStripWidget.h"
#include "ui/components/ImageDropWidget.h"

#include <QApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class MediaComponentTest final : public QObject
{
    Q_OBJECT

private slots:
    void iconProviderRendersKnownIcon();
    void iconButtonRefreshesWithTheme();
    void asyncImageLoadsOffThread();
    void asyncImageCanDeferOffscreenWork();
    void clearedImageRejectsStaleResult();
    void imageDropPurposeIsConfigurable();
    void imageDropRendersDashedPlaceholderBorder();
    void fanartPreservesJsonOrderAndRemoteState();
    void fanartFinalizesPendingLocalImages();
    void crawlerFieldsKeepPythonSelectionContract();
};

void MediaComponentTest::iconProviderRendersKnownIcon()
{
    const QStringList names = {
        QStringLiteral("menu"),
        QStringLiteral("arrow_left"),
        QStringLiteral("arrow_right"),
        QStringLiteral("square_pen"),
        QStringLiteral("scroll_text"),
        QStringLiteral("check"),
        QStringLiteral("list_plus"),
        QStringLiteral("layout_panel_left"),
        QStringLiteral("circle_plus"),
        QStringLiteral("copy"),
        QStringLiteral("refresh_cw"),
        QStringLiteral("circle_question_mark"),
        QStringLiteral("settings"),
        QStringLiteral("brush_cleaning"),
        QStringLiteral("panel_left_close"),
        QStringLiteral("layout_grid"),
        QStringLiteral("layout_waterfall"),
        QStringLiteral("trash_2"),
        QStringLiteral("tv"),
    };
    for (const QString &name : names)
    {
        QVERIFY2(darkeye::IconProvider::contains(name), qPrintable(name));
        const QIcon rendered = darkeye::IconProvider::builtIn(
            name, QSize(24, 24), QColor(QStringLiteral("#c62828")), 1.0);
        QVERIFY2(!rendered.isNull(), qPrintable(name));
        QVERIFY2(!rendered.pixmap(QSize(24, 24)).isNull(), qPrintable(name));
    }
    QVERIFY(!darkeye::IconProvider::contains(QStringLiteral("missing")));
    const QIcon icon = darkeye::IconProvider::builtIn(QStringLiteral("search"), QSize(24, 24),
                                                      QColor(QStringLiteral("#c62828")), 1.0);
    QVERIFY(!icon.isNull());
    QVERIFY(!icon.pixmap(QSize(24, 24)).isNull());
}

void MediaComponentTest::iconButtonRefreshesWithTheme()
{
    darkeye::ThemeService themes(*qApp);
    QVERIFY(themes.setTheme(darkeye::ThemeId::Light));
    darkeye::IconButton button(QStringLiteral("settings"), &themes);
    const QImage light = button.icon().pixmap(button.iconSize()).toImage();
    QVERIFY(themes.setTheme(darkeye::ThemeId::Dark));
    const QImage dark = button.icon().pixmap(button.iconSize()).toImage();
    QVERIFY(light != dark);
}

void MediaComponentTest::asyncImageLoadsOffThread()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("cover.png"));
    QImage source(300, 200, QImage::Format_RGB32);
    source.fill(Qt::red);
    QVERIFY(source.save(path));

    darkeye::AsyncImageLabel label;
    label.resize(120, 80);
    QSignalSpy loaded(&label, &darkeye::AsyncImageLabel::imageLoaded);
    label.setFitMode(darkeye::ImageFitMode::Cover);
    label.setSource(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 3000);
    QVERIFY(!label.pixmap().isNull());
    QCOMPARE(label.pixmap().size(), QSize(120, 80));
}

void MediaComponentTest::asyncImageCanDeferOffscreenWork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("deferred-cover.jpg"));
    QImage source(1200, 800, QImage::Format_RGB32);
    source.fill(Qt::green);
    QVERIFY(source.save(path));

    darkeye::AsyncImageLabel label;
    label.resize(120, 80);
    label.setDeferredLoading(true);
    QSignalSpy loaded(&label, &darkeye::AsyncImageLabel::imageLoaded);
    label.setSource(path);
    QTest::qWait(50);
    QCOMPARE(loaded.count(), 0);
    QVERIFY(label.pixmap().isNull());

    label.startDeferredLoad(100);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 3000);
    QCOMPARE(label.pixmap().size(), QSize(120, 80));
}

void MediaComponentTest::clearedImageRejectsStaleResult()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("large.png"));
    QImage source(2000, 2000, QImage::Format_RGB32);
    source.fill(Qt::blue);
    QVERIFY(source.save(path));

    darkeye::AsyncImageLabel label;
    label.resize(300, 300);
    QSignalSpy loaded(&label, &darkeye::AsyncImageLabel::imageLoaded);
    label.setSource(path);
    label.clearSource();
    QTest::qWait(300);
    QCOMPARE(loaded.count(), 0);
    QVERIFY(label.pixmap().isNull());
    QCOMPARE(label.text(), QStringLiteral("无图片"));
}

void MediaComponentTest::imageDropPurposeIsConfigurable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    darkeye::ImageDropWidget imageDrop(directory.path());
    imageDrop.setPurpose(QStringLiteral("作品封面"));
    QCOMPARE(imageDrop.purpose(), QStringLiteral("作品封面"));
}

void MediaComponentTest::imageDropRendersDashedPlaceholderBorder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    darkeye::ImageDropWidget imageDrop(directory.path());
    imageDrop.resize(220, 260);
    imageDrop.show();
    QCoreApplication::processEvents();

    auto *preview = imageDrop.findChild<darkeye::AsyncImageLabel *>(
        QStringLiteral("ImageDropPreview"));
    QVERIFY(preview != nullptr);
    const QImage rendered = preview->grab().toImage();
    const QColor expectedBorder(QStringLiteral("#8a8a8a"));
    bool foundBorderPixel = false;
    for (int y = 0; y < rendered.height() && !foundBorderPixel; ++y)
    {
        for (int x = 0; x < rendered.width(); ++x)
        {
            if (rendered.pixelColor(x, y) == expectedBorder)
            {
                foundBorderPixel = true;
                break;
            }
        }
    }
    QVERIFY2(foundBorderPixel, "empty image drops must retain a dashed placeholder border");
}

void MediaComponentTest::fanartPreservesJsonOrderAndRemoteState()
{
    QList<darkeye::FanartEntry> entries;
    QString errorMessage;
    QVERIFY(darkeye::FanartStripWidget::parseJson(
        QStringLiteral("[{\"url\":\"https://example.invalid/一.jpg\",\"file\":\"one.jpg\"},"
                       "{\"url\":\"https://example.invalid/two.png\",\"file\":\"\"}]"),
        &entries, &errorMessage));
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).file, QStringLiteral("one.jpg"));
    QCOMPARE(entries.at(1).url, QStringLiteral("https://example.invalid/two.png"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    darkeye::FanartStripWidget strip(directory.path());
    strip.setEntries(entries);
    QSignalSpy changed(&strip, &darkeye::FanartStripWidget::fanartChanged);
    strip.setUrlList({QStringLiteral("https://example.invalid/一.jpg"),
                      QStringLiteral("https://example.invalid/two.png")});
    QCOMPARE(changed.count(), 0);
    QCOMPARE(strip.entries().at(0).file, QStringLiteral("one.jpg"));
    QVERIFY(strip.moveEntry(1, 0));
    QCOMPARE(changed.count(), 1);
    const QString serialized = darkeye::FanartStripWidget::toJson(strip.entries());
    QList<darkeye::FanartEntry> roundTrip;
    QVERIFY(darkeye::FanartStripWidget::parseJson(serialized, &roundTrip, &errorMessage));
    QCOMPARE(roundTrip, strip.entries());
    QVERIFY(
        !darkeye::FanartStripWidget::parseJson(QStringLiteral("{}"), &roundTrip, &errorMessage));
}

void MediaComponentTest::fanartFinalizesPendingLocalImages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.png"));
    QImage source(64, 48, QImage::Format_ARGB32);
    source.fill(QColor(10, 20, 30, 120));
    QVERIFY(source.save(sourcePath));
    const QString fanartDirectory = directory.filePath(QStringLiteral("fanart"));
    darkeye::FanartStripWidget strip(fanartDirectory);
    QString errorMessage;
    QVERIFY(strip.addLocalImage(sourcePath, &errorMessage));
    strip.setUrlList({QStringLiteral("https://example.invalid/remote.jpg")});
    QVERIFY(strip.addLocalImage(sourcePath, &errorMessage));

    QList<darkeye::FanartEntry> finalized;
    QStringList createdFiles;
    QVERIFY(strip.finalizedEntries(QStringLiteral("ABC-123"), &finalized, &createdFiles,
                                   &errorMessage));
    QCOMPARE(finalized.size(), 2);
    QCOMPARE(finalized.at(0).url, QStringLiteral("https://example.invalid/remote.jpg"));
    QCOMPARE(finalized.at(1).localPath, QString());
    QVERIFY(finalized.at(1).file.startsWith(QStringLiteral("abc123_fa_")));
    QVERIFY(finalized.at(1).file.endsWith(QStringLiteral(".jpg")));
    QCOMPARE(createdFiles.size(), 1);
    QVERIFY(QFileInfo::exists(createdFiles.first()));
    QVERIFY(!QImage(createdFiles.first()).isNull());
}

void MediaComponentTest::crawlerFieldsKeepPythonSelectionContract()
{
    const QList<darkeye::CrawlerFieldDefinition> definitions =
        darkeye::CrawlerFieldSelector::availableFields();
    QCOMPARE(definitions.size(), 15);
    QCOMPARE(definitions.first().key, QStringLiteral("release_date"));
    QCOMPARE(definitions.first().label, QStringLiteral("发布日期"));
    QCOMPARE(definitions.last().key, QStringLiteral("fanart"));
    QCOMPARE(definitions.last().label, QStringLiteral("剧照"));

    darkeye::CrawlerFieldSelector selector;
    QVERIFY(selector.selectedFields().isEmpty());
    QSignalSpy changed(&selector, &darkeye::CrawlerFieldSelector::selectionChanged);
    selector.setSelectedFields(
        {QStringLiteral("cover"), QStringLiteral("fanart"), QStringLiteral("unknown")});
    QCOMPARE(selector.selectedFields(),
             QSet<QString>({QStringLiteral("cover"), QStringLiteral("fanart")}));
    QCOMPARE(changed.count(), 1);
    QVERIFY(selector.isFieldChecked(QStringLiteral("cover")));
    QVERIFY(!selector.setFieldChecked(QStringLiteral("unknown"), true));
    selector.invertSelection();
    QCOMPARE(selector.selectedFields().size(), 13);
    selector.selectAll();
    QCOMPARE(selector.selectedFields().size(), 15);
    selector.clearSelection();
    QVERIFY(selector.selectedFields().isEmpty());
}

QTEST_MAIN(MediaComponentTest)
#include "MediaComponentTest.moc"
