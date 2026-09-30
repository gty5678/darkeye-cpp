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

class MakerManagementWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MakerManagementWidget(QSqlDatabase database, ThemeService &themes,
                                  QWidget *parent = nullptr);

    void refresh();

signals:
    void referencesChanged(ReferenceKind kind);
    void prefixesChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class ActiveTable { Prefixes, Makers };

    void refreshMakerChoices();
    void updateActiveTableIndicator();
    void selectPrefix(int row);
    void selectMaker(int row);
    void beginNew();
    void saveCurrent();
    void removeCurrent();
    void redirectMaker();
    void importJson();
    void exportJson();
    [[nodiscard]] ReferenceRecord makerRecord() const;

    QSqlDatabase m_database;
    ReferenceRepository m_repository;
    ReferenceJsonService m_jsonService;
    ThemeService &m_themes;
    QList<MakerPrefixRecord> m_prefixes;
    QList<ReferenceRecord> m_makers;
    qint64 m_currentPrefixId = 0;
    qint64 m_currentMakerId = 0;
    ActiveTable m_activeTable = ActiveTable::Prefixes;
    QTableWidget *m_prefixTable = nullptr;
    QTableWidget *m_makerTable = nullptr;
    QLineEdit *m_prefix = nullptr;
    QComboBox *m_prefixMaker = nullptr;
    QLineEdit *m_chineseName = nullptr;
    QLineEdit *m_japaneseName = nullptr;
    QLineEdit *m_aliases = nullptr;
    QPlainTextEdit *m_detail = nullptr;
};

} // namespace darkeye
