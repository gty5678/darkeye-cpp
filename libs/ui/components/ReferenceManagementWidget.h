#pragma once

#include "database/repositories/ReferenceRepository.h"
#include "services/ReferenceJsonService.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace darkeye
{

class ThemeService;

class ReferenceManagementWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ReferenceManagementWidget(ReferenceKind kind, QSqlDatabase database,
                                       ThemeService &themes, QWidget *parent = nullptr);

    void refresh();
    [[nodiscard]] ReferenceKind kind() const noexcept;

signals:
    void referencesChanged(ReferenceKind kind);

private:
    void buildUi();
    void selectRow(int row);
    void beginNewRecord();
    void saveCurrent();
    void removeCurrent();
    void redirectCurrent();
    void importJson(const QString &path);
    void exportJson(const QString &path);
    [[nodiscard]] ReferenceRecord editorRecord() const;
    [[nodiscard]] QString displayName(const ReferenceRecord &record) const;

    ReferenceKind m_kind;
    ReferenceRepository m_repository;
    ReferenceJsonService m_jsonService;
    ThemeService &m_themes;
    QList<ReferenceRecord> m_records;
    qint64 m_currentId = 0;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_chineseName = nullptr;
    QLineEdit *m_japaneseName = nullptr;
    QLineEdit *m_aliases = nullptr;
    QPlainTextEdit *m_detail = nullptr;
    QLineEdit *m_extra = nullptr;
    QComboBox *m_redirectSource = nullptr;
    QComboBox *m_redirectTarget = nullptr;
};

} // namespace darkeye
