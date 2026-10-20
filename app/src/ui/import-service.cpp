#include "ui/import-service.h"

#include "core/texts.h"
#include "platform/import-files.h"
#include "recognition/vocabulary.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>

namespace whilefresh::ui {
using namespace core;
namespace rec = recognition;

namespace {

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
    // Build from the domain catalog: recognition's JSON reader predates the category-array format.
    auto vocabulary = std::make_shared<rec::Vocabulary>();
    for (const auto &code : m_builtin->catalog->categoryOrder())
        vocabulary->addCategory(code, m_builtin->catalog->categoryDisplayName(code));
    for (const auto &entry : m_builtin->catalog->entries())
        vocabulary->add({ entry.name, entry.category, entry.aliases });
    m_vocabulary = vocabulary;
    connect(&recognizer, &rec::RecognitionService::succeeded, this, &ImportService::succeeded);
    connect(&recognizer, &rec::RecognitionService::failed, this, &ImportService::recognitionFailed);
}

ImportService::~ImportService() { discard(); }
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
    m_payload = payload;
    m_received = true;
    m_draft = {};
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
    rec::RecognitionRequest request;
    request.vocabulary = m_vocabulary;
    request.kind = rec::inputKindFromString(m_draft.record.inputKind).value_or(rec::InputKind::Screenshot);
    request.text = m_payload.text;
    if (m_payload.kind == platform::ImportPayload::Kind::Images) {
        // A failed receive must not be usable as a partially validated upload.
        if (m_payload.images.isEmpty() || m_draft.record.images.size() != m_payload.images.size())
            return reject(rec::ErrorKind::InvalidInput, texts::importEmptyInput());
        for (const auto &image : m_payload.images)
            request.images.append({ image.localPath, image.capturedAt.date() });
    } else if (m_payload.text.trimmed().isEmpty()) {
        return reject(rec::ErrorKind::InvalidInput, texts::importEmptyInput());
    }
    m_requestId = m_recognizer->recognize(request);
    return true;
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

void ImportService::succeeded(quint64 id, const rec::RecognitionResult &result)
{
    if (id != m_requestId || !m_received)
        return;
    m_requestId = 0;
    m_draft.record.isDemo = m_demo || result.isDemo;
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
        if (draft.category == rec::ItemCategory::Seasoning && !m_builtin->catalog->contains(item.canonicalName))
            return reject(rec::ErrorKind::InvalidInput, QStringLiteral("请为调料选择标准食材名，或改为食材入库"));
        if (draft.category == rec::ItemCategory::Food && draft.needsDateConfirmation())
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
        if (draft.category == rec::ItemCategory::Food) {
            item.importId = record.id;
            ok = ok && m_store.insertItem(item) > 0;
        }
        if (draft.category == rec::ItemCategory::Seasoning || pantry.isPantry(item.canonicalName)) {
            if (auto suggestion = suggestionForPurchase(item.canonicalName, pantry)) {
                suggestion->importId = record.id;
                ok = ok && m_store.addSuggestion(*suggestion) > 0;
            }
        }
    }
    if (!ok || !m_store.commit()) {
        m_store.rollback();
        return reject(rec::ErrorKind::Internal, QStringLiteral("无法保存，请重试"));
    }
    m_received = false;
    m_payload = {};
    m_draft = {};
    emit committed(record.id);
    return true;
}

bool ImportService::discard()
{
    const auto requestId = m_requestId;
    m_requestId = 0;
    if (requestId && m_recognizer)
        m_recognizer->cancel(requestId);
    QStringList paths;
    // Files may still be referenced by an older import (e.g. a caller reusing a payload).
    QSet<QString> referenced;
    for (const auto &record : m_store.imports()) {
        for (const auto &image : record.images)
            referenced.insert(QFileInfo(image.path).absoluteFilePath());
    }
    for (const auto &image : m_payload.images) {
        if (QFileInfo::exists(image.localPath) && !referenced.contains(QFileInfo(image.localPath).absoluteFilePath()))
            paths.append(image.localPath);
    }
    const bool ok = paths.isEmpty() || m_store.discardImportFiles(paths);
    m_draft = {};
    m_received = false;
    if (ok)
        m_payload = {};
    return ok;
}

}
