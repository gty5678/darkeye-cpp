#pragma once

#include "ui/components/IdCheckList.h"

#include <QList>
#include <QWidget>

class QLineEdit;
class QListWidget;

namespace darkeye
{

// Python ActressSelector / ActorSelector 对应控件：上方待选、下方已选，使用按钮移动条目。
class PersonTransferSelector final : public QWidget
{
    Q_OBJECT

public:
    explicit PersonTransferSelector(QString personLabel, QWidget *parent = nullptr);

    void setOptions(const QList<IdLabelOption> &options);
    [[nodiscard]] QList<qint64> selectedIds() const;
    void setSelectedIds(const QList<qint64> &ids);
    void selectId(qint64 id);
    void clearSelection();

signals:
    void selectionChanged();
    void addRequested();

private:
    void rebuildAvailable();
    void rebuildSelected();
    void chooseCurrent();
    void removeCurrent();
    [[nodiscard]] QString optionText(const IdLabelOption &option) const;

    QList<IdLabelOption> m_options;
    QList<qint64> m_selectedIds;
    QLineEdit *m_search = nullptr;
    QListWidget *m_available = nullptr;
    QListWidget *m_selected = nullptr;
};

} // namespace darkeye
