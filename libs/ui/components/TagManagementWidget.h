#pragma once

#include "database/repositories/ReferenceRepository.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace darkeye
{

class ColorPicker;
class ThemeService;
class TokenVLabel;

class TagManagementWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TagManagementWidget(QSqlDatabase database, ThemeService &themes,
                                 QWidget *parent = nullptr);

    void refresh();
    void refreshTagTypes();

signals:
    void tagsChanged();

private:
    void buildUi();
    void selectRow(int row);
    void beginNewTag();
    void saveCurrent();
    void removeCurrent();
    void redirectCurrent();
    void updatePreview();
    [[nodiscard]] TagRecord editorRecord() const;

    ReferenceRepository m_repository;
    ThemeService &m_themes;
    QList<TagRecord> m_records;
    qint64 m_currentId = 0;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_type = nullptr;
    ColorPicker *m_color = nullptr;
    QPlainTextEdit *m_detail = nullptr;
    QLineEdit *m_aliases = nullptr;
    TokenVLabel *m_preview = nullptr;
    QComboBox *m_redirectSource = nullptr;
    QComboBox *m_redirectTarget = nullptr;
};

class TagTypeManagementWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TagTypeManagementWidget(QSqlDatabase database, ThemeService &themes,
                                     QWidget *parent = nullptr);

    void refresh();

signals:
    void typesChanged();

private:
    void selectRow(int row);
    void beginNewType();
    void saveCurrent();
    void removeCurrent();
    void moveCurrent(int offset);

    ReferenceRepository m_repository;
    ThemeService &m_themes;
    QList<TagTypeRecord> m_records;
    qint64 m_currentId = 0;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_name = nullptr;
};

} // namespace darkeye
