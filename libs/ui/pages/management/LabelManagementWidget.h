#pragma once

#include "database/repositories/ReferenceRepository.h"
#include "services/ReferenceJsonService.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace darkeye
{

class ThemeService;

class LabelManagementWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit LabelManagementWidget(ReferenceKind kind, QSqlDatabase database, ThemeService &themes,
                                  QWidget *parent = nullptr);

    void refresh();

signals:
    void referencesChanged(ReferenceKind kind);

private:
    void applyFilter(const QString &text);
    void selectRow(int row);
    void beginNew();
    void saveCurrent();
    void removeCurrent();
    void redirectCurrent();
    void importJson();
    void exportJson();
    [[nodiscard]] ReferenceRecord editorRecord() const;

    QSqlDatabase m_database;
    ReferenceKind m_kind;
    ReferenceRepository m_repository;
    ReferenceJsonService m_jsonService;
    ThemeService &m_themes;
    QList<ReferenceRecord> m_records;
    qint64 m_currentId = 0;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_search = nullptr;
    QLineEdit *m_chineseName = nullptr;
    QLineEdit *m_japaneseName = nullptr;
    QLineEdit *m_aliases = nullptr;
    QPlainTextEdit *m_detail = nullptr;
};

} // namespace darkeye
