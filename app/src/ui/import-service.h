#pragma once

#include "core/date-text.h"
#include "core/fridge-item.h"
#include "core/import-duplicates.h"
#include "data/builtin-data.h"
#include "data/store.h"
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
    bool needsDateConfirmation() const;
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
class ImportService : public QObject
{
    Q_OBJECT
public:
    ImportService(data::Store &store, std::shared_ptr<const data::BuiltinData> builtin,
                  const core::Clock &clock, recognition::RecognitionService &recognizer,
                  bool demo = false, QObject *parent = nullptr);
    ~ImportService() override;

    bool receive(const platform::ImportPayload &payload);
    // Explicit consent is required before any recognizer invocation, even after a retry.
    bool start(bool privacyConfirmed, bool allowDuplicate = false);
    bool busy() const { return m_requestId != 0; }
    const ImportDraft &draft() const { return m_draft; }
    bool updateItem(int index, DraftItem item);
    bool setCaptureDate(int imageIndex, const QDate &date);
    bool confirm(bool allowDuplicate = false);
    bool discard();

    static QString privacyNotice();

signals:
    void prepared();
    void draftReady();
    void draftChanged();
    void duplicateDetected(const whilefresh::core::DuplicateCheck &duplicate);
    void failed(const whilefresh::recognition::RecognitionError &error);
    void committed(qint64 importId);

private:
    void succeeded(quint64 id, const recognition::RecognitionResult &result);
    void recognitionFailed(quint64 id, const recognition::RecognitionError &error);
    void parseDates(DraftItem &item) const;
    void recalculate();
    bool reject(recognition::ErrorKind kind, const QString &message);
    QList<QPair<QString, QString>> products() const;

    data::Store &m_store;
    std::shared_ptr<const data::BuiltinData> m_builtin;
    const core::Clock &m_clock;
    QPointer<recognition::RecognitionService> m_recognizer;
    std::shared_ptr<const recognition::Vocabulary> m_vocabulary;
    platform::ImportPayload m_payload;
    ImportDraft m_draft;
    quint64 m_requestId = 0;
    bool m_received = false;
    bool m_demo = false;
};

}

Q_DECLARE_METATYPE(whilefresh::core::DuplicateCheck)
