#include "database/SchemaManager.h"
#include "database/SqliteConnection.h"
#include "darkeye_ui/theme/ThemeService.h"
#include "ui/pages/management/PersonalRecordManagementWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest>

class PersonalRecordManagementWidgetTest final : public QObject
{
    Q_OBJECT

private slots:
    void editsSearchesAndSavesPrivateRecords();
};

void PersonalRecordManagementWidgetTest::editsSearchesAndSavesPrivateRecords()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    darkeye::SqliteConnection connection;
    QString errorMessage;
    QVERIFY2(connection.open(temporaryDirectory.filePath(QStringLiteral("private.db")), false,
                             &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(darkeye::SchemaManager::initializeEmptyDatabase(
                 connection, darkeye::DatabaseKind::Private, &errorMessage),
             qPrintable(errorMessage));

    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    darkeye::ThemeService themes(*application);
    darkeye::PersonalRecordManagementWidget widget(connection.database(), themes);

    auto *tableChoice = widget.findChild<QComboBox *>(QStringLiteral("personalRecordTableChoice"));
    auto *search = widget.findChild<QLineEdit *>(QStringLiteral("personalRecordSearch"));
    auto *table = widget.findChild<QTableView *>(QStringLiteral("personalRecordTable"));
    auto *model = widget.findChild<QSqlTableModel *>();
    QVERIFY(tableChoice != nullptr);
    QVERIFY(search != nullptr);
    QVERIFY(table != nullptr);
    QVERIFY(model != nullptr);
    QCOMPARE(tableChoice->count(), 3);
    QCOMPARE(tableChoice->itemText(0), QStringLiteral("love_making"));
    QCOMPARE(tableChoice->itemText(1), QStringLiteral("masturbation"));
    QCOMPARE(tableChoice->itemText(2), QStringLiteral("sexual_arousal"));
    QCOMPARE(model->tableName(), QStringLiteral("love_making"));

    const int row = model->rowCount();
    QVERIFY(model->insertRow(row));
    QVERIFY(model->setData(model->index(row, model->record().indexOf(QStringLiteral("event_time"))),
                            QStringLiteral("2026-10-01 12:00")));
    QVERIFY(model->setData(model->index(row, model->record().indexOf(QStringLiteral("rating"))), 4));
    QPushButton *saveButton = nullptr;
    for (QPushButton *button : widget.findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("保存修改"))
            saveButton = button;
    QVERIFY(saveButton != nullptr);
    saveButton->click();

    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("SELECT event_time, rating FROM love_making")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("2026-10-01 12:00"));
    QCOMPARE(query.value(1).toInt(), 4);

    search->setText(QStringLiteral("不存在"));
    QTRY_COMPARE(table->model()->rowCount(), 0);
    search->clear();
    QTRY_COMPARE(table->model()->rowCount(), 1);
}

QTEST_MAIN(PersonalRecordManagementWidgetTest)
#include "PersonalRecordManagementWidgetTest.moc"
