#include "ui/pages/WorkPage.h"
#include "settings/Paths.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/IconButton.h"
#include "darkeye_ui/components/LazyScrollArea.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "database/SchemaManager.h"
#include "database/SqliteConnection.h"
#include "database/repositories/WorkRepository.h"
#include "crawler/CrawlerScheduler.h"
#include "ui/components/FanartStripWidget.h"
#include "ui/components/AsyncImageLabel.h"
#include "ui/components/ClickableLabel.h"
#include "ui/components/ImageDropWidget.h"
#include "ui/components/WorkCard.h"
#include "ui/pages/management/AddWorkTabPage3.h"
#include "ui/components/WorkTagSelector.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

namespace
{
class ContextMenuSpy final : public QWidget
{
public:
    int contextMenuEvents = 0;

protected:
    void contextMenuEvent(QContextMenuEvent *event) override
    {
        ++contextMenuEvents;
        event->accept();
    }
};

template <typename T>
T *findWorkControl(QObject *root, const QString &controlId)
{
    if (root == nullptr) return nullptr;
    for (T *child : root->findChildren<T *>())
        if (child->property("workControlId").toString() == controlId)
            return child;
    return nullptr;
}
}

class WorkPageTest final : public QObject
{
    Q_OBJECT

  private slots:
    void matchesPythonControlAndRoutingContract();
    void loadsOneDatabasePageAtATime();
    void filtersByPrivateScope();
    void persistsViewPreferences();
    void groupsAndSelectsTags();
    void importsCoverAndRollsBackOnDatabaseFailure();
    void workCardCopiesSerialAndActivatesOnlyCover();
    void matchesPythonWorkListSqlContract();
    void loadsConfiguredRealDatabaseWhenAvailable();
};

void WorkPageTest::loadsConfiguredRealDatabaseWhenAvailable()
{
    const QString dataDirectory = qEnvironmentVariable("DARKEYE_REAL_DATA_DIR").trimmed();
    if (dataDirectory.isEmpty())
        QSKIP("Set DARKEYE_REAL_DATA_DIR to run the real-data WorkPage integration "
              "check");

    const QString applicationDirectory = QFileInfo(dataDirectory).absoluteDir().absolutePath();
    const darkeye::settings::Paths paths(applicationDirectory);
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(paths.publicDatabase(), true, &errorMessage), qPrintable(errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::WorkPage page(connection.database(), themes, paths.workCoverDirectory());
    page.initialize();

    auto *count = findWorkControl<QLabel>(&page, QStringLiteral("WorkCountLabel"));
    auto *cardsWidget = page.findChild<QWidget *>(QStringLiteral("DesignLazyScrollArea"));
    QVERIFY(count != nullptr);
    QVERIFY2(count->text() != QStringLiteral("过滤总数:0"), qPrintable(count->text()));
    QVERIFY(cardsWidget != nullptr);
    auto *cards = static_cast<darkeye::LazyScrollArea *>(cardsWidget);
    QVERIFY2(cards->itemCount() > 0, "The real database returned no WorkPage cards");
    QVERIFY(cards->itemCount() <= cards->pageSize());
}

void WorkPageTest::matchesPythonWorkListSqlContract()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage));
    QSqlQuery seed(connection.database());
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO maker(cn_name) VALUES('片商甲')")));
    const qint64 makerId = seed.lastInsertId().toLongLong();
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO maker(cn_name) VALUES('片商乙')")));
    const qint64 otherMakerId = seed.lastInsertId().toLongLong();
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO tag(tag_id,tag_name) VALUES(1,'蓝色')")));
    QVERIFY(seed.exec(QStringLiteral("INSERT INTO tag(tag_id,tag_name) VALUES(2,'橙色')")));

    darkeye::WorkRepository repository(connection.database());
    darkeye::Work highlighted;
    highlighted.serialNumber = QStringLiteral("SQL-001");
    highlighted.chineseTitle = QStringLiteral("中文标题");
    highlighted.japaneseTitle = QStringLiteral("日文标题");
    highlighted.releaseDate = QStringLiteral("2025-01-02");
    highlighted.makerId = makerId;
    QVERIFY(repository.insertComplete(highlighted, {}, {}, {1, 2}, &errorMessage).has_value());
    darkeye::Work noDate;
    noDate.serialNumber = QStringLiteral("SQL-002");
    noDate.chineseTitle = QStringLiteral("无日期");
    noDate.makerId = makerId;
    QVERIFY(repository.insertComplete(noDate, {}, {}, {}, &errorMessage).has_value());
    darkeye::Work other;
    other.serialNumber = QStringLiteral("SQL-003");
    other.chineseTitle = QStringLiteral("其他片商");
    other.releaseDate = QStringLiteral("2024-01-02");
    other.makerId = otherMakerId;
    QVERIFY(repository.insertComplete(other, {}, {}, {}, &errorMessage).has_value());

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::WorkPage page(connection.database(), themes);
    page.initialize();
    auto *maker = findWorkControl<QComboBox>(&page, QStringLiteral("WorkMakerFilter"));
    auto *sort = findWorkControl<QComboBox>(&page, QStringLiteral("WorkSortSelector"));
    auto *count = findWorkControl<QLabel>(&page, QStringLiteral("WorkCountLabel"));
    QVERIFY(maker != nullptr);
    QVERIFY(sort != nullptr);
    QVERIFY(count != nullptr);
    QCOMPARE(count->text(), QStringLiteral("过滤总数:4"));
    QCOMPARE(page.findChildren<darkeye::WorkCard *>().size(), 4);

    QTRY_VERIFY_WITH_TIMEOUT(maker->findData(makerId) > 0, 2000);
    const int makerIndex = maker->findData(makerId);
    maker->setCurrentIndex(makerIndex);
    QTRY_COMPARE(count->text(), QStringLiteral("过滤总数:3"));
    QTRY_COMPARE(page.findChildren<darkeye::WorkCard *>().size(), 3);

    sort->setCurrentText(QStringLiteral("发布时间逆序"));
    QTRY_COMPARE(count->text(), QStringLiteral("过滤总数:2"));
    QTRY_COMPARE(page.findChildren<darkeye::WorkCard *>().size(), 2);
    for (auto *card : page.findChildren<darkeye::WorkCard *>())
    {
        const auto *serial = card->findChild<darkeye::ClickableLabel *>(
            QStringLiteral("WorkCardSerialNumber"));
        QVERIFY(serial != nullptr);
        QCOMPARE(serial->text(), QStringLiteral("SQL-001"));
    }
}

void WorkPageTest::matchesPythonControlAndRoutingContract()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));

    darkeye::WorkRepository repository(connection.database());
    darkeye::Work work;
    work.serialNumber = QStringLiteral("PAGE-001");
    work.chineseTitle = QStringLiteral("原始标题");
    const auto workId = repository.insertComplete(work, {}, {}, {}, &errorMessage);
    QVERIFY2(workId.has_value(), qPrintable(errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::WorkPage page(connection.database(), themeService);
    page.initialize();
    auto *sortSelector = findWorkControl<QComboBox>(&page, QStringLiteral("WorkSortSelector"));
    auto *tagList = page.findChild<QGraphicsView *>(QStringLiteral("WorkTagList"));
    auto *notesFilter = findWorkControl<QLineEdit>(&page, QStringLiteral("WorkNotesFilter"));
    auto *coverButton = findWorkControl<darkeye::IconButton>(
        &page, QStringLiteral("WorkCoverViewButton"));
    auto *refreshButton = findWorkControl<QPushButton>(
        &page, QStringLiteral("WorkRefreshButton"));
    auto *scopeSelector = findWorkControl<QComboBox>(
        &page, QStringLiteral("WorkScopeSelector"));
    auto *countLabel = findWorkControl<QLabel>(&page, QStringLiteral("WorkCountLabel"));
    QVERIFY(sortSelector != nullptr);
    QVERIFY(tagList != nullptr);
    QVERIFY(notesFilter != nullptr);
    QVERIFY(coverButton != nullptr);
    QVERIFY(refreshButton != nullptr);
    QVERIFY(scopeSelector != nullptr);
    QVERIFY(countLabel != nullptr);
    QCOMPARE(notesFilter->objectName(), QStringLiteral("DesignInput"));
    QCOMPARE(sortSelector->objectName(), QStringLiteral("DesignComboBox"));
    QCOMPARE(scopeSelector->objectName(), QStringLiteral("DesignComboBox"));
    QCOMPARE(countLabel->objectName(), QStringLiteral("DesignLabel"));
    QCOMPARE(coverButton->objectName(), QStringLiteral("DesignIconPushButton"));
    QCOMPARE(refreshButton->objectName(), QStringLiteral("DesignRotateButton"));
    QCOMPARE(sortSelector->count(), 13);
    QCOMPARE(sortSelector->currentText(), QStringLiteral("添加逆序"));
    QCOMPARE(page.findChildren<darkeye::WorkCard *>().size(), 1);

    auto *card = page.findChild<darkeye::WorkCard *>();
    auto *cover = card ? card->findChild<darkeye::AsyncImageLabel *>(
                             QStringLiteral("WorkCardCover"))
                       : nullptr;
    QVERIFY(card != nullptr);
    QVERIFY(cover != nullptr);
    QSignalSpy detailRequested(&page, &darkeye::WorkPage::detailRequested);
    QSignalSpy editRequested(&page, &darkeye::WorkPage::editRequested);
    QTest::mouseClick(cover, Qt::LeftButton);
    QTest::mouseClick(cover, Qt::RightButton);
    QCoreApplication::processEvents();
    QCOMPARE(detailRequested.count(), 1);
    QCOMPARE(detailRequested.first().first().toLongLong(), *workId);
    QCOMPARE(editRequested.count(), 1);
    QCOMPARE(editRequested.first().first().toLongLong(), *workId);
}

void WorkPageTest::loadsOneDatabasePageAtATime()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage),
             qPrintable(errorMessage));
    darkeye::WorkRepository repository(connection.database());
    for (int index = 1; index <= 75; ++index)
    {
        darkeye::Work work;
        work.serialNumber = QStringLiteral("PAGE-%1").arg(index, 3, 10, QLatin1Char('0'));
        QVERIFY(repository.insertComplete(work, {}, {}, {}, &errorMessage).has_value());
    }

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::WorkPage page(connection.database(), themeService);
    QVERIFY(!page.isInitialized());
    page.initialize();
    QVERIFY(page.isInitialized());
    auto *scrollArea = page.findChild<QScrollArea *>(QStringLiteral("DesignLazyScrollArea"));
    auto *lazyArea = dynamic_cast<darkeye::LazyScrollArea *>(scrollArea);
    QVERIFY(lazyArea != nullptr);
    QCOMPARE(lazyArea->itemCount(), 70);
    QCOMPARE(page.findChildren<darkeye::WorkCard *>().size(), 70);

    QVERIFY(lazyArea->loadNextPage());
    QCOMPARE(lazyArea->itemCount(), 75);
    QCOMPARE(page.findChildren<darkeye::WorkCard *>().size(), 75);
    QVERIFY(lazyArea->reachedEnd());
}

void WorkPageTest::filtersByPrivateScope()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection publicConnection;
    darkeye::SqliteConnection privateConnection;
    QString errorMessage;
    QVERIFY2(publicConnection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(privateConnection.open(QDir(temporaryDirectory.path()).filePath("private.db"), false, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(publicConnection, darkeye::DatabaseKind::Public,
                                                            &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(privateConnection, darkeye::DatabaseKind::Private,
                                                            &errorMessage));

    darkeye::WorkRepository works(publicConnection.database());
    darkeye::Work first;
    first.serialNumber = QStringLiteral("SCOPE-001");
    const auto firstId = works.insertComplete(first, {}, {}, {}, &errorMessage);
    darkeye::Work second;
    second.serialNumber = QStringLiteral("SCOPE-002");
    const auto secondId = works.insertComplete(second, {}, {}, {}, &errorMessage);
    QVERIFY(firstId.has_value());
    QVERIFY(secondId.has_value());
    darkeye::PrivateRepository privateData(privateConnection.database());
    QVERIFY(privateData.addFavoriteWork(*firstId, first.serialNumber, &errorMessage));
    QVERIFY(privateData.addFavoriteWork(*secondId, second.serialNumber, &errorMessage));
    QVERIFY(privateData.addMasturbationRecord(*firstId, first.serialNumber, QStringLiteral("2026-09-10"), QString(), 4,
                                              QString(), &errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    darkeye::WorkPage page(publicConnection.database(), themeService, {},
                           privateConnection.database());
    page.initialize();
    auto *scope = findWorkControl<QComboBox>(&page, QStringLiteral("WorkScopeSelector"));
    QVERIFY(scope != nullptr);
    QCOMPARE(page.findChildren<darkeye::WorkCard *>().size(), 2);
    scope->setCurrentText(QStringLiteral("收藏未观看"));
    QTRY_COMPARE(page.findChildren<darkeye::WorkCard *>().size(), 1);
    auto *serial = page.findChild<darkeye::ClickableLabel *>(
        QStringLiteral("WorkCardSerialNumber"));
    QVERIFY(serial != nullptr);
    QCOMPARE(serial->text(), QStringLiteral("SCOPE-002"));
    scope->setCurrentText(QStringLiteral("已撸过"));
    QTRY_VERIFY([&page]
    {
        const auto *label = page.findChild<darkeye::ClickableLabel *>(
            QStringLiteral("WorkCardSerialNumber"));
        return label != nullptr && label->text() == QStringLiteral("SCOPE-001");
    }());
    serial = page.findChild<darkeye::ClickableLabel *>(QStringLiteral("WorkCardSerialNumber"));
    QVERIFY(serial != nullptr);
    QCOMPARE(serial->text(), QStringLiteral("SCOPE-001"));
}

void WorkPageTest::persistsViewPreferences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage));
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    const darkeye::AppSettings originalSettings = darkeye::settings::app();
    darkeye::AppSettings testSettings = originalSettings;
    testSettings.workTagSelectorVisible = true;
    testSettings.workLargeCoverView = false;
    darkeye::settings::saveApp(testSettings);

    {
        darkeye::WorkPage page(connection.database(), themeService);
        page.initialize();
        auto *tagPanel = page.findChild<QWidget *>(QStringLiteral("WorkTagPanel"));
        auto *tagButton = findWorkControl<darkeye::IconButton>(
            &page, QStringLiteral("WorkTagPanelButton"));
        auto *viewButton = findWorkControl<darkeye::IconButton>(
            &page, QStringLiteral("WorkCoverViewButton"));
        auto *lazyArea = dynamic_cast<darkeye::LazyScrollArea *>(
            page.findChild<QScrollArea *>(QStringLiteral("DesignLazyScrollArea")));
        QVERIFY(tagPanel != nullptr);
        QVERIFY(tagButton != nullptr);
        QVERIFY(viewButton != nullptr);
        QVERIFY(lazyArea != nullptr);
        QVERIFY(!tagPanel->isHidden());
        QTest::mouseClick(tagButton, Qt::LeftButton);
        QTest::mouseClick(viewButton, Qt::LeftButton);
        QVERIFY(!tagPanel->isVisible());
        QCOMPARE(lazyArea->columnWidth(), 250);
    }

    darkeye::WorkPage restored(connection.database(), themeService);
    restored.initialize();
    auto *restoredPanel = restored.findChild<QWidget *>(QStringLiteral("WorkTagPanel"));
    auto *restoredArea = dynamic_cast<darkeye::LazyScrollArea *>(
        restored.findChild<QScrollArea *>(QStringLiteral("DesignLazyScrollArea")));
    QVERIFY(restoredPanel != nullptr);
    QVERIFY(restoredArea != nullptr);
    QVERIFY(!restoredPanel->isVisible());
    QCOMPARE(restoredArea->columnWidth(), 250);
    darkeye::settings::saveApp(originalSettings);
}

void WorkPageTest::groupsAndSelectsTags()
{
    const QList<darkeye::TagOption> tags{
        {1, QStringLiteral("标签甲"), QStringLiteral("类型一"), QStringLiteral("#336699"),
         QStringLiteral("说明甲"), {}, {QStringLiteral("旧标签甲")}},
        {2, QStringLiteral("标签乙"), QStringLiteral("类型二"), QStringLiteral("#663399"),
         QStringLiteral("说明乙")}};
    QWidget selectorHost;
    selectorHost.resize(800, 500);
    auto *selectorLayout = new QHBoxLayout(&selectorHost);
    selectorLayout->setContentsMargins(0, 0, 0, 0);
    darkeye::WorkTagSelector selector(tags, nullptr, &selectorHost);
    selectorLayout->addWidget(&selector, 0, Qt::AlignLeft);
    selectorLayout->addStretch();
    selectorHost.show();
    QTest::qWait(1);
    auto *tabs = selector.findChild<QTabWidget *>();
    const auto availableLists = selector.findChildren<QGraphicsView *>(QStringLiteral("WorkAvailableTagList"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(availableLists.size(), 2);
    auto *search = selector.findChild<QLineEdit *>(QStringLiteral("WorkTagSearch"));
    auto *searchResult = selector.findChild<QLabel *>(QStringLiteral("WorkTagSearchResult"));
    auto *searchNext = selector.findChild<QPushButton *>(QStringLiteral("WorkTagSearchNext"));
    auto *expand = selector.findChild<QPushButton *>(QStringLiteral("WorkTagExpandButton"));
    QVERIFY(search != nullptr);
    QVERIFY(searchResult != nullptr);
    QVERIFY(searchNext != nullptr);
    QVERIFY(expand != nullptr);
    search->setText(QStringLiteral("标签"));
    QCOMPARE(searchResult->text(), QStringLiteral("结果 1/2"));
    searchNext->click();
    QCOMPARE(searchResult->text(), QStringLiteral("结果 2/2"));
    search->setText(QStringLiteral("旧标签"));
    QCOMPARE(searchResult->text(), QStringLiteral("结果 1/1"));
    search->setText(QStringLiteral("类型一"));
    QCOMPARE(searchResult->text(), QStringLiteral("无匹配结果"));
    expand->click();
    QTest::qWait(350);
    QCOMPARE(selector.width(), 409);
    QCOMPARE(selector.pos().x(), 0);
    expand->click();
    QTest::qWait(350);
    QCOMPARE(selector.width(), 154);
    QCOMPARE(selector.pos().x(), 0);
    expand->click();
    QTest::qWait(350);
    QCOMPARE(selector.pos().x(), 0);
    QSignalSpy selectionChanged(&selector, &darkeye::WorkTagSelector::selectionChanged);
    QGraphicsObject *firstTag = nullptr;
    QGraphicsView *firstTagView = nullptr;
    for (auto *view : availableLists)
        for (auto *item : view->scene()->items())
            if (auto *object = item->toGraphicsObject();
                object && object->objectName() == QStringLiteral("WorkTag_1"))
            {
                firstTag = object;
                firstTagView = view;
            }
    QVERIFY(firstTag != nullptr);
    QVERIFY(firstTagView != nullptr);
    const QPoint clickPoint = firstTagView->mapFromScene(firstTag->sceneBoundingRect().center());
    QTest::mouseClick(firstTagView->viewport(), Qt::LeftButton, Qt::NoModifier, clickPoint);
    QCOMPARE(selector.selectedIds(), QList<qint64>{1});
    QCOMPARE(selectionChanged.count(), 1);
    QVERIFY(!firstTag->isVisible());
    auto *selectedView = selector.findChild<QGraphicsView *>(QStringLiteral("WorkTagList"));
    QVERIFY(selectedView != nullptr);
    const auto selectedItems = selectedView->scene()->items();
    QVERIFY(std::any_of(selectedItems.cbegin(), selectedItems.cend(), [](QGraphicsItem *item) {
        auto *object = item->toGraphicsObject();
        return object && object->objectName() == QStringLiteral("WorkTag_1");
    }));
    selector.clearSelection();
    QVERIFY(selector.selectedIds().isEmpty());
    QCOMPARE(selectionChanged.count(), 2);
    QVERIFY(firstTag->isVisible());

    selector.setLoader(
        [] { return QList<darkeye::TagOption>{{3, QStringLiteral("新标签"), QStringLiteral("新类型"), {}, {}}}; });
    selector.reloadTags();
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabText(0), QStringLiteral("新类型"));
}

void WorkPageTest::importsCoverAndRollsBackOnDatabaseFailure()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString coverDirectory = QDir(temporaryDirectory.path()).filePath("covers");
    const QString fanartDirectory = QDir(temporaryDirectory.path()).filePath("fanart");
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY(connection.open(QDir(temporaryDirectory.path()).filePath("public.db"), false, &errorMessage));
    QVERIFY(darkeye::SchemaManager::initializeEmptyDatabase(connection, darkeye::DatabaseKind::Public, &errorMessage));
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themeService(*application);
    darkeye::WorkRepository repository(connection.database());

    const QString sourcePath = QDir(temporaryDirectory.path()).filePath("source.png");
    QImage source(40, 30, QImage::Format_RGB32);
    source.fill(Qt::blue);
    QVERIFY(source.save(sourcePath));

    darkeye::CrawlerScheduler crawlerScheduler(
        QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/work")));
    darkeye::AddWorkTabPage3 editor(connection.database(), themeService, crawlerScheduler,
                                    coverDirectory, fanartDirectory);
    editor.beginCreate();
    auto *serial = findWorkControl<QLineEdit>(&editor, QStringLiteral("WorkSerialInput"));
    auto *title = findWorkControl<QPlainTextEdit>(
        &editor, QStringLiteral("WorkChineseTitleInput"));
    auto *imageDrop = editor.findChild<darkeye::ImageDropWidget *>(QStringLiteral("WorkCoverDropWidget"));
    auto *fanart = findWorkControl<darkeye::FanartStripWidget>(
        &editor, QStringLiteral("WorkFanartStrip"));
    auto *save = editor.findChild<QPushButton *>(QStringLiteral("WorkSaveButton"));
    QVERIFY(serial != nullptr);
    QVERIFY(title != nullptr);
    QVERIFY(imageDrop != nullptr);
    QVERIFY(fanart != nullptr);
    QVERIFY(save != nullptr);
    QSignalSpy savedSpy(&editor, &darkeye::AddWorkTabPage3::workSaved);
    serial->setText(QStringLiteral("cover-001"));
    title->setPlainText(QStringLiteral("封面导入"));
    imageDrop->setImagePath(sourcePath);
    imageDrop->setDirty(true);
    QVERIFY(fanart->addLocalImage(sourcePath, &errorMessage));
    fanart->setUrlList({QStringLiteral("https://example.invalid/scene.jpg")});
    QVERIFY(fanart->addLocalImage(sourcePath, &errorMessage));
    // 保存按钮位于可停靠工作区的基础信息窗格；测试业务行为时直接触发其槽，
    // 不依赖当前活动标签页的可见状态。
    save->click();

    QCOMPARE(savedSpy.count(), 1);
    const qint64 savedId = savedSpy.first().at(0).toLongLong();
    const auto saved = repository.findById(savedId, &errorMessage);
    QVERIFY2(saved.has_value(), qPrintable(errorMessage));
    QCOMPARE(saved->imageUrl, QStringLiteral("COVER-001.jpg"));
    const QString savedCover = QDir(coverDirectory).filePath(saved->imageUrl);
    QVERIFY(QFileInfo::exists(savedCover));
    QImage imported(savedCover);
    QVERIFY(!imported.isNull());
    QList<darkeye::FanartEntry> savedFanart;
    QVERIFY(darkeye::FanartStripWidget::parseJson(saved->fanartJson, &savedFanart, &errorMessage));
    QCOMPARE(savedFanart.size(), 2);
    QCOMPARE(savedFanart.at(0).url, QStringLiteral("https://example.invalid/scene.jpg"));
    QVERIFY(savedFanart.at(1).file.startsWith(QStringLiteral("cover001_fa_")));
    const QString importedFanart = QDir(fanartDirectory).filePath(savedFanart.at(1).file);
    QVERIFY(QFileInfo::exists(importedFanart));

    darkeye::Work duplicate;
    duplicate.serialNumber = QStringLiteral("DUPLICATE");
    QVERIFY(repository.insertComplete(duplicate, {}, {}, {}, &errorMessage).has_value());
    QVERIFY(QDir().mkpath(coverDirectory));
    const QString existingCover = QDir(coverDirectory).filePath("DUPLICATE.jpg");
    QImage original(40, 30, QImage::Format_RGB32);
    original.fill(Qt::green);
    QVERIFY(original.save(existingCover, "JPEG", 100));

    editor.beginCreate();
    serial->setText(QStringLiteral("DUPLICATE"));
    imageDrop->setImagePath(sourcePath);
    imageDrop->setDirty(true);
    QVERIFY(fanart->addLocalImage(sourcePath, &errorMessage));
    const int fanartCountBeforeFailure = QDir(fanartDirectory).entryList({QStringLiteral("*.jpg")}, QDir::Files).size();
    QTest::mouseClick(save, Qt::LeftButton);

    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(QDir(fanartDirectory).entryList({QStringLiteral("*.jpg")}, QDir::Files).size(), fanartCountBeforeFailure);
    QImage restored(existingCover);
    QVERIFY(!restored.isNull());
    const QColor restoredColor = restored.pixelColor(restored.width() / 2, restored.height() / 2);
    QVERIFY(restoredColor.green() > restoredColor.blue());

    QVERIFY(editor.loadWork(savedId));
    QVERIFY(fanart->removeEntry(1));
    QTest::mouseClick(save, Qt::LeftButton);
    QCOMPARE(savedSpy.count(), 2);
    QVERIFY(!QFileInfo::exists(importedFanart));
    const auto withoutLocalFanart = repository.findById(savedId, &errorMessage);
    QVERIFY(withoutLocalFanart.has_value());
    QList<darkeye::FanartEntry> remainingFanart;
    QVERIFY(darkeye::FanartStripWidget::parseJson(withoutLocalFanart->fanartJson, &remainingFanart, &errorMessage));
    QCOMPARE(remainingFanart.size(), 1);
    QCOMPARE(remainingFanart.first().url, QStringLiteral("https://example.invalid/scene.jpg"));
}

void WorkPageTest::workCardCopiesSerialAndActivatesOnlyCover()
{
    darkeye::WorkSummary work;
    work.id = 42;
    work.serialNumber = QStringLiteral("CARD-042");
    work.chineseTitle = QStringLiteral("作品标题");

    ContextMenuSpy contextMenuHost;
    darkeye::WorkCard card(work, {}, false, &contextMenuHost);
    card.show();
    QCoreApplication::processEvents();

    auto *serial = card.findChild<darkeye::ClickableLabel *>(
        QStringLiteral("WorkCardSerialNumber"));
    auto *cover = card.findChild<darkeye::AsyncImageLabel *>(
        QStringLiteral("WorkCardCover"));
    QVERIFY(serial != nullptr);
    QVERIFY(cover != nullptr);
    QSignalSpy activated(&card, &darkeye::WorkCard::activated);
    QSignalSpy editRequested(&card, &darkeye::WorkCard::editRequested);

    QTest::mouseClick(serial, Qt::LeftButton);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("CARD-042"));
    QCOMPARE(activated.count(), 0);
    QCOMPARE(editRequested.count(), 0);

    QTest::mouseClick(&card, Qt::LeftButton, Qt::NoModifier,
                      card.rect().bottomLeft() + QPoint(4, -4));
    QCOMPARE(activated.count(), 0);

    QTest::mouseClick(cover, Qt::LeftButton);
    QCOMPARE(activated.count(), 1);
    QTest::mouseClick(cover, Qt::RightButton);
    QCoreApplication::processEvents();
    QCOMPARE(editRequested.count(), 1);
    QContextMenuEvent contextMenu(QContextMenuEvent::Mouse, cover->rect().center(),
                                  cover->mapToGlobal(cover->rect().center()));
    QCoreApplication::sendEvent(cover, &contextMenu);
    QCOMPARE(contextMenuHost.contextMenuEvents, 0);

    QWidget host;
    auto *layout = new QHBoxLayout(&host);
    darkeye::WorkCard first(work, {}, true, &host);
    work.id = 43;
    darkeye::WorkCard second(work, {}, true, &host);
    layout->addWidget(&first);
    layout->addWidget(&second);
    host.show();
    first.setFocus();
    QTRY_VERIFY(first.hasFocus());
    QTest::keyClick(&first, Qt::Key_Right);
    QTRY_VERIFY(second.hasFocus());
    QTest::keyClick(&second, Qt::Key_Left);
    QTRY_VERIFY(first.hasFocus());
}

QTEST_MAIN(WorkPageTest)
#include "WorkPageTest.moc"
