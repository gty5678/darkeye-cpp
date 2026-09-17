#include "ui/components/JsonTransferBar.h"

#include "darkeye_ui/components/DesignButton.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace darkeye
{

JsonTransferBar::JsonTransferBar(QString defaultFileName, QWidget *parent)
    : QWidget(parent), m_defaultFileName(std::move(defaultFileName))
{
    setObjectName(QStringLiteral("JsonTransferBar"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *hint = new QLabel(QStringLiteral("兼容 Python 数据文件，不包含数据库 ID"), this);
    auto *importButton = new DesignButton(QStringLiteral("从 JSON 导入"), this);
    importButton->setObjectName(QStringLiteral("JsonImportButton"));
    auto *exportButton = new DesignButton(QStringLiteral("导出 JSON"), this);
    exportButton->setObjectName(QStringLiteral("JsonExportButton"));
    layout->addWidget(hint);
    layout->addStretch();
    layout->addWidget(importButton);
    layout->addWidget(exportButton);
    connect(importButton, &QPushButton::clicked, this,
            [this]
            {
                const QString path = QFileDialog::getOpenFileName(
                    this, QStringLiteral("导入参考资料"), m_defaultFileName,
                    QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));
                if (!path.isEmpty())
                    emit importRequested(path);
            });
    connect(exportButton, &QPushButton::clicked, this,
            [this]
            {
                const QString path = QFileDialog::getSaveFileName(
                    this, QStringLiteral("导出参考资料"), m_defaultFileName,
                    QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));
                if (!path.isEmpty())
                    emit exportRequested(path);
            });
}

} // namespace darkeye
