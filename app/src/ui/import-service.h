#pragma once

#include "core/date-text.h"
#include "core/fridge-item.h"
#include "core/import-duplicates.h"
#include "core/resumable-job.h"
#include "core/vocabulary-resolver.h"
#include "data/builtin-data.h"
#include "data/store.h"
#include "ai/ingredient-classifier.h"
#include "platform/import-payload.h"
#include "recognition/recognition-service.h"

#include <QObject>
#include <QPointer>

namespace whilefresh::ui {

struct DraftItem {
    recognition::RecognizedItem recognized;
    core::FridgeItem item;
    recognition::ItemCategory category = recognition::ItemCategory::Food;
    bool selected = true;
    core::DateParse production;
    core::DateParse purchase;
    core::DateParse expiry;
    core::NameSource nameSource = core::NameSource::Unresolved;
    bool needsDateConfirmation() const;
    bool needsShelfLife() const { return item.needsShelfLife(); } // 待录保质期
    bool aiClassified() const { return nameSource == core::NameSource::AiClassified; }
};

struct ImportDraft {
    data::ImportRecord record;
    QList<DraftItem> items;
    core::DuplicateCheck duplicate;
    QString ignoredSummary;
    QString warning;
    bool ready = false;
    int ignoredCount() const;
};

// One active import per service. receive() takes ownership of the platform's private copies.
// Failure retains the payload for retry; discard() cancels recognition and deletes those copies.
// The recognizer and Store must outlive this object. Use a dedicated recognizer instance.
//
// Image imports are persisted as a resumable job (R-ADR-9): each recognized batch is stored as it
// arrives, so a retry or a later process resumes at the first batch that is not done. Destroying
// the service keeps such a job and its images for resume(); text imports are not persisted (the
// pasted text is never written to the database) and are discarded as before.
class ImportService : public QObject
{
    Q_OBJECT
public:
    ImportService(data::Store &store, std::shared_ptr<const data::BuiltinData> builtin,
                  const core::Clock &clock, recognition::RecognitionService &recognizer,
                  bool demo = false, QObject *parent = nullptr);
    ~ImportService() override;

    // Optional; both must outlive this object. Without a classifier, names the vocabulary cannot
    // resolve locally stay 未归类; without a mapping store nothing is remembered.
    void setNameResolution(core::NameMappingStore *mappings, ai::IngredientClassifier *classifier);

    bool receive(const platform::ImportPayload &payload);
    // Explicit consent is required before any recognizer invocation, even after a retry.
    bool start(bool privacyConfirmed, bool allowDuplicate = false);
    bool busy() const { return m_requestId != 0 || m_classifyId != 0; }
    const ImportDraft &draft() const { return m_draft; }
    bool updateItem(int index, DraftItem item);
    bool setCaptureDate(int imageIndex, const QDate &date);
    bool confirm(bool allowDuplicate = false);
    // The user skips a suspected duplicate: an import record with status skipped_duplicate and
    // duplicate_of is written, without items, and the draft is discarded.
    bool skipDuplicate();
    bool discard();

    // An import job left unfinished by an earlier run (or this one) that nothing is working on.
    bool hasUnfinishedJob() const;
    // Continues it: rebuilds the draft directly when every batch is done, otherwise sends only the
    // batches that are not done. Consent was given when the job started.
    bool resume();
    // Drops the unfinished job and its images.
    bool discardUnfinishedJob();

    static QString privacyNotice();

signals:
    void prepared();
    void draftReady();
    void draftChanged();
    void duplicateDetected(const whilefresh::core::DuplicateCheck &duplicate);
    void failed(const whilefresh::recognition::RecognitionError &error);
    void committed(qint64 importId);
    void skipped(qint64 importId);

private:
    recognition::RecognitionRequest request() const;
    void succeeded(quint64 id, const recognition::RecognitionResult &result);
    void batchSucceeded(quint64 id, int index, int count, const recognition::RecognitionResult &part);
    void buildDraft(const recognition::RecognitionResult &result);
    std::optional<recognition::RecognitionResult> resultFromJob() const;
    QByteArray jobPayload() const;
    bool restoreJobPayload(const QByteArray &payload);
    void saveJob();
    bool dropJob(qint64 id, const QStringList &paths);
    bool dropStoredJobs();
    void recognitionFailed(quint64 id, const recognition::RecognitionError &error);
    void classified(quint64 id, const ai::Classification &result);
    void classificationFailed(quint64 id, const ai::ChatError &error);
    void finishDraft();
    void rememberNames(const QList<DraftItem> &items);
    void parseDates(DraftItem &item) const;
    void recalculate();
    bool reject(recognition::ErrorKind kind, const QString &message);
    QList<QPair<QString, QString>> products() const;

    data::Store &m_store;
    std::shared_ptr<const data::BuiltinData> m_builtin;
    const core::Clock &m_clock;
    QPointer<recognition::RecognitionService> m_recognizer;
    platform::ImportPayload m_payload;
    ImportDraft m_draft;
    core::NameMappingStore *m_mappings = nullptr;
    QPointer<ai::IngredientClassifier> m_classifier;
    quint64 m_requestId = 0;
    qint64 m_jobId = 0;
    core::ResumableJob m_job;
    bool m_batchesReported = false; // in the current recognizer run
    quint64 m_classifyId = 0;
    bool m_received = false;
    bool m_demo = false;
};

}

Q_DECLARE_METATYPE(whilefresh::core::DuplicateCheck)
