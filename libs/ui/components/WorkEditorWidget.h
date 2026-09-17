#pragma once

#include "database/repositories/PersonRepository.h"
#include "database/repositories/ReferenceRepository.h"
#include "database/repositories/WorkRepository.h"

#include <QUrl>
#include <QWidget>
#include <optional>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QShowEvent;
class QSpinBox;

namespace darkeye
{

class IdCheckList;
class CrawlerFieldSelector;
class FanartStripWidget;
class ImageDropWidget;
class ThemeService;
class WikiTextEdit;

class WorkEditorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WorkEditorWidget(
        QSqlDatabase database, ThemeService &themes, QString coverDirectory = {},
        QString fanartDirectory = {},
        QUrl imageFetchEndpoint = QUrl(QStringLiteral("http://127.0.0.1:56790/api/v1/image")),
        QWidget *parent = nullptr);

    void beginCreate();
    bool loadWork(qint64 workId);
    void refreshReferences();
    void refreshAssociations();
    [[nodiscard]] QString currentSerialNumber() const;

signals:
    void workSaved(qint64 workId, bool created);
    void workLinkRequested(qint64 workId);
    void actressLinkRequested(qint64 actressId);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void applyWork(const Work &work);
    [[nodiscard]] Work editorWork() const;
    void save();
    void clearEditor();
    void populateReferenceCombo(QComboBox *combo, ReferenceKind kind,
                                std::optional<qint64> selectedId = std::nullopt);

    WorkRepository m_repository;
    ReferenceRepository m_references;
    PersonRepository m_people;
    ThemeService &m_themes;
    QString m_coverDirectory;
    QString m_fanartDirectory;
    QUrl m_imageFetchEndpoint;
    std::optional<Work> m_currentWork;
    bool m_associationsLoaded = false;
    bool m_fanartDirty = false;
    ImageDropWidget *m_imageDrop = nullptr;
    QLineEdit *m_serialNumber = nullptr;
    QLineEdit *m_chineseTitle = nullptr;
    QLineEdit *m_japaneseTitle = nullptr;
    QLineEdit *m_director = nullptr;
    QLineEdit *m_releaseDate = nullptr;
    QSpinBox *m_runtime = nullptr;
    QLineEdit *m_imageUrl = nullptr;
    QLineEdit *m_videoUrl = nullptr;
    QComboBox *m_maker = nullptr;
    QComboBox *m_label = nullptr;
    QComboBox *m_series = nullptr;
    WikiTextEdit *m_notes = nullptr;
    QPlainTextEdit *m_chineseStory = nullptr;
    QPlainTextEdit *m_japaneseStory = nullptr;
    IdCheckList *m_actresses = nullptr;
    IdCheckList *m_actors = nullptr;
    IdCheckList *m_tags = nullptr;
    FanartStripWidget *m_fanart = nullptr;
    CrawlerFieldSelector *m_crawlerFields = nullptr;
};

} // namespace darkeye
