#include "ui/dialogs/PersonEditorDialog.h"

#include "darkeye_ui/theme/ThemeService.h"
#include "darkeye_ui/components/DesignButton.h"
#include "darkeye_ui/components/DesignComboBox.h"
#include "darkeye_ui/components/DesignInput.h"
#include "darkeye_ui/components/ToastNotification.h"
#include "darkeye_ui/components/TokenControls.h"
#include "darkeye_ui/components/TokenViews.h"
#include "ui/components/ImageDropWidget.h"
#include "ui/components/WikiTextEdit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace
{

QSpinBox *measurementSpin(QWidget *parent, const QString &objectName)
{
    auto *spin = new darkeye::TokenSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(0, 300);
    spin->setSpecialValueText(QStringLiteral("未填写"));
    return spin;
}

QComboBox *scoreCombo(QWidget *parent, const QString &objectName, const QStringList &labels)
{
    auto *combo = new darkeye::DesignComboBox(parent);
    combo->setObjectName(objectName);
    combo->addItem(QStringLiteral("未填写"), QVariant());
    for (int score = 0; score < labels.size(); ++score)
        combo->addItem(labels.at(score), score);
    return combo;
}

void setOptionalSpin(QSpinBox *spin, const std::optional<int> &value)
{
    spin->setValue(value.value_or(0));
}

} // namespace

namespace darkeye
{

PersonEditorDialog::PersonEditorDialog(QSqlDatabase database, ThemeService &themes,
                                       QString imageDirectory, QWidget *parent,
                                       QString coverDirectory)
    : QDialog(parent), m_repository(database), m_works(std::move(database)), m_themes(themes),
      m_imageDirectory(std::move(imageDirectory)), m_coverDirectory(std::move(coverDirectory))
{
    setObjectName(QStringLiteral("PersonEditorDialog"));
    setModal(true);
    resize(760, 720);
    buildUi();
}

void PersonEditorDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);

    auto *commonForm = new QFormLayout;
    m_imageDrop = new ImageDropWidget(m_imageDirectory, content);
    m_imageDrop->setObjectName(QStringLiteral("PersonImageDropWidget"));
    m_birthday = new DesignLineEdit(content);
    m_birthday->setObjectName(QStringLiteral("PersonBirthdayInput"));
    m_height = measurementSpin(content, QStringLiteral("PersonHeightInput"));
    m_needUpdate = new TokenCheckBox(QStringLiteral("需要后续采集更新"), content);
    m_needUpdate->setObjectName(QStringLiteral("PersonNeedUpdateInput"));
    m_notes = new WikiTextEdit(content);
    m_notes->setObjectName(QStringLiteral("PersonNotesInput"));
    m_notes->setMinimumHeight(100);
    m_notes->setCompleterList(m_works.serialSuggestions());
    m_notes->setWorkIdResolver([this](const QString &serial) {
        return m_works.findIdBySerial(serial);
    });
    m_notes->setImageResolver([this](const QString &serial) {
        const auto id = m_works.findIdBySerial(serial);
        if (!id) return QString();
        const auto work = m_works.findById(*id);
        if (!work || work->imageUrl.trimmed().isEmpty()) return QString();
        return QFileInfo(work->imageUrl).isAbsolute()
            ? work->imageUrl : QDir(m_coverDirectory).filePath(work->imageUrl);
    });
    connect(m_notes, &WikiTextEdit::workLinkRequested,
            this, &PersonEditorDialog::workLinkRequested);
    connect(m_notes, &WikiTextEdit::actressLinkRequested,
            this, &PersonEditorDialog::actressLinkRequested);
    commonForm->addRow(QStringLiteral("头像"), m_imageDrop);
    commonForm->addRow(QStringLiteral("生日"), m_birthday);
    commonForm->addRow(QStringLiteral("身高"), m_height);
    commonForm->addRow(QString(), m_needUpdate);
    commonForm->addRow(QStringLiteral("备注"), m_notes);
    contentLayout->addLayout(commonForm);

    m_actressFields = new QWidget(content);
    auto *actressForm = new QFormLayout(m_actressFields);
    m_bust = measurementSpin(m_actressFields, QStringLiteral("PersonBustInput"));
    m_waist = measurementSpin(m_actressFields, QStringLiteral("PersonWaistInput"));
    m_hip = measurementSpin(m_actressFields, QStringLiteral("PersonHipInput"));
    m_cup = new DesignLineEdit(m_actressFields);
    m_cup->setObjectName(QStringLiteral("PersonCupInput"));
    m_debutDate = new DesignLineEdit(m_actressFields);
    m_debutDate->setObjectName(QStringLiteral("PersonDebutDateInput"));
    m_minnanoUrl = new DesignLineEdit(m_actressFields);
    m_minnanoUrl->setObjectName(QStringLiteral("PersonMinnanoInput"));
    actressForm->addRow(QStringLiteral("胸围"), m_bust);
    actressForm->addRow(QStringLiteral("腰围"), m_waist);
    actressForm->addRow(QStringLiteral("臀围"), m_hip);
    actressForm->addRow(QStringLiteral("罩杯"), m_cup);
    actressForm->addRow(QStringLiteral("出道日期"), m_debutDate);
    actressForm->addRow(QStringLiteral("Minnano ID"), m_minnanoUrl);
    contentLayout->addWidget(m_actressFields);

    m_actorFields = new QWidget(content);
    auto *actorForm = new QFormLayout(m_actorFields);
    m_handsome = scoreCombo(m_actorFields, QStringLiteral("PersonHandsomeInput"),
                            {QStringLiteral("丑"), QStringLiteral("普通"), QStringLiteral("帅")});
    m_fat = scoreCombo(m_actorFields, QStringLiteral("PersonFatInput"),
                       {QStringLiteral("胖"), QStringLiteral("普通"), QStringLiteral("瘦")});
    actorForm->addRow(QStringLiteral("外貌"), m_handsome);
    actorForm->addRow(QStringLiteral("体型"), m_fat);
    contentLayout->addWidget(m_actorFields);

    auto *nameTools = new QHBoxLayout;
    auto *addName = new DesignButton(QStringLiteral("添加姓名"), content);
    addName->setObjectName(QStringLiteral("PersonAddNameButton"));
    auto *removeName = new DesignButton(QStringLiteral("删除选中姓名"), content);
    removeName->setObjectName(QStringLiteral("PersonRemoveNameButton"));
    auto *moveNameUp = new DesignButton(QStringLiteral("上移"), content);
    moveNameUp->setObjectName(QStringLiteral("PersonMoveNameUpButton"));
    auto *moveNameDown = new DesignButton(QStringLiteral("下移"), content);
    moveNameDown->setObjectName(QStringLiteral("PersonMoveNameDownButton"));
    nameTools->addWidget(addName);
    nameTools->addWidget(removeName);
    nameTools->addWidget(moveNameUp);
    nameTools->addWidget(moveNameDown);
    nameTools->addStretch();
    contentLayout->addLayout(nameTools);
    m_names = new TokenTableWidget(0, 4, content);
    m_names->setObjectName(QStringLiteral("PersonNameEditor"));
    m_names->setHorizontalHeaderLabels({QStringLiteral("中文"), QStringLiteral("日文"),
                                        QStringLiteral("英文"), QStringLiteral("假名")});
    m_names->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_names->setSelectionBehavior(QAbstractItemView::SelectRows);
    contentLayout->addWidget(m_names);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    m_buttons->setObjectName(QStringLiteral("PersonEditorButtons"));
    root->addWidget(m_buttons);
    connect(addName, &QPushButton::clicked, this, [this] { addNameRow(); });
    connect(removeName, &QPushButton::clicked, this, &PersonEditorDialog::removeSelectedNameRows);
    connect(moveNameUp, &QPushButton::clicked, this, [this] { moveSelectedNameRow(-1); });
    connect(moveNameDown, &QPushButton::clicked, this, [this] { moveSelectedNameRow(1); });
    connect(m_imageDrop, &ImageDropWidget::imageRejected, this,
            [this](const QString &message) { Toast::showError(this, message, &m_themes); });
    connect(m_buttons, &QDialogButtonBox::accepted, this,
            [this]
            {
                if (savePerson())
                    accept();
            });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool PersonEditorDialog::loadPerson(PersonKind kind, qint64 personId)
{
    QString errorMessage;
    const auto details = m_repository.findDetails(kind, personId, &errorMessage);
    if (!details.has_value())
    {
        Toast::showError(parentWidget(),
                         errorMessage.isEmpty() ? QStringLiteral("人物不存在") : errorMessage,
                         &m_themes);
        return false;
    }
    m_original = details;
    setWindowTitle(kind == PersonKind::Actress ? QStringLiteral("修改女演员")
                                               : QStringLiteral("修改男演员"));
    populate(*details);
    return true;
}

bool PersonEditorDialog::savePerson()
{
    if (!m_original.has_value())
        return false;
    QString errorMessage;
    PersonDetails details = editorDetails();
    const PersonName primary = details.names.isEmpty() ? PersonName{} : details.names.first();
    QString fileNamePart = primary.japanese.trimmed();
    if (fileNamePart.isEmpty())
        fileNamePart = primary.chinese.trimmed();
    if (fileNamePart.isEmpty())
        fileNamePart = primary.english.trimmed();
    if (fileNamePart.isEmpty())
        fileNamePart = QStringLiteral("person");
    QString storedImagePath;
    if (!m_imageDrop->persistAsJpeg(QStringLiteral("%1-%2.jpg").arg(details.id).arg(fileNamePart),
                                    &storedImagePath, &errorMessage))
    {
        Toast::showError(this, errorMessage, &m_themes);
        return false;
    }
    details.imagePath = storedImagePath;
    if (!m_repository.updateDetails(details, &errorMessage))
    {
        Toast::showError(this, errorMessage, &m_themes);
        return false;
    }
    m_original = m_repository.findDetails(details.kind, details.id);
    emit personSaved(details.kind, details.id);
    return true;
}

qint64 PersonEditorDialog::personId() const noexcept
{
    return m_original.has_value() ? m_original->id : 0;
}

void PersonEditorDialog::populate(const PersonDetails &details)
{
    m_imageDrop->setImagePath(details.imagePath);
    m_birthday->setText(details.birthday);
    setOptionalSpin(m_height, details.height);
    m_needUpdate->setChecked(details.needUpdate);
    m_notes->setPlainText(details.notes);
    m_actressFields->setVisible(details.kind == PersonKind::Actress);
    m_actorFields->setVisible(details.kind == PersonKind::Actor);
    setOptionalSpin(m_bust, details.bust);
    setOptionalSpin(m_waist, details.waist);
    setOptionalSpin(m_hip, details.hip);
    m_cup->setText(details.cup);
    m_debutDate->setText(details.debutDate);
    m_minnanoUrl->setText(details.minnanoUrl);
    const auto setScore = [](QComboBox *combo, const std::optional<int> &score)
    { combo->setCurrentIndex(score.has_value() ? combo->findData(*score) : 0); };
    setScore(m_handsome, details.handsome);
    setScore(m_fat, details.fat);
    m_names->setRowCount(0);
    for (const PersonName &name : details.names)
        addNameRow(name);
    if (details.names.isEmpty())
        addNameRow();
}

PersonDetails PersonEditorDialog::editorDetails() const
{
    PersonDetails details = *m_original;
    details.imagePath = m_imageDrop->imagePath();
    details.birthday = m_birthday->text();
    details.height = optionalSpinValue(m_height);
    details.needUpdate = m_needUpdate->isChecked();
    details.notes = m_notes->toPlainText();
    details.names.clear();
    for (int row = 0; row < m_names->rowCount(); ++row)
    {
        const auto text = [this, row](int column)
        {
            const auto *item = m_names->item(row, column);
            return item == nullptr ? QString() : item->text().trimmed();
        };
        details.names.append({0, text(0), text(1), text(2), text(3)});
    }
    if (details.kind == PersonKind::Actress)
    {
        details.bust = optionalSpinValue(m_bust);
        details.waist = optionalSpinValue(m_waist);
        details.hip = optionalSpinValue(m_hip);
        details.cup = m_cup->text();
        details.debutDate = m_debutDate->text();
        details.minnanoUrl = m_minnanoUrl->text();
    }
    else
    {
        const auto score = [](const QComboBox *combo) -> std::optional<int>
        {
            return combo->currentIndex() == 0 ? std::nullopt
                                              : std::optional<int>(combo->currentData().toInt());
        };
        details.handsome = score(m_handsome);
        details.fat = score(m_fat);
    }
    return details;
}

void PersonEditorDialog::addNameRow(const PersonName &name)
{
    const int row = m_names->rowCount();
    m_names->insertRow(row);
    const QStringList values{name.chinese, name.japanese, name.english, name.kana};
    for (int column = 0; column < values.size(); ++column)
        m_names->setItem(row, column, new QTableWidgetItem(values.at(column)));
}

void PersonEditorDialog::removeSelectedNameRows()
{
    QList<int> rows;
    for (const QModelIndex &index : m_names->selectionModel()->selectedRows())
        rows.append(index.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (const int row : rows)
        m_names->removeRow(row);
    if (m_names->rowCount() == 0)
        addNameRow();
}

void PersonEditorDialog::moveSelectedNameRow(int offset)
{
    const QModelIndexList selectedRows = m_names->selectionModel()->selectedRows();
    if (selectedRows.size() != 1)
        return;
    const int sourceRow = selectedRows.first().row();
    const int targetRow = sourceRow + offset;
    if (targetRow < 0 || targetRow >= m_names->rowCount())
        return;
    for (int column = 0; column < m_names->columnCount(); ++column)
    {
        QTableWidgetItem *source = m_names->takeItem(sourceRow, column);
        QTableWidgetItem *target = m_names->takeItem(targetRow, column);
        m_names->setItem(sourceRow, column, target);
        m_names->setItem(targetRow, column, source);
    }
    m_names->selectRow(targetRow);
}

std::optional<int> PersonEditorDialog::optionalSpinValue(const QSpinBox *spinBox)
{
    return spinBox->value() == 0 ? std::nullopt : std::optional<int>(spinBox->value());
}

} // namespace darkeye

