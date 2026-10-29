#include "ui/import-service.h"

#include "core/fridge-groups.h"
#include "core/quantity.h"
#include "core/texts.h"
#include "platform/import-files.h"
#include "recognition/response-parser.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace whilefresh::ui {
using namespace core;
namespace rec = recognition;

namespace {

// One stored batch: the raw chat-completion body and the images it covered, enough to parse
// and merge it again exactly as the recognizer did.
QByteArray encodeBatch(const QByteArray &response, const rec::BatchSpan &span)
{
    const QJsonObject batch{ { "firstImage", span.firstImage }, { "lastImage", span.lastImage },
                             { "response", QString::fromUtf8(response) } };
    return QJsonDocument(batch).toJson(QJsonDocument::Compact);
}

platform::ImportedImage::DateSource dateSourceFromName(const QString &name)
{
    using Source = platform::ImportedImage::DateSource;
    for (Source source : { Source::Camera, Source::Exif, Source::MediaTaken, Source::MediaAdded, Source::FileModified }) {
        if (platform::dateSourceName(source) == name)
            return source;
    }
    return Source::None;
}

std::optional<Storage> storageOf(rec::StorageKind storage)
{
    switch (storage) {
    case rec::StorageKind::Fridge: return Storage::Fridge;
    case rec::StorageKind::Freezer: return Storage::Freezer;
    case rec::StorageKind::Room: return Storage::Room;
    case rec::StorageKind::Unknown: return std::nullopt;
    }
    return std::nullopt;
}

RecordedDate recorded(const DateParse &parsed)
{
    return { parsed.date, parsed.text.isEmpty() ? DateOrigin::None : DateOrigin::Visible, parsed.text };
}

}

bool DraftItem::needsDateConfirmation() const
{
    return (production.status == DateParse::Status::NeedsBaseDate && !item.productionDate.isSet())
        || (purchase.status == DateParse::Status::NeedsBaseDate && !item.purchaseDate.isSet())
        || (expiry.status == DateParse::Status::NeedsBaseDate && !item.packageExpiry.isSet());
}

int ImportDraft::ignoredCount() const
{
    int count = 0;
    for (const auto &item : items)
        count += item.category == rec::ItemCategory::Irrelevant;
    return count;
}

ImportService::ImportService(data::Store &store, std::shared_ptr<const data::BuiltinData> builtin,
                             const Clock &clock, rec::RecognitionService &recognizer, bool demo, QObject *parent)
    : QObject(parent), m_store(store), m_builtin(std::move(builtin)), m_clock(clock),
      m_recognizer(&recognizer), m_demo(demo)
{
    connect(&recognizer, &rec::RecognitionService::succeeded, this, &ImportService::succeeded);
    connect(&recognizer, &rec::RecognitionService::failed, this, &ImportService::recognitionFailed);
    connect(&recognizer, &rec::RecognitionService::batchSucceeded, this, &ImportService::batchSucceeded);
}

ImportService::~ImportService()
{
    if (m_jobId) {
        // The job and its images stay for resume() in the next run.
        if (m_requestId && m_recognizer)
            m_recognizer->cancel(m_requestId);
        return;
    }
    discard();
}
QString ImportService::privacyNotice() { return texts::uploadPrivacyNotice(); }

bool ImportService::reject(rec::ErrorKind kind, const QString &message)
{
    auto error = rec::makeError(kind, {});
    if (!message.isEmpty())
        error.userMessage = message;
    emit failed(error);
    return false;
}

bool ImportService::receive(const platform::ImportPayload &payload)
{
    // The current draft must be explicitly confirmed/discarded; unsolicited shares cannot replace it.
    if (m_received || !m_payload.images.isEmpty()) {
        QSet<QString> active;
        for (const auto &image : m_payload.images)
            active.insert(QFileInfo(image.localPath).absoluteFilePath());
        QStringList rejected;
        for (const auto &image : payload.images) {
            if (!active.contains(QFileInfo(image.localPath).absoluteFilePath()))
                rejected.append(image.localPath);
        }
        m_store.discardImportFiles(rejected);
        return reject(rec::ErrorKind::InvalidInput, QStringLiteral("请先确认或放弃当前入库草稿，再重新导入"));
    }
    // A new import replaces a job nobody resumed; its images would otherwise never be freed.
    dropStoredJobs();
    m_payload = payload;
    m_received = true;
    m_draft = {};
    m_jobId = 0;
    m_job = {};
    auto &record = m_draft.record;
    record.createdAt = m_clock.now();
    record.isDemo = m_demo;
    record.origin = platform::originName(payload.origin);
    if (payload.kind == platform::ImportPayload::Kind::Text) {
        record.inputKind = rec::enumKey(rec::InputKind::OrderText);
        record.textLength = payload.text.size();
        if (payload.text.trimmed().isEmpty())
            return reject(rec::ErrorKind::InvalidInput, texts::importEmptyInput());
    } else {
        record.inputKind = rec::enumKey(payload.origin == platform::ImportPayload::Origin::Camera
                                           ? rec::InputKind::Receipt : rec::InputKind::Screenshot);
        if (payload.images.isEmpty() || payload.images.size() > platform::maxImagesPerImport)
            return reject(rec::ErrorKind::InvalidInput, texts::importEmptyInput());
        qint64 bytes = 0;
        QStringList hashes;
        for (const auto &image : payload.images) {
            QFile file(image.localPath);
            const QFileInfo info(image.localPath);
            if (info.isSymLink() || info.absoluteDir().canonicalPath() != QDir(m_store.importsDirectory()).canonicalPath()
                || !info.isFile() || info.size() <= 0 || info.size() > platform::maxImageBytes
                || !file.open(QIODevice::ReadOnly))
                return reject(rec::ErrorKind::InvalidInput, QStringLiteral("无法读取导入图片，请重新选择"));
            bytes += info.size();
            if (bytes > platform::maxImportBytes)
                return reject(rec::ErrorKind::InvalidInput, QStringLiteral("图片总大小超过导入限制"));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hash.addData(&file))
                return reject(rec::ErrorKind::InvalidInput, QStringLiteral("无法读取导入图片，请重新选择"));
            const QString sha = QString::fromLatin1(hash.result().toHex());
            hashes.append(sha);
            record.images.append({ image.localPath, sha, image.mimeType, image.capturedAt,
                                   platform::dateSourceName(image.capturedAtSource) });
        }
        m_draft.duplicate = checkSameImage(hashes, m_store.importFingerprints());
    }
    emit prepared();
    if (m_draft.duplicate.isDuplicate())
        emit duplicateDetected(m_draft.duplicate);
    return true;
}

bool ImportService::start(bool privacyConfirmed, bool allowDuplicate)
{
    if (!m_received || busy() || m_draft.ready || !m_recognizer)
        return reject(rec::ErrorKind::InvalidInput, QStringLiteral("当前不能开始识别"));
    if (!privacyConfirmed)
        return reject(rec::ErrorKind::InvalidInput, QStringLiteral("上传前需要你确认隐私提示"));
    if (m_draft.duplicate.isDuplicate() && !allowDuplicate) {
        emit duplicateDetected(m_draft.duplicate);
        return false;
    }
    if (m_payload.kind == platform::ImportPayload::Kind::Images) {
        // A failed receive must not be usable as a partially validated upload.
        if (m_payload.images.isEmpty() || m_draft.record.images.size() != m_payload.images.size())
            return reject(rec::ErrorKind::InvalidInput, texts::importEmptyInput());
        m_job.start();
        if (m_jobId)
            saveJob();
        else
            m_jobId = m_store.insertImportJob(jobPayload(), m_job);
    } else if (m_payload.text.trimmed().isEmpty()) {
        return reject(rec::ErrorKind::InvalidInput, texts::importEmptyInput());
    }
    m_batchesReported = false;
    m_requestId = m_recognizer->recognize(request());
    return true;
}

rec::RecognitionRequest ImportService::request() const
{
    rec::RecognitionRequest request;
    request.catalog = m_builtin->catalog;
    request.kind = rec::inputKindFromString(m_draft.record.inputKind).value_or(rec::InputKind::Screenshot);
    request.text = m_payload.text;
    for (const auto &image : m_payload.images)
        request.images.append({ image.localPath, image.capturedAt.date() });
    if (m_payload.kind == platform::ImportPayload::Kind::Images)
        request.firstBatch = m_job.nextBatch();
    return request;
}

void ImportService::saveJob()
{
    if (m_jobId && !m_store.saveImportJob(m_jobId, m_job))
        qWarning("import job %lld: progress not saved", m_jobId);
}

QByteArray ImportService::jobPayload() const
{
    const auto &record = m_draft.record;
    QJsonArray images;
    for (const auto &image : record.images) {
        images.append(QJsonObject{
            { "path", image.path }, { "sha256", image.sha256 }, { "mimeType", image.mimeType },
            { "capturedAt", image.capturedAt.isValid() ? image.capturedAt.toUTC().toString(Qt::ISODateWithMs) : QString() },
            { "capturedAtSource", image.capturedAtSource } });
    }
    const QJsonObject payload{ { "inputKind", record.inputKind }, { "origin", record.origin },
                               { "createdAt", record.createdAt.toUTC().toString(Qt::ISODateWithMs) },
                               { "isDemo", record.isDemo }, { "images", images } };
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

bool ImportService::restoreJobPayload(const QByteArray &json)
{
    const QJsonObject payload = QJsonDocument::fromJson(json).object();
    m_draft = {};
    m_payload = {};
    m_payload.kind = platform::ImportPayload::Kind::Images;
    auto &record = m_draft.record;
    record.inputKind = payload.value("inputKind").toString();
    record.origin = payload.value("origin").toString();
    m_payload.origin = platform::originFromName(record.origin).value_or(platform::ImportPayload::Origin::Gallery);
    record.createdAt = QDateTime::fromString(payload.value("createdAt").toString(), Qt::ISODateWithMs);
    record.isDemo = payload.value("isDemo").toBool() || m_demo;
    bool complete = rec::inputKindFromString(record.inputKind).has_value();
    const QString directory = QDir(m_store.importsDirectory()).canonicalPath();
    for (const auto &value : payload.value("images").toArray()) {
        const QJsonObject object = value.toObject();
        data::ImportImageRecord image{ object.value("path").toString(), object.value("sha256").toString(),
                                       object.value("mimeType").toString(),
                                       QDateTime::fromString(object.value("capturedAt").toString(), Qt::ISODateWithMs).toLocalTime(),
                                       object.value("capturedAtSource").toString() };
        const QFileInfo info(image.path);
        complete = complete && info.isFile() && !info.isSymLink() && info.absoluteDir().canonicalPath() == directory;
        record.images.append(image);
        m_payload.images.append({ image.path, image.mimeType, image.capturedAt, dateSourceFromName(image.capturedAtSource) });
    }
    return complete && !record.images.isEmpty();
}

void ImportService::parseDates(DraftItem &draft) const
{
    QDate base;
    int index = draft.recognized.imageIndex;
    if (index < 0 && m_payload.images.size() == 1)
        index = 0;
    if (index >= 0 && index < m_payload.images.size())
        base = m_payload.images.at(index).capturedAt.date();
    draft.production = parseDateText(draft.recognized.productionDateText, base);
    draft.purchase = parseDateText(draft.recognized.purchaseDateText, base);
    draft.expiry = parseDateText(draft.recognized.expiryDateText, base);
    if (draft.item.productionDate.origin != DateOrigin::User)
        draft.item.productionDate = recorded(draft.production);
    if (draft.item.purchaseDate.origin != DateOrigin::User)
        draft.item.purchaseDate = recorded(draft.purchase);
    if (draft.item.packageExpiry.origin != DateOrigin::User)
        draft.item.packageExpiry = recorded(draft.expiry);
}

void ImportService::batchSucceeded(quint64 id, int index, int count, const rec::RecognitionResult &part)
{
    if (id != m_requestId || m_payload.kind != platform::ImportPayload::Kind::Images)
        return;
    m_batchesReported = true;
    if (!m_job.plan(count)) {
        // The images were split differently this time, so stored batches no longer line up:
        // start over from the first batch.
        m_recognizer->cancel(m_requestId);
        m_job.reset();
        m_job.start();
        saveJob();
        m_requestId = m_recognizer->recognize(request());
        return;
    }
    if (m_job.completeBatch(index, encodeBatch(part.rawResponses.value(0), part.batchSpans.value(0))))
        saveJob();
}

void ImportService::succeeded(quint64 id, const rec::RecognitionResult &result)
{
    if (id != m_requestId || !m_received)
        return;
    m_requestId = 0;
    if (m_payload.kind != platform::ImportPayload::Kind::Images) {
        buildDraft(result);
        return;
    }
    if (!m_batchesReported) {
        // Recognizers that answer in one piece (replay, demo): the whole answer becomes the job.
        m_job.reset();
        m_job.start();
        if (m_job.plan(int(result.rawResponses.size()))) {
            for (int i = 0; i < result.rawResponses.size(); ++i)
                m_job.completeBatch(i, encodeBatch(result.rawResponses.at(i), result.batchSpans.value(i)));
        }
        saveJob();
        buildDraft(result);
        return;
    }
    // Batches finished in an earlier run are only in the job; merge everything from there.
    const auto all = m_job.state() == ResumableJob::State::Done ? resultFromJob() : std::nullopt;
    buildDraft(all ? *all : result);
}

std::optional<rec::RecognitionResult> ImportService::resultFromJob() const
{
    const auto kind = rec::inputKindFromString(m_draft.record.inputKind).value_or(rec::InputKind::Screenshot);
    QList<rec::RecognitionResult> parts;
    for (const QByteArray &encoded : m_job.results()) {
        const QJsonObject batch = QJsonDocument::fromJson(encoded).object();
        auto outcome = rec::parseChatCompletion(batch.value("response").toString().toUtf8(), m_builtin->catalog.get(), kind);
        if (!outcome.result)
            return std::nullopt;
        outcome.result->batchSpans = { { batch.value("firstImage").toInt(-1), batch.value("lastImage").toInt(-1) } };
        parts.append(*outcome.result);
    }
    if (parts.isEmpty())
        return std::nullopt;
    return rec::mergeResults(parts);
}

void ImportService::buildDraft(const rec::RecognitionResult &result)
{
    m_draft.items.clear();
    m_draft.record.isDemo = m_draft.record.isDemo || m_demo || result.isDemo;
    m_draft.record.truncated = result.truncated;
    m_draft.record.sourceType = rec::enumKey(result.sourceType);
    m_draft.warning = result.truncated ? texts::importMayBeIncomplete() : QString();
    m_draft.ignoredSummary = result.ignoredSummary;
    const auto rules = m_store.ruleBook();
    const auto cache = m_store.aiEstimateCache();
    const ShelfLifeContext context{ m_builtin->catalog.get(), m_builtin->shelfLife.get(), &rules, &cache };
    for (const auto &recognized : result.items) {
        DraftItem draft;
        draft.recognized = recognized;
        draft.category = recognized.category;
        draft.selected = recognized.selected && recognized.category != rec::ItemCategory::Irrelevant;
        auto &item = draft.item;
        item.canonicalName = m_builtin->catalog->contains(recognized.canonicalName)
            ? recognized.canonicalName : rec::uncategorizedName();
        item.displayName = item.canonicalName == rec::uncategorizedName() ? recognized.rawName : item.canonicalName;
        item.rawName = recognized.rawName;
        item.category = m_builtin->catalog->categoryOf(item.canonicalName);
        item.quantityText = recognized.quantityText;
        item.count = recognized.count;
        item.entryDate = m_clock.today();
        item.aiDays = { recognized.shelfLifeDays.fridge, recognized.shelfLifeDays.freezer, recognized.shelfLifeDays.room };
        const auto storage = suggestStorage(item.canonicalName, storageOf(recognized.storage),
                                             recognized.category == rec::ItemCategory::Seasoning, context);
        item.storage = storage.storage;
        item.storageSource = storage.source;
        parseDates(draft);
        m_draft.items.append(draft);
    }
    m_draft.ready = true;
    recalculate();
    emit draftReady();
}

void ImportService::recognitionFailed(quint64 id, const rec::RecognitionError &error)
{
    if (id != m_requestId)
        return;
    m_requestId = 0;
    if (m_payload.kind == platform::ImportPayload::Kind::Images && m_job.fail())
        saveJob();
    emit failed(error);
}

QList<QPair<QString, QString>> ImportService::products() const
{
    QList<QPair<QString, QString>> result;
    for (const auto &draft : m_draft.items) {
        if (draft.selected && draft.category != rec::ItemCategory::Irrelevant)
            result.append({ draft.item.canonicalName, draft.item.quantityText.trimmed() + QChar(0x1f)
                                                        + QString::number(draft.item.count) });
    }
    return result;
}

void ImportService::recalculate()
{
    auto cache = m_store.aiEstimateCache();
    const auto rules = m_store.ruleBook();
    const ShelfLifeContext context{ m_builtin->catalog.get(), m_builtin->shelfLife.get(), &rules, &cache };
    for (auto &draft : m_draft.items) {
        draft.item.kind = classifyItemKind(draft.item.canonicalName, draft.category == rec::ItemCategory::Seasoning,
                                           draft.item.storage, m_builtin->catalog.get(), m_builtin->shelfLife.get());
        const auto result = resolveShelfLife(draft.item.shelfLifeInput(), context);
        draft.item.applyShelfLife(result);
        if (draft.selected && draft.category != rec::ItemCategory::Irrelevant) {
            for (const auto &entry : result.newAiEstimates)
                cache.insert(entry.name, entry.storage, entry.days);
        }
    }
    if (m_draft.duplicate.kind != DuplicateCheck::Kind::SameImage)
        m_draft.duplicate = checkSimilarItems(products(), m_clock.now(), m_store.importFingerprints());
    if (m_draft.duplicate.isDuplicate())
        emit duplicateDetected(m_draft.duplicate);
}

bool ImportService::updateItem(int index, DraftItem draft)
{
    if (!m_draft.ready || busy() || index < 0 || index >= m_draft.items.size())
        return false;
    const auto &before = m_draft.items.at(index);
    draft.recognized = before.recognized;
    draft.production = before.production;
    draft.purchase = before.purchase;
    draft.expiry = before.expiry;
    draft.item.entryDate = before.item.entryDate;
    draft.item.id = 0;
    draft.item.importId = 0;
    draft.item.status = ItemStatus::InStock;
    if (draft.item.canonicalName != before.item.canonicalName) {
        draft.item.aiDays = {};
        draft.item.referenceConditionsConfirmed = false;
    }
    if (draft.item.storage != before.item.storage) {
        draft.item.manualDays.reset();
        draft.item.referenceConditionsConfirmed = false;
    }
    draft.item.category = m_builtin->catalog->categoryOf(draft.item.canonicalName);
    if (draft.item.productionDate.date != before.item.productionDate.date)
        draft.item.productionDate.origin = DateOrigin::User;
    if (draft.item.purchaseDate.date != before.item.purchaseDate.date)
        draft.item.purchaseDate.origin = DateOrigin::User;
    if (draft.item.packageExpiry.date != before.item.packageExpiry.date) {
        draft.item.packageExpiry.origin = DateOrigin::User;
        draft.item.manualDays.reset();
    }
    m_draft.items[index] = draft;
    recalculate();
    emit draftChanged();
    return true;
}

bool ImportService::setCaptureDate(int imageIndex, const QDate &date)
{
    if (!m_draft.ready || !date.isValid() || imageIndex < 0 || imageIndex >= m_payload.images.size())
        return false;
    m_payload.images[imageIndex].capturedAt = QDateTime(date, QTime(12, 0));
    m_draft.record.images[imageIndex].capturedAt = m_payload.images.at(imageIndex).capturedAt;
    m_draft.record.images[imageIndex].capturedAtSource = QStringLiteral("user");
    for (auto &item : m_draft.items)
        parseDates(item);
    recalculate();
    emit draftChanged();
    return true;
}

bool ImportService::confirm(bool allowDuplicate)
{
    if (!m_draft.ready || busy())
        return false;
    recalculate();
    if (m_draft.duplicate.isDuplicate() && !allowDuplicate)
        return false;
    if (products().isEmpty())
        return reject(rec::ErrorKind::InvalidInput, texts::importNothingToConfirm());
    for (const auto &draft : m_draft.items) {
        if (!draft.selected || draft.category == rec::ItemCategory::Irrelevant)
            continue;
        const auto &item = draft.item;
        if (item.count <= 0 || item.displayName.trimmed().isEmpty()
            || (!m_builtin->catalog->contains(item.canonicalName) && item.canonicalName != rec::uncategorizedName())
            || (item.manualDays && *item.manualDays <= 0) || (item.labelledDays && *item.labelledDays <= 0))
            return reject(rec::ErrorKind::InvalidInput, QStringLiteral("请检查商品名称、数量和保质天数"));
        if (draft.needsDateConfirmation())
            return reject(rec::ErrorKind::InvalidInput, texts::importNeedsBaseDate());
    }
    if (!m_store.transaction())
        return reject(rec::ErrorKind::Internal, QStringLiteral("无法保存，请重试"));
    auto record = m_draft.record;
    bool ok = m_store.insertImport(record) > 0;
    ok = ok && m_store.saveImportProducts(record.id, products());
    auto cache = m_store.aiEstimateCache();
    const auto rules = m_store.ruleBook();
    const auto pantry = m_store.pantryRules(m_builtin->catalog.get());
    for (auto draft : m_draft.items) {
        if (!draft.selected || draft.category == rec::ItemCategory::Irrelevant)
            continue;
        auto &item = draft.item;
        item.entryDate = m_clock.today();
        const auto resolution = resolveShelfLife(item.shelfLifeInput(),
                                                 { m_builtin->catalog.get(), m_builtin->shelfLife.get(), &rules, &cache });
        item.applyShelfLife(resolution);
        for (const auto &entry : resolution.newAiEstimates) {
            ok = ok && m_store.cacheAiEstimate(entry);
            cache.insert(entry.name, entry.storage, entry.days);
        }
        // Seasonings are stock too (常备调料); the pantry suggestion below still applies to them.
        const Amount amount = totalAmount(item.quantityText, item.count);
        item.amount = amount.value;
        item.unit = amount.unit;
        item.importId = record.id;
        ok = ok && m_store.insertItem(item) > 0;
        if (!m_builtin->catalog->contains(item.canonicalName))
            continue;
        if (draft.category == rec::ItemCategory::Seasoning || pantry.isPantry(item.canonicalName)) {
            if (auto suggestion = suggestionForPurchase(item.canonicalName, pantry)) {
                suggestion->importId = record.id;
                ok = ok && m_store.addSuggestion(*suggestion) > 0;
            }
        }
    }
    if (m_jobId)
        ok = ok && m_store.deleteImportJob(m_jobId);
    if (!ok || !m_store.commit()) {
        m_store.rollback();
        return reject(rec::ErrorKind::Internal, QStringLiteral("无法保存，请重试"));
    }
    m_received = false;
    m_payload = {};
    m_draft = {};
    m_jobId = 0;
    m_job = {};
    emit committed(record.id);
    return true;
}

bool ImportService::skipDuplicate()
{
    if (!m_received || busy() || !m_draft.duplicate.isDuplicate())
        return false;
    auto record = m_draft.record;
    record.images.clear(); // deleted with the draft below; the record only says what was skipped
    record.status = data::ImportStatus::SkippedDuplicate;
    record.duplicateOf = m_draft.duplicate.importId;
    if (m_store.insertImport(record) <= 0)
        return reject(rec::ErrorKind::Internal, QStringLiteral("无法保存，请重试"));
    discard();
    emit skipped(record.id);
    return true;
}

bool ImportService::discard()
{
    const auto requestId = m_requestId;
    m_requestId = 0;
    if (requestId && m_recognizer)
        m_recognizer->cancel(requestId);
    QStringList paths;
    for (const auto &image : m_payload.images)
        paths.append(image.localPath);
    const bool ok = dropJob(m_jobId, paths);
    m_jobId = 0;
    m_job = {};
    m_draft = {};
    m_received = false;
    if (ok)
        m_payload = {};
    return ok;
}

bool ImportService::dropJob(qint64 id, const QStringList &candidates)
{
    if (id)
        m_store.deleteImportJob(id);
    // Files may still be referenced by an older import (e.g. a caller reusing a payload).
    QSet<QString> referenced;
    for (const auto &record : m_store.imports(true)) {
        for (const auto &image : record.images)
            referenced.insert(QFileInfo(image.path).absoluteFilePath());
    }
    QStringList paths;
    for (const auto &path : candidates) {
        if (QFileInfo::exists(path) && !referenced.contains(QFileInfo(path).absoluteFilePath()))
            paths.append(path);
    }
    return paths.isEmpty() || m_store.discardImportFiles(paths);
}

bool ImportService::hasUnfinishedJob() const
{
    return !m_received && m_store.importJob().has_value();
}

bool ImportService::resume()
{
    if (m_received || busy() || !m_recognizer)
        return false;
    const auto stored = m_store.importJob();
    if (!stored)
        return false;
    m_jobId = stored->id;
    m_job = stored->job;
    if (!restoreJobPayload(stored->payload)) {
        discard();
        return reject(rec::ErrorKind::InvalidInput, QStringLiteral("导入的图片已不在，请重新导入"));
    }
    m_received = true;
    if (m_job.state() == ResumableJob::State::Done) {
        if (const auto result = resultFromJob()) {
            buildDraft(*result);
            return true;
        }
        m_job.reset(); // stored answers unreadable: recognize again
    }
    m_job.start();
    saveJob();
    m_batchesReported = false;
    m_requestId = m_recognizer->recognize(request());
    return true;
}

bool ImportService::discardUnfinishedJob()
{
    if (m_received || busy())
        return false;
    return dropStoredJobs();
}

bool ImportService::dropStoredJobs()
{
    // All rows, not just importJob(): a job whose rows cannot be restored is invisible there but
    // still owns images.
    bool ok = true;
    for (const auto &stored : m_store.importJobRows()) {
        QStringList paths;
        for (const auto &image : QJsonDocument::fromJson(stored.payload).object().value("images").toArray())
            paths.append(image.toObject().value("path").toString());
        ok = dropJob(stored.id, paths) && ok;
    }
    return ok;
}

}
