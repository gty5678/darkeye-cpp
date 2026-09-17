#pragma once

#include "database/repositories/ReferenceRepository.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;

namespace darkeye
{

class ThemeService;

class MakerPrefixManagementWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MakerPrefixManagementWidget(QSqlDatabase database, ThemeService &themes,
                                         QWidget *parent = nullptr);

    void refresh();
    void refreshMakers();

signals:
    void prefixesChanged();

private:
    void selectRow(int row);
    void beginNewPrefix();
    void saveCurrent();
    void removeCurrent();

    ReferenceRepository m_repository;
    ThemeService &m_themes;
    QList<MakerPrefixRecord> m_records;
    qint64 m_currentId = 0;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_prefix = nullptr;
    QComboBox *m_maker = nullptr;
};

} // namespace darkeye
