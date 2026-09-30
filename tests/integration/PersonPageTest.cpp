#include "ui/pages/PersonPage.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/SchemaManager.h"
#include "database/SqliteConnection.h"
#include "database/repositories/PersonRepository.h"
#include "database/repositories/PrivateRepository.h"
#include "database/repositories/WorkRepository.h"
#include "ui/pages/PersonDetailPage.h"
#include "darkeye_ui/components/CompleterLineEdit.h"
#include "darkeye_ui/components/Charts.h"
#include "darkeye_ui/components/HeartLabel.h"
#include "darkeye_ui/components/LazyScrollArea.h"
#include "ui/components/ImageDropWidget.h"
#include "ui/pages/ModifyActressPage.h"
#include "ui/components/PersonInfoPanel.h"
#include "ui/components/ActressWorkTimeline.h"
#include "ui/components/WikiTextEdit.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSqlQuery>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class PersonPageTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersActressesAndRespectsFavoriteScope();
    void detailAndEditorCompletePersonRoundTrip();
    void actressTimelineSupportsPreviewAndMiddleButtonPan();
};

void PersonPageTest::actressTimelineSupportsPreviewAndMiddleButtonPan()
{
    darkeye::ActressWorkTimeline timeline;
    timeline.resize(760, 180);
    darkeye::PersonWorkSummary work;
    work.id = 42;
    work.serialNumber = QStringLiteral("TIMELINE-042");
    work.title = QStringLiteral("时间线悬浮作品");
    work.releaseDate = QStringLiteral("2020-06-18");
    work.highlightTagId = 2;
    work.standard = true;
    timeline.setWorks({work});
    timeline.show();
    QApplication::processEvents();

    auto *canvas = timeline.findChild<QWidget *>(QStringLiteral("ActressWorkTimelineCanvas"));
    auto *scroll = timeline.findChild<QScrollArea *>();
    QVERIFY(canvas != nullptr);
    QVERIFY(scroll != nullptr);
    const QPoint marker = timeline.markerCenter(work.id);
    QVERIFY(!marker.isNull());
    QTest::mouseMove(canvas, marker);
    QTRY_VERIFY(timeline.hoverPreviewVisible());

    const int before = scroll->horizontalScrollBar()->value();
    const QPoint dragStart(qBound(20, marker.x(), canvas->width() - 20), marker.y());
    QTest::mousePress(canvas, Qt::MiddleButton, Qt::NoModifier, dragStart);
    QTest::mouseMove(canvas, dragStart - QPoint(50, 0), 20);
    QTest::mouseRelease(canvas, Qt::MiddleButton, Qt::NoModifier,
                        dragStart - QPoint(50, 0));
    QVERIFY(scroll->horizontalScrollBar()->value() > before);
    QVERIFY(!timeline.hoverPreviewVisible());
}

void PersonPageTest::filtersActressesAndRespectsFavoriteScope()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection publicConnection;
    darkeye::SqliteConnection privateConnection;
    QString errorMessage;
    QVERIFY2(
        publicConnection.open(QDir(temporaryDirectory.path()).filePath(QStringLiteral("public.db")),
                              false, &errorMessage),
        qPrintable(errorMessage));
    QVERIFY2(privateConnection.open(
                 QDir(temporaryDirectory.path()).filePath(QStringLiteral("private.db")), false,
                 &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 publicConnection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 privateConnection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));

    darkeye::PersonRepository people(publicConnection.database());
    const auto favoriteId = people.create(darkeye::PersonKind::Actress, QStringLiteral("收藏演员"),
                                          QStringLiteral("お気に入り"), &errorMessage);
    const auto otherId = people.create(darkeye::PersonKind::Actress, QStringLiteral("其他演员"),
                                       QStringLiteral("その他"), &errorMessage);
    QVERIFY2(favoriteId.has_value() && otherId.has_value(), qPrintable(errorMessage));
    darkeye::PrivateRepository privateData(privateConnection.database());
    QVERIFY2(
        privateData.addFavoriteActress(*favoriteId, QStringLiteral("お気に入り"), &errorMessage),
        qPrintable(errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::PersonPage page(darkeye::PersonKind::Actress, publicConnection.database(),
                             privateConnection.database(), themes);
    auto *area = static_cast<darkeye::LazyScrollArea *>(
        page.findChild<QWidget *>(QStringLiteral("PersonLazyScrollArea")));
    auto *name = page.findChild<darkeye::CompleterLineEdit *>(QStringLiteral("PersonNameFilter"));
    auto *scope = page.findChild<QComboBox *>(QStringLiteral("PersonScopeSelector"));
    QVERIFY(area != nullptr);
    QVERIFY(name != nullptr);
    QVERIFY(scope != nullptr);
    QCOMPARE(area->itemCount(), 2);

    scope->setCurrentIndex(1);
    QTRY_COMPARE_WITH_TIMEOUT(area->itemCount(), 1, 500);
    name->setText(QStringLiteral("其他"));
    QTRY_COMPARE_WITH_TIMEOUT(area->itemCount(), 0, 500);
    scope->setCurrentIndex(0);
    QTRY_COMPARE_WITH_TIMEOUT(area->itemCount(), 1, 500);
}

void PersonPageTest::detailAndEditorCompletePersonRoundTrip()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection publicConnection;
    darkeye::SqliteConnection privateConnection;
    QString errorMessage;
    QVERIFY2(
        publicConnection.open(QDir(temporaryDirectory.path()).filePath(QStringLiteral("public.db")),
                              false, &errorMessage),
        qPrintable(errorMessage));
    QVERIFY2(privateConnection.open(
                 QDir(temporaryDirectory.path()).filePath(QStringLiteral("private.db")), false,
                 &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 publicConnection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 privateConnection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));

    darkeye::PersonRepository people(publicConnection.database());
    const auto actressId = people.create(darkeye::PersonKind::Actress, QStringLiteral("详情演员"),
                                         QStringLiteral("詳細女優"), &errorMessage);
    QVERIFY2(actressId.has_value(), qPrintable(errorMessage));
    const auto referenceActressId = people.create(
        darkeye::PersonKind::Actress, QStringLiteral("参考演员"), QStringLiteral("参考女優"),
        &errorMessage);
    const auto zeroActressId = people.create(
        darkeye::PersonKind::Actress, QStringLiteral("零值演员"), QStringLiteral("零値女優"),
        &errorMessage);
    QVERIFY2(referenceActressId.has_value(), qPrintable(errorMessage));
    QVERIFY2(zeroActressId.has_value(), qPrintable(errorMessage));
    QSqlQuery fixture(publicConnection.database());
    fixture.prepare(QStringLiteral(
        "UPDATE actress SET height=?,bust=?,waist=?,hip=?,cup=? WHERE actress_id=?"));
    const auto updateBody = [&fixture](qint64 id, int height, int bust, int waist, int hip,
                                       const QString &cup) {
        fixture.bindValue(0, height);
        fixture.bindValue(1, bust);
        fixture.bindValue(2, waist);
        fixture.bindValue(3, hip);
        fixture.bindValue(4, cup);
        fixture.bindValue(5, id);
        return fixture.exec();
    };
    QVERIFY(updateBody(*actressId, 162, 88, 58, 87, QStringLiteral("F")));
    QVERIFY(updateBody(*referenceActressId, 168, 90, 60, 89, QStringLiteral("G")));
    QVERIFY(updateBody(*zeroActressId, 0, 0, 0, 0, QStringLiteral("A")));
    QVERIFY(fixture.exec(QStringLiteral("INSERT INTO tag(tag_id,tag_name) VALUES(1,'蓝色')")));
    QVERIFY(fixture.exec(QStringLiteral("INSERT INTO tag(tag_id,tag_name) VALUES(2,'橙色')")));
    darkeye::WorkRepository works(publicConnection.database());
    darkeye::Work work;
    work.serialNumber = QStringLiteral("PERSON-001");
    work.japaneseTitle = QStringLiteral("不能回退到日文标题");
    const auto workId = works.insertComplete(work, {*actressId}, {}, {1, 2}, &errorMessage);
    QVERIFY2(workId.has_value(), qPrintable(errorMessage));
    darkeye::Work deletedWork;
    deletedWork.serialNumber = QStringLiteral("PERSON-DELETED");
    deletedWork.chineseTitle = QStringLiteral("软删除仍读取");
    const auto deletedWorkId = works.insertComplete(
        deletedWork, {*actressId}, {}, {}, &errorMessage);
    QVERIFY2(deletedWorkId.has_value(), qPrintable(errorMessage));
    QVERIFY2(works.setDeleted(*deletedWorkId, true, &errorMessage), qPrintable(errorMessage));

    const auto sqlAligned = people.findDetails(darkeye::PersonKind::Actress, *actressId,
                                               &errorMessage);
    QVERIFY2(sqlAligned.has_value(), qPrintable(errorMessage));
    QCOMPARE(sqlAligned->bodyReference.size(), 2);
    QCOMPARE(sqlAligned->works.size(), 3);
    QSet<int> timelineTags;
    bool foundEmptyChineseTitle = false;
    bool foundDeletedWork = false;
    for (const auto &item : sqlAligned->works)
    {
        if (item.serialNumber == work.serialNumber)
            foundEmptyChineseTitle = item.title.isEmpty();
        if (item.serialNumber == deletedWork.serialNumber)
            foundDeletedWork = true;
        if (item.highlightTagId > 0) timelineTags.insert(item.highlightTagId);
    }
    QVERIFY(foundEmptyChineseTitle);
    QVERIFY(foundDeletedWork);
    QCOMPARE(timelineTags, QSet<int>({1, 2}));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::PersonDetailPage detail(darkeye::PersonKind::Actress, publicConnection.database(),
                                     privateConnection.database(), themes);
    QVERIFY(detail.showPerson(*actressId));
    QCOMPARE(detail.currentPersonId(), *actressId);
    auto *panel = detail.findChild<darkeye::PersonInfoPanel *>(QStringLiteral("PersonInfoPanel"));
    auto *workTable = detail.findChild<QTableWidget *>(QStringLiteral("PersonWorkTable"));
    auto *timeline = detail.findChild<darkeye::ActressWorkTimeline *>(
        QStringLiteral("ActressWorkTimeline"));
    auto *heart = detail.findChild<darkeye::HeartLabel *>(QStringLiteral("PersonFavoriteButton"));
    auto *radar = detail.findChild<QWidget *>(QStringLiteral("DesignRadarChart"));
    QVERIFY(panel != nullptr);
    QVERIFY(workTable != nullptr);
    QVERIFY(timeline != nullptr);
    QVERIFY(heart != nullptr);
    QVERIFY(radar != nullptr);
    QCOMPARE(workTable->rowCount(), 3);
    QCOMPARE(timeline->markerCount(), 3);
    QCOMPARE(timeline->works().size(), sqlAligned->works.size());
    auto *radarChart = static_cast<darkeye::RadarChartWidget *>(radar);
    QCOMPARE(radarChart->categories(),
             QStringList({QStringLiteral("身高"), QStringLiteral("罩杯"),
                          QStringLiteral("胸围"), QStringLiteral("腰围"),
                          QStringLiteral("臀围")}));
    QCOMPARE(radarChart->values().size(), 5);
    QCOMPARE(timeline->works().first().serialNumber, QStringLiteral("PERSON-001"));
    QCOMPARE(workTable->item(0, 0)->text(), QStringLiteral("PERSON-001"));
    QTest::mouseClick(heart, Qt::LeftButton);
    darkeye::PrivateRepository privateData(privateConnection.database());
    QVERIFY(privateData.isFavoriteActress(*actressId));
    QVERIFY(detail.showPerson(*actressId));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(detail.findChild<darkeye::HeartLabel *>(QStringLiteral("PersonFavoriteButton")),
             heart);

    const QString imageDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("actressimages"));
    darkeye::ModifyActressPage editor(publicConnection.database(), themes, imageDirectory);
    QVERIFY(editor.loadActress(*actressId));
    QJsonObject alias;
    alias.insert(QStringLiteral("jp"), QStringLiteral("采集艺名"));
    alias.insert(QStringLiteral("kana"), QStringLiteral("げいめい"));
    alias.insert(QStringLiteral("en"), QStringLiteral("Captured Alias"));
    QJsonObject captureData;
    captureData.insert(QStringLiteral("日文名"), QStringLiteral("采集日文名"));
    captureData.insert(QStringLiteral("假名"), QStringLiteral("さいしゅう"));
    captureData.insert(QStringLiteral("英文名"), QStringLiteral("Captured Name"));
    captureData.insert(QStringLiteral("身高"), 166);
    captureData.insert(QStringLiteral("胸围"), 88);
    captureData.insert(QStringLiteral("腰围"), 57);
    captureData.insert(QStringLiteral("臀围"), 86);
    captureData.insert(QStringLiteral("罩杯"), QStringLiteral("F"));
    captureData.insert(QStringLiteral("出生日期"), QStringLiteral("2000-01-02"));
    captureData.insert(QStringLiteral("出道日期"), QStringLiteral("2020-03-04"));
    captureData.insert(QStringLiteral("minnano_actress_id"), QStringLiteral("12345"));
    captureData.insert(QStringLiteral("alias_chain"), QJsonArray{alias});
    QJsonObject context;
    context.insert(QStringLiteral("actress_id"), *actressId);
    QJsonObject capture;
    capture.insert(QStringLiteral("context"), context);
    capture.insert(QStringLiteral("data"), captureData);
    QString captureError;
    QVERIFY2(editor.applyCapture(capture, &captureError), qPrintable(captureError));
    QSpinBox *capturedHeight = nullptr;
    QLineEdit *capturedMinnano = nullptr;
    for (QSpinBox *spinBox : editor.findChildren<QSpinBox *>())
    {
        if (spinBox->property("testId") == QStringLiteral("PersonHeightInput"))
            capturedHeight = spinBox;
    }
    for (QLineEdit *lineEdit : editor.findChildren<QLineEdit *>())
    {
        if (lineEdit->property("testId") == QStringLiteral("PersonMinnanoInput"))
            capturedMinnano = lineEdit;
    }
    QVERIFY(capturedHeight != nullptr);
    QCOMPARE(capturedHeight->value(), 166);
    QVERIFY(capturedMinnano != nullptr);
    QCOMPARE(capturedMinnano->text(), QStringLiteral("12345"));
    auto *capturedNames = editor.findChild<QTableWidget *>(QStringLiteral("DesignTableWidget"));
    QVERIFY(capturedNames != nullptr);
    QCOMPARE(capturedNames->item(0, 1)->text(), QStringLiteral("采集日文名"));
    QCOMPARE(capturedNames->item(1, 1)->text(), QStringLiteral("采集艺名"));
    QVERIFY(editor.loadActress(*actressId));
    const auto uncommitted =
        people.findDetails(darkeye::PersonKind::Actress, *actressId, &errorMessage);
    QVERIFY2(uncommitted.has_value(), qPrintable(errorMessage));
    QVERIFY(uncommitted->height != 166);
    auto *nameTable = editor.findChild<QTableWidget *>(QStringLiteral("DesignTableWidget"));
    QPushButton *addName = nullptr;
    QPushButton *moveNameUp = nullptr;
    for (QPushButton *button : editor.findChildren<QPushButton *>())
    {
        if (button->property("testId") == QStringLiteral("PersonAddNameButton"))
            addName = button;
        if (button->property("testId") == QStringLiteral("PersonMoveNameUpButton"))
            moveNameUp = button;
    }
    auto *notes = editor.findChild<darkeye::WikiTextEdit *>();
    auto *imageDrop =
        editor.findChild<darkeye::ImageDropWidget *>(QStringLiteral("PersonImageDropWidget"));
    QVERIFY(nameTable != nullptr);
    QVERIFY(addName != nullptr);
    QVERIFY(moveNameUp != nullptr);
    QVERIFY(notes != nullptr);
    QVERIFY(notes->completerList().contains(QStringLiteral("PERSON-001")));
    QVERIFY(imageDrop != nullptr);
    QPushButton *save = nullptr;
    for (QPushButton *button : editor.findChildren<QPushButton *>())
    {
        if (button->property("testId") == QStringLiteral("PersonSaveButton"))
            save = button;
    }
    QVERIFY(save != nullptr);
    QVERIFY(!save->isEnabled());
    nameTable->item(0, 0)->setText(QStringLiteral("修改后姓名"));
    QVERIFY(save->isEnabled());
    QVERIFY(save->styleSheet().contains(QStringLiteral("#FFA500")));
    addName->click();
    nameTable->item(1, 1)->setText(QStringLiteral("旧艺名"));
    nameTable->selectRow(1);
    moveNameUp->click();
    notes->setPlainText(QStringLiteral("修改后的备注"));
    const QString sourceImage =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("source-avatar.png"));
    QImage avatar(24, 32, QImage::Format_ARGB32);
    avatar.fill(QColor(80, 120, 200, 180));
    QVERIFY(avatar.save(sourceImage));
    imageDrop->setImagePath(sourceImage);
    QSignalSpy savedSignal(&editor, &darkeye::ModifyActressPage::personSaved);
    QVERIFY(editor.savePerson());
    QCOMPARE(savedSignal.count(), 1);
    QVERIFY(!save->isEnabled());

    const auto saved = people.findDetails(darkeye::PersonKind::Actress, *actressId, &errorMessage);
    QVERIFY2(saved.has_value(), qPrintable(errorMessage));
    QCOMPARE(saved->names.size(), 2);
    QCOMPARE(saved->names.first().japanese, QStringLiteral("旧艺名"));
    QCOMPARE(saved->names.last().chinese, QStringLiteral("修改后姓名"));
    QCOMPARE(saved->notes, QStringLiteral("修改后的备注"));
    QCOMPARE(saved->imagePath, QStringLiteral("%1-旧艺名.jpg").arg(*actressId));
    const QString storedImage = QDir(imageDirectory).filePath(saved->imagePath);
    QVERIFY(QFileInfo::exists(storedImage));
    QVERIFY(!QImage(storedImage).isNull());
    QVERIFY(detail.showPerson(*actressId));
    QCOMPARE(workTable->rowCount(), 3);
}

QTEST_MAIN(PersonPageTest)
#include "PersonPageTest.moc"
