#include "darkeye_ui/components/LazyScrollArea.h"
#include "darkeye_ui/layouts/FlowLayout.h"
#include "darkeye_ui/layouts/VerticalFlowLayout.h"
#include "darkeye_ui/layouts/VerticalTextLayout.h"
#include "darkeye_ui/layouts/WaterfallLayout.h"

#include <QLabel>
#include <QWidget>
#include <QtTest>

class LayoutTest final : public QObject
{
    Q_OBJECT

private slots:
    void flowLayoutWrapsItems();
    void verticalFlowLayoutWrapsItemsToTheLeft();
    void verticalTextLayoutKeepsPythonPunctuationAndRuns();
    void waterfallUsesShortestColumn();
    void lazyAreaLoadsOnePageAtATime();
    void lazyAreaSupportsHeaderAndPreloadsUntilScrollable();
};

void LayoutTest::flowLayoutWrapsItems()
{
    QWidget parent;
    auto *layout = new darkeye::FlowLayout(&parent, 0, 10);
    QList<QLabel *> labels;
    for (int index = 0; index < 3; ++index) {
        auto *label = new QLabel(QString::number(index), &parent);
        label->setFixedSize(50, 20);
        labels.append(label);
        layout->addWidget(label);
    }

    layout->setGeometry(QRect(0, 0, 120, 100));
    QCOMPARE(labels.at(0)->geometry(), QRect(0, 0, 50, 20));
    QCOMPARE(labels.at(1)->geometry(), QRect(60, 0, 50, 20));
    QCOMPARE(labels.at(2)->geometry(), QRect(0, 30, 50, 20));
    QCOMPARE(layout->heightForWidth(120), 50);
}

void LayoutTest::verticalFlowLayoutWrapsItemsToTheLeft()
{
    QWidget parent;
    parent.resize(70, 100);
    auto *layout = new darkeye::VerticalFlowLayout(&parent, 0, 10);
    QList<QLabel *> labels;
    for (int index = 0; index < 3; ++index) {
        auto *label = new QLabel(QString::number(index), &parent);
        label->setFixedSize(20, 40);
        labels.append(label);
        layout->addWidget(label);
    }

    layout->setGeometry(QRect(0, 0, 70, 100));
    QCOMPARE(labels.at(0)->geometry(), QRect(49, 0, 20, 40));
    QCOMPARE(labels.at(1)->geometry(), QRect(49, 50, 20, 40));
    QCOMPARE(labels.at(2)->geometry(), QRect(19, 0, 20, 40));
    QCOMPARE(layout->widthForHeight(100), 50);
}

void LayoutTest::verticalTextLayoutKeepsPythonPunctuationAndRuns()
{
    const QFont font(QStringLiteral("Microsoft YaHei"), 14);
    const QFontMetrics metrics(font);
    darkeye::VerticalTextLayout layout(metrics, metrics.height() * 0.05,
                                       metrics.height() * 0.1);
    QCOMPARE(darkeye::VerticalTextLayout::replaceEllipsis(
                 QStringLiteral("你好，《ABP-123》……")),
             QStringLiteral("你好︐︽ABP-123︾︙"));
    const auto runs = darkeye::VerticalTextLayout::splitTextBlocks(
        QStringLiteral("作品ABP-123︾"));
    QCOMPARE(runs.size(), 3);
    QCOMPARE(runs.at(0).text, QStringLiteral("作品"));
    QVERIFY(!runs.at(0).english);
    QCOMPARE(runs.at(1).text, QStringLiteral("ABP-123"));
    QVERIFY(runs.at(1).english);

    const auto blocks = layout.calculateLayout(QStringLiteral("作品ABP-123"),
                                               100, 200);
    QVERIFY(blocks.size() >= 3);
    bool hasRotatedAscii = false;
    for (const auto &block : blocks) {
        if (block.english && block.text == QStringLiteral("ABP-123")) {
            hasRotatedAscii = qFuzzyCompare(block.rotation, 90.0);
        }
    }
    QVERIFY(hasRotatedAscii);
    QVERIFY(layout.calculateSize(QStringLiteral("很多很多很多文字"),
                                 metrics.height() * 2).width()
            > metrics.maxWidth());
}

void LayoutTest::waterfallUsesShortestColumn()
{
    QWidget parent;
    auto *layout = new darkeye::WaterfallLayout(&parent, 100, 0, 10);
    auto *first = new QLabel(&parent);
    auto *second = new QLabel(&parent);
    auto *third = new QLabel(&parent);
    first->setFixedSize(100, 40);
    second->setFixedSize(100, 60);
    third->setFixedSize(100, 30);
    layout->addWidget(first);
    layout->addWidget(second);
    layout->addWidget(third);

    layout->setGeometry(QRect(0, 0, 210, 200));
    QCOMPARE(first->geometry(), QRect(0, 0, 100, 40));
    QCOMPARE(second->geometry(), QRect(110, 0, 100, 60));
    QCOMPARE(third->geometry(), QRect(0, 50, 100, 30));
    QCOMPARE(layout->heightForWidth(210), 80);
}

void LayoutTest::lazyAreaLoadsOnePageAtATime()
{
    darkeye::LazyScrollArea area(100);
    area.setPageSize(3);
    int calls = 0;
    area.setLoader([&calls](int page, int pageSize) {
        ++calls;
        QList<QWidget *> result;
        const int count = page == 0 ? pageSize : (page == 1 ? 1 : 0);
        for (int index = 0; index < count; ++index) {
            auto *label = new QLabel(QString::number(index));
            label->setFixedSize(100, 20);
            result.append(label);
        }
        return result;
    });

    QCOMPARE(calls, 1);
    QCOMPARE(area.itemCount(), 3);
    QCOMPARE(area.currentPage(), 1);
    QVERIFY(!area.reachedEnd());

    QVERIFY(area.loadNextPage());
    QCOMPARE(calls, 2);
    QCOMPARE(area.itemCount(), 4);
    QCOMPARE(area.currentPage(), 2);
    QVERIFY(area.reachedEnd());
    QVERIFY(!area.loadNextPage());
    QCOMPARE(calls, 2);
}

void LayoutTest::lazyAreaSupportsHeaderAndPreloadsUntilScrollable()
{
    darkeye::LazyScrollArea area(100);
    area.resize(700, 500);
    auto *header = new QLabel(QStringLiteral("概览"));
    header->setFixedSize(200, 30);
    area.setHeaderWidget(header);
    QCOMPARE(area.headerWidget(), header);
    area.setPageSize(2);
    int calls = 0;
    area.setLoader([&calls](int page, int) {
        ++calls;
        QList<QWidget *> result;
        const int count = page == 0 ? 2 : (page == 1 ? 1 : 0);
        for (int index = 0; index < count; ++index) {
            auto *label = new QLabel(QString::number(index));
            label->setFixedSize(100, 20);
            result.append(label);
        }
        return result;
    });
    area.show();
    QTRY_COMPARE_WITH_TIMEOUT(area.itemCount(), 3, 1000);
    QVERIFY(area.reachedEnd());
    QCOMPARE(calls, 2);
    area.setHeaderWidget(nullptr);
    QVERIFY(area.headerWidget() == nullptr);
}

QTEST_MAIN(LayoutTest)
#include "LayoutTest.moc"
