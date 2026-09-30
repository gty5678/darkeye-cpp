#include "ui/pages/AvPage.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>
#include <QtTest>

class AvPageTest final : public QObject
{
    Q_OBJECT

private slots:
    void buildsDocumentTreeAndRendersMarkdown();
};

void AvPageTest::buildsDocumentTreeAndRendersMarkdown()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("分类")));

    QFile first(directory.filePath(QStringLiteral("01-首页.md")));
    QVERIFY(first.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(first.write("# 首页\n\n进入[[详情]]") > 0);
    first.close();

    QFile second(directory.filePath(QStringLiteral("分类/详情.md")));
    QVERIFY(second.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(second.write("## 详情\n\n正文") > 0);
    second.close();

    darkeye::AvPage page(directory.path());
    page.initialize();

    auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("AvWikiTree"));
    auto *browser = page.findChild<QTextBrowser *>(QStringLiteral("AvWikiBrowser"));
    QVERIFY(tree != nullptr);
    QVERIFY(browser != nullptr);
    QCOMPARE(tree->topLevelItemCount(), 2);
    QCOMPARE(tree->currentItem()->text(0), QStringLiteral("01-首页"));
    QVERIFY(browser->toPlainText().contains(QStringLiteral("进入详情")));
    QVERIFY(browser->toHtml().contains(QStringLiteral("internal:详情")));

    browser->anchorClicked(QUrl(QStringLiteral("internal:详情")));
    QCOMPARE(tree->currentItem()->text(0), QStringLiteral("详情"));
    QVERIFY(browser->toPlainText().contains(QStringLiteral("正文")));
}

QTEST_MAIN(AvPageTest)
#include "AvPageTest.moc"
