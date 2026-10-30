#include "data/store.h"

#include "data/database.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace whilefresh::data {

Q_LOGGING_CATEGORY(lcStore, "whilefresh.data")

using namespace whilefresh::core;

namespace {

QString nonNull(const QString &text)
{
    return text.isNull() ? QStringLiteral("") : text;
}

QVariant dateValue(const QDate &date)
{
    return date.isValid() ? QVariant(date.toString(Qt::ISODate)) : QVariant(QMetaType(QMetaType::QString));
}

QDate dateFrom(const QVariant &value)
{
    return value.isNull() ? QDate() : QDate::fromString(value.toString(), Qt::ISODate);
}

QVariant dateTimeValue(const QDateTime &dateTime)
{
    return dateTime.isValid() ? QVariant(dateTime.toUTC().toString(Qt::ISODateWithMs))
                              : QVariant(QMetaType(QMetaType::QString));
}

QDateTime dateTimeFrom(const QVariant &value)
{
    return value.isNull() ? QDateTime() : QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

QVariant intValue(std::optional<int> value)
{
    return value ? QVariant(*value) : QVariant(QMetaType(QMetaType::Int));
}

std::optional<int> intFrom(const QVariant &value)
{
    return value.isNull() ? std::nullopt : std::optional<int>(value.toInt());
}

QVariant boolValue(std::optional<bool> value)
{
    return value ? QVariant(*value ? 1 : 0) : QVariant(QMetaType(QMetaType::Int));
}

std::optional<bool> boolFrom(const QVariant &value)
{
    return value.isNull() ? std::nullopt : std::optional<bool>(value.toInt() != 0);
}

bool run(QSqlQuery &query)
{
    if (query.exec())
        return true;
    qCWarning(lcStore, "query failed: %s", qUtf8Printable(query.lastError().text()));
    return false;
}

bool run(QSqlQuery &query, const QString &sql)
{
    if (query.exec(sql))
        return true;
    qCWarning(lcStore, "query failed: %s", qUtf8Printable(query.lastError().text()));
    return false;
}

const QString itemColumns = QStringLiteral(
    "import_id, canonical_name, display_name, raw_name, category, quantity_text, count, storage, "
    "storage_source, use_first, status, status_changed_at, purchase_date, purchase_date_origin, "
    "purchase_date_text, entry_date, production_date, production_date_origin, production_date_text, "
    "package_expiry_date, package_expiry_origin, package_expiry_text, labelled_days, manual_days, "
    "ai_fridge_days, ai_freezer_days, ai_room_days, shelf_life_source, shelf_life_days, expiry_date, "
    "start_basis, shelf_life_note, created_at, updated_at, reference_conditions_confirmed, kind, amount, unit, "
    "snoozed_until");

void bindItem(QSqlQuery &query, const FridgeItem &item)
{
    query.bindValue(QStringLiteral(":import_id"),
                    item.importId > 0 ? QVariant(item.importId) : QVariant(QMetaType(QMetaType::LongLong)));
    query.bindValue(QStringLiteral(":canonical_name"), nonNull(item.canonicalName));
    query.bindValue(QStringLiteral(":display_name"), nonNull(item.displayName));
    query.bindValue(QStringLiteral(":raw_name"), nonNull(item.rawName));
    query.bindValue(QStringLiteral(":category"), nonNull(item.category));
    query.bindValue(QStringLiteral(":quantity_text"), nonNull(item.quantityText));
    query.bindValue(QStringLiteral(":count"), item.count);
    query.bindValue(QStringLiteral(":kind"), itemKindKey(item.kind));
    query.bindValue(QStringLiteral(":amount"), item.amount);
    query.bindValue(QStringLiteral(":unit"), nonNull(item.unit));
    query.bindValue(QStringLiteral(":snoozed_until"), dateValue(item.snoozedUntil));
    query.bindValue(QStringLiteral(":storage"), storageKey(item.storage));
    query.bindValue(QStringLiteral(":storage_source"), storageSourceKey(item.storageSource));
    query.bindValue(QStringLiteral(":use_first"), item.useFirst ? 1 : 0);
    query.bindValue(QStringLiteral(":status"), itemStatusKey(item.status));
    query.bindValue(QStringLiteral(":status_changed_at"), dateTimeValue(item.statusChangedAt));
    query.bindValue(QStringLiteral(":purchase_date"), dateValue(item.purchaseDate.date));
    query.bindValue(QStringLiteral(":purchase_date_origin"), dateOriginKey(item.purchaseDate.origin));
    query.bindValue(QStringLiteral(":purchase_date_text"), nonNull(item.purchaseDate.text));
    query.bindValue(QStringLiteral(":entry_date"), item.entryDate.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":production_date"), dateValue(item.productionDate.date));
    query.bindValue(QStringLiteral(":production_date_origin"), dateOriginKey(item.productionDate.origin));
    query.bindValue(QStringLiteral(":production_date_text"), nonNull(item.productionDate.text));
    query.bindValue(QStringLiteral(":package_expiry_date"), dateValue(item.packageExpiry.date));
    query.bindValue(QStringLiteral(":package_expiry_origin"), dateOriginKey(item.packageExpiry.origin));
    query.bindValue(QStringLiteral(":package_expiry_text"), nonNull(item.packageExpiry.text));
    query.bindValue(QStringLiteral(":labelled_days"), intValue(item.labelledDays));
    query.bindValue(QStringLiteral(":manual_days"), intValue(item.manualDays));
    query.bindValue(QStringLiteral(":ai_fridge_days"), intValue(item.aiDays.fridge));
    query.bindValue(QStringLiteral(":ai_freezer_days"), intValue(item.aiDays.freezer));
    query.bindValue(QStringLiteral(":ai_room_days"), intValue(item.aiDays.room));
    query.bindValue(QStringLiteral(":shelf_life_source"), shelfLifeSourceKey(item.shelfLifeSource));
    query.bindValue(QStringLiteral(":shelf_life_days"), intValue(item.shelfLifeDays));
    query.bindValue(QStringLiteral(":expiry_date"), dateValue(item.expiryDate));
    query.bindValue(QStringLiteral(":start_basis"), startBasisKey(item.startBasis));
    query.bindValue(QStringLiteral(":shelf_life_note"), nonNull(item.shelfLifeNote));
    query.bindValue(QStringLiteral(":reference_conditions_confirmed"), item.referenceConditionsConfirmed ? 1 : 0);
    query.bindValue(QStringLiteral(":created_at"), dateTimeValue(item.createdAt));
    query.bindValue(QStringLiteral(":updated_at"), dateTimeValue(item.updatedAt));
}

RecordedDate recordedDate(const QSqlQuery &query, const QString &prefix, const QString &textColumn)
{
    RecordedDate date;
    date.date = dateFrom(query.value(prefix));
    date.origin = dateOriginFromKey(query.value(prefix + QStringLiteral("_origin")).toString()).value_or(DateOrigin::None);
    date.text = query.value(textColumn).toString();
    return date;
}

FridgeItem itemFrom(const QSqlQuery &query)
{
    FridgeItem item;
    item.id = query.value(QStringLiteral("id")).toLongLong();
    item.importId = query.value(QStringLiteral("import_id")).toLongLong();
    item.canonicalName = query.value(QStringLiteral("canonical_name")).toString();
    item.displayName = query.value(QStringLiteral("display_name")).toString();
    item.rawName = query.value(QStringLiteral("raw_name")).toString();
    item.category = query.value(QStringLiteral("category")).toString();
    item.quantityText = query.value(QStringLiteral("quantity_text")).toString();
    item.count = query.value(QStringLiteral("count")).toInt();
    item.kind = itemKindFromKey(query.value(QStringLiteral("kind")).toString()).value_or(ItemKind::Food);
    item.amount = query.value(QStringLiteral("amount")).toDouble();
    item.unit = query.value(QStringLiteral("unit")).toString();
    item.snoozedUntil = dateFrom(query.value(QStringLiteral("snoozed_until")));
    item.storage = storageFromKey(query.value(QStringLiteral("storage")).toString()).value_or(Storage::Fridge);
    item.storageSource =
        storageSourceFromKey(query.value(QStringLiteral("storage_source")).toString()).value_or(StorageSource::Default);
    item.useFirst = query.value(QStringLiteral("use_first")).toInt() != 0;
    item.status = itemStatusFromKey(query.value(QStringLiteral("status")).toString()).value_or(ItemStatus::InStock);
    item.statusChangedAt = dateTimeFrom(query.value(QStringLiteral("status_changed_at")));
    item.purchaseDate = recordedDate(query, QStringLiteral("purchase_date"), QStringLiteral("purchase_date_text"));
    item.entryDate = dateFrom(query.value(QStringLiteral("entry_date")));
    item.productionDate =
        recordedDate(query, QStringLiteral("production_date"), QStringLiteral("production_date_text"));
    item.packageExpiry.date = dateFrom(query.value(QStringLiteral("package_expiry_date")));
    item.packageExpiry.text = query.value(QStringLiteral("package_expiry_text")).toString();
    item.packageExpiry.origin =
        dateOriginFromKey(query.value(QStringLiteral("package_expiry_origin")).toString()).value_or(DateOrigin::None);
    item.labelledDays = intFrom(query.value(QStringLiteral("labelled_days")));
    item.manualDays = intFrom(query.value(QStringLiteral("manual_days")));
    item.aiDays.fridge = intFrom(query.value(QStringLiteral("ai_fridge_days")));
    item.aiDays.freezer = intFrom(query.value(QStringLiteral("ai_freezer_days")));
    item.aiDays.room = intFrom(query.value(QStringLiteral("ai_room_days")));
    item.shelfLifeSource =
        shelfLifeSourceFromKey(query.value(QStringLiteral("shelf_life_source")).toString()).value_or(ShelfLifeSource::None);
    item.shelfLifeDays = intFrom(query.value(QStringLiteral("shelf_life_days")));
    item.expiryDate = dateFrom(query.value(QStringLiteral("expiry_date")));
    item.startBasis = startBasisFromKey(query.value(QStringLiteral("start_basis")).toString()).value_or(StartBasis::None);
    item.shelfLifeNote = query.value(QStringLiteral("shelf_life_note")).toString();
    item.referenceConditionsConfirmed = query.value(QStringLiteral("reference_conditions_confirmed")).toBool();
    item.createdAt = dateTimeFrom(query.value(QStringLiteral("created_at")));
    item.updatedAt = dateTimeFrom(query.value(QStringLiteral("updated_at")));
    return item;
}

QString placeholders(const QString &columns)
{
    QStringList names;
    for (const QString &column : columns.split(QLatin1Char(',')))
        names.append(QLatin1Char(':') + column.trimmed());
    return names.join(QStringLiteral(", "));
}

QString assignments(const QString &columns)
{
    QStringList pairs;
    for (const QString &column : columns.split(QLatin1Char(','))) {
        const QString name = column.trimmed();
        pairs.append(name + QStringLiteral(" = :") + name);
    }
    return pairs.join(QStringLiteral(", "));
}

}

QString importStatusKey(ImportStatus status)
{
    return status == ImportStatus::SkippedDuplicate ? QStringLiteral("skipped_duplicate") : QStringLiteral("confirmed");
}

Store::Store(Database &database, const Clock &clock, QString importsDirectory)
    : m_database(database), m_clock(clock), m_importsDirectory(std::move(importsDirectory))
{
}

QString Store::now() const
{
    return m_clock.now().toUTC().toString(Qt::ISODateWithMs);
}

bool Store::transaction()
{
    return m_database.transaction();
}

bool Store::commit()
{
    return m_database.commit();
}

void Store::rollback()
{
    m_database.rollback();
}

qint64 Store::insertItem(FridgeItem &item)
{
    const QDateTime stamp = m_clock.now();
    if (!item.createdAt.isValid())
        item.createdAt = stamp;
    item.updatedAt = stamp;
    if (!item.entryDate.isValid())
        item.entryDate = m_clock.today();
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO items (%1) VALUES (%2)").arg(itemColumns, placeholders(itemColumns)));
    bindItem(query, item);
    if (!run(query))
        return 0;
    item.id = query.lastInsertId().toLongLong();
    return item.id;
}

bool Store::updateItem(FridgeItem &item)
{
    item.updatedAt = m_clock.now();
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("UPDATE items SET %1 WHERE id = :id").arg(assignments(itemColumns)));
    bindItem(query, item);
    query.bindValue(QStringLiteral(":id"), item.id);
    return run(query) && query.numRowsAffected() == 1;
}

std::optional<FridgeItem> Store::item(qint64 id) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT * FROM items WHERE id = ?"));
    query.addBindValue(id);
    if (!run(query) || !query.next())
        return std::nullopt;
    return itemFrom(query);
}

QList<FridgeItem> Store::items(std::optional<ItemStatus> status) const
{
    QSqlQuery query(m_database.handle());
    if (status) {
        query.prepare(QStringLiteral("SELECT * FROM items WHERE status = ? ORDER BY id"));
        query.addBindValue(itemStatusKey(*status));
    } else {
        query.prepare(QStringLiteral("SELECT * FROM items ORDER BY id"));
    }
    QList<FridgeItem> list;
    if (!run(query))
        return list;
    while (query.next())
        list.append(itemFrom(query));
    return list;
}

QList<FridgeItem> Store::itemsForImport(qint64 importId) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT * FROM items WHERE import_id = ? ORDER BY id"));
    query.addBindValue(importId);
    QList<FridgeItem> list;
    if (!run(query))
        return list;
    while (query.next())
        list.append(itemFrom(query));
    return list;
}

qint64 Store::insertImport(ImportRecord &record)
{
    if (!record.createdAt.isValid())
        record.createdAt = m_clock.now();
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO imports (created_at, input_kind, origin, is_demo, truncated, text_length, "
                                 "source_type, status, duplicate_of) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(dateTimeValue(record.createdAt));
    query.addBindValue(nonNull(record.inputKind));
    query.addBindValue(nonNull(record.origin));
    query.addBindValue(record.isDemo ? 1 : 0);
    query.addBindValue(record.truncated ? 1 : 0);
    query.addBindValue(record.textLength);
    query.addBindValue(record.sourceType);
    query.addBindValue(importStatusKey(record.status));
    query.addBindValue(record.duplicateOf > 0 ? QVariant(record.duplicateOf) : QVariant(QMetaType(QMetaType::LongLong)));
    if (!run(query))
        return 0;
    record.id = query.lastInsertId().toLongLong();
    for (int i = 0; i < record.images.size(); ++i) {
        const ImportImageRecord &image = record.images.at(i);
        QSqlQuery insert(m_database.handle());
        insert.prepare(QStringLiteral("INSERT INTO import_images (import_id, position, path, sha256, mime_type, "
                                      "captured_at, captured_at_source) VALUES (?, ?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(record.id);
        insert.addBindValue(i);
        insert.addBindValue(nonNull(image.path));
        insert.addBindValue(nonNull(image.sha256));
        insert.addBindValue(nonNull(image.mimeType));
        insert.addBindValue(dateTimeValue(image.capturedAt));
        insert.addBindValue(nonNull(image.capturedAtSource));
        if (!run(insert))
            return 0;
    }
    return record.id;
}

std::optional<ImportRecord> Store::importRecord(qint64 id) const
{
    for (const ImportRecord &record : imports()) {
        if (record.id == id)
            return record;
    }
    return std::nullopt;
}

QList<ImportRecord> Store::imports(bool includeDeleted) const
{
    QList<ImportRecord> records;
    QHash<qint64, int> index;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT * FROM imports %1 ORDER BY created_at DESC, id DESC")
                        .arg(includeDeleted ? QString() : QStringLiteral("WHERE deleted_at IS NULL"))))
        return records;
    while (query.next()) {
        ImportRecord record;
        record.id = query.value(QStringLiteral("id")).toLongLong();
        record.createdAt = dateTimeFrom(query.value(QStringLiteral("created_at")));
        record.inputKind = query.value(QStringLiteral("input_kind")).toString();
        record.sourceType = query.value(QStringLiteral("source_type")).toString();
        record.origin = query.value(QStringLiteral("origin")).toString();
        record.isDemo = query.value(QStringLiteral("is_demo")).toInt() != 0;
        record.truncated = query.value(QStringLiteral("truncated")).toInt() != 0;
        record.textLength = query.value(QStringLiteral("text_length")).toInt();
        record.status = query.value(QStringLiteral("status")).toString() == importStatusKey(ImportStatus::SkippedDuplicate)
            ? ImportStatus::SkippedDuplicate : ImportStatus::Confirmed;
        record.duplicateOf = query.value(QStringLiteral("duplicate_of")).toLongLong();
        record.deletedAt = dateTimeFrom(query.value(QStringLiteral("deleted_at")));
        index.insert(record.id, records.size());
        records.append(record);
    }
    QSqlQuery images(m_database.handle());
    if (!run(images, QStringLiteral("SELECT * FROM import_images ORDER BY import_id, position")))
        return records;
    while (images.next()) {
        const auto it = index.constFind(images.value(QStringLiteral("import_id")).toLongLong());
        if (it == index.constEnd())
            continue;
        records[*it].images.append({ images.value(QStringLiteral("path")).toString(),
                                     images.value(QStringLiteral("sha256")).toString(),
                                     images.value(QStringLiteral("mime_type")).toString(),
                                     dateTimeFrom(images.value(QStringLiteral("captured_at"))),
                                     images.value(QStringLiteral("captured_at_source")).toString() });
    }
    return records;
}

QList<ImportFingerprint> Store::importFingerprints() const
{
    QList<ImportFingerprint> fingerprints;
    QHash<qint64, int> index;
    for (const ImportRecord &record : imports()) {
        ImportFingerprint fingerprint;
        fingerprint.importId = record.id;
        fingerprint.createdAt = record.createdAt;
        for (const ImportImageRecord &image : record.images)
            fingerprint.imageSha256.append(image.sha256);
        index.insert(record.id, fingerprints.size());
        fingerprints.append(fingerprint);
    }
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral(
            "SELECT import_id, canonical_name, quantity FROM import_products UNION ALL "
            "SELECT import_id, canonical_name, quantity_text || char(31) || count FROM items "
            "WHERE import_id IS NOT NULL AND import_id NOT IN (SELECT import_id FROM import_products)")))
        return fingerprints;
    while (query.next()) {
        const auto it = index.constFind(query.value(0).toLongLong());
        if (it != index.constEnd())
            fingerprints[*it].items.append({ query.value(1).toString(), query.value(2).toString() });
    }
    return fingerprints;
}

bool Store::saveImportProducts(qint64 id, const QList<QPair<QString, QString>> &products)
{
    for (const auto &product : products) {
        QSqlQuery query(m_database.handle());
        query.prepare(QStringLiteral("INSERT INTO import_products (import_id, canonical_name, quantity) VALUES (?, ?, ?)"));
        query.addBindValue(id);
        query.addBindValue(product.first);
        query.addBindValue(product.second);
        if (!run(query))
            return false;
    }
    return true;
}

bool Store::discardImportFiles(const QStringList &paths)
{
    const QString directory = QDir(m_importsDirectory).canonicalPath();
    if (directory.isEmpty())
        return false;
    for (const auto &path : paths) {
        const QFileInfo file(path);
        if (file.absoluteDir().canonicalPath() != directory || file.isSymLink())
            return false;
    }
    if (!transaction())
        return false;
    bool ok = true;
    for (const auto &path : paths) {
        QSqlQuery query(m_database.handle());
        query.prepare(QStringLiteral("INSERT OR IGNORE INTO file_cleanup (path) VALUES (?)"));
        query.addBindValue(QFileInfo(path).absoluteFilePath());
        ok = run(query) && ok;
    }
    if (!ok || !commit()) {
        rollback();
        return false;
    }
    return retryFileCleanup();
}

bool Store::retryFileCleanup()
{
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT path FROM file_cleanup WHERE path NOT IN (SELECT path FROM import_images)")))
        return false;
    QStringList paths;
    while (query.next())
        paths.append(query.value(0).toString());
    query.finish();
    bool ok = true;
    for (const auto &path : paths) {
        if (QFileInfo::exists(path) && !removeImportFile(path)) {
            ok = false;
            continue;
        }
        QSqlQuery done(m_database.handle());
        done.prepare(QStringLiteral("DELETE FROM file_cleanup WHERE path = ?"));
        done.addBindValue(path);
        ok = run(done) && ok;
    }
    return ok;
}

bool Store::removeImportFile(const QString &path) const
{
    if (m_importsDirectory.isEmpty() || path.isEmpty())
        return false;
    const QFileInfo file(path);
    const QString directory = QDir(m_importsDirectory).canonicalPath();
    if (directory.isEmpty() || file.absoluteDir().canonicalPath() != directory || !file.isFile())
        return false;
    return QFile::remove(file.absoluteFilePath());
}

bool Store::deleteImport(qint64 id)
{
    if (!importRecord(id) || !transaction())
        return false;
    QSqlQuery query(m_database.handle());
    bool ok = true;
    for (const QString &sql : { QStringLiteral("INSERT OR IGNORE INTO file_cleanup SELECT path FROM import_images WHERE import_id = ?"),
                                QStringLiteral("DELETE FROM import_products WHERE import_id = ?"),
                                QStringLiteral("DELETE FROM suggestions WHERE import_id = ? AND status = 'pending'"),
                                QStringLiteral("DELETE FROM items WHERE import_id = ? AND status = 'in_stock'"),
                                QStringLiteral("DELETE FROM import_images WHERE import_id = ?") }) {
        query.prepare(sql);
        query.addBindValue(id);
        ok = ok && run(query);
    }
    query.prepare(QStringLiteral("UPDATE imports SET deleted_at = ? WHERE id = ?"));
    query.addBindValue(now());
    query.addBindValue(id);
    ok = ok && run(query);
    if (!ok || !commit()) {
        rollback();
        return false;
    }
    return retryFileCleanup();
}

bool Store::clearImportHistory()
{
    if (!transaction())
        return false;
    QSqlQuery query(m_database.handle());
    const QString visible = QStringLiteral("SELECT id FROM imports WHERE deleted_at IS NULL");
    bool ok = run(query, QStringLiteral("INSERT OR IGNORE INTO file_cleanup SELECT path FROM import_images "
                                        "WHERE import_id IN (%1)").arg(visible))
        && run(query, QStringLiteral("DELETE FROM import_products WHERE import_id IN (%1)").arg(visible))
        && run(query, QStringLiteral("DELETE FROM import_images WHERE import_id IN (%1)").arg(visible));
    if (ok) {
        query.prepare(QStringLiteral("UPDATE imports SET deleted_at = ? WHERE deleted_at IS NULL"));
        query.addBindValue(now());
        ok = run(query);
    }
    if (!ok || !commit()) {
        rollback();
        return false;
    }
    return retryFileCleanup();
}

bool Store::deleteAllData()
{
    if (!transaction())
        return false;
    QSqlQuery query(m_database.handle());
    bool ok = run(query, QStringLiteral("INSERT OR IGNORE INTO file_cleanup SELECT path FROM import_images"));
    if (!m_importsDirectory.isEmpty()) {
        for (const auto &file : QDir(m_importsDirectory).entryInfoList(QDir::Files | QDir::Hidden | QDir::System)) {
            query.prepare(QStringLiteral("INSERT OR IGNORE INTO file_cleanup (path) VALUES (?)"));
            query.addBindValue(file.absoluteFilePath());
            ok = run(query) && ok;
        }
    }
    for (const QString &table : { QStringLiteral("import_products"), QStringLiteral("suggestions"), QStringLiteral("items"), QStringLiteral("import_images"),
                                  QStringLiteral("imports"), QStringLiteral("shelf_life_rules"),
                                  QStringLiteral("storage_rules"), QStringLiteral("ai_shelf_life_cache"),
                                  QStringLiteral("pantry_overrides"), QStringLiteral("shopping_items"),
                                  QStringLiteral("settings"), QStringLiteral("name_mappings"), QStringLiteral("preferences"),
                                  QStringLiteral("import_job_batches"), QStringLiteral("import_jobs"),
                                  QStringLiteral("cleanup_sessions") })
        ok = ok && run(query, QStringLiteral("DELETE FROM ") + table);
    if (!ok || !commit()) {
        rollback();
        return false;
    }
    return retryFileCleanup();
}

// Job and batch rows change together: a half-written job (say "done" with pending batches) could
// not be restored, so every write is one transaction.
qint64 Store::insertImportJob(const QByteArray &payload, const ResumableJob &job)
{
    if (!transaction())
        return 0;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO import_jobs (state, payload, created_at, updated_at) VALUES (?, ?, ?, ?)"));
    query.addBindValue(ResumableJob::stateKey(job.state()));
    query.addBindValue(QString::fromUtf8(payload));
    query.addBindValue(now());
    query.addBindValue(now());
    const qint64 id = run(query) ? query.lastInsertId().toLongLong() : 0;
    if (!id || !writeImportJob(id, job) || !commit()) {
        rollback();
        return 0;
    }
    return id;
}

bool Store::saveImportJob(qint64 id, const ResumableJob &job)
{
    if (!transaction())
        return false;
    if (!writeImportJob(id, job) || !commit()) {
        rollback();
        return false;
    }
    return true;
}

bool Store::writeImportJob(qint64 id, const ResumableJob &job)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("UPDATE import_jobs SET state = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(ResumableJob::stateKey(job.state()));
    query.addBindValue(now());
    query.addBindValue(id);
    if (!run(query) || query.numRowsAffected() != 1)
        return false;
    query.prepare(QStringLiteral("DELETE FROM import_job_batches WHERE job_id = ? AND batch_index >= ?"));
    query.addBindValue(id);
    query.addBindValue(job.batchCount());
    bool ok = run(query);
    for (int i = 0; i < job.batchCount() && ok; ++i) {
        const auto &batch = job.batches().at(i);
        query.prepare(QStringLiteral("INSERT INTO import_job_batches (job_id, batch_index, state, result, updated_at) "
                                     "VALUES (?, ?, ?, ?, ?) ON CONFLICT (job_id, batch_index) DO UPDATE SET "
                                     "state = excluded.state, result = excluded.result, updated_at = excluded.updated_at"));
        query.addBindValue(id);
        query.addBindValue(i);
        query.addBindValue(ResumableJob::batchStateKey(batch.state));
        query.addBindValue(batch.result.isNull() ? QVariant(QMetaType(QMetaType::QByteArray)) : QVariant(batch.result));
        query.addBindValue(now());
        ok = run(query);
    }
    return ok;
}

std::optional<ImportJobRecord> Store::importJob() const
{
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT id, state, payload, created_at FROM import_jobs ORDER BY id DESC LIMIT 1"))
        || !query.next())
        return std::nullopt;
    ImportJobRecord record;
    record.id = query.value(0).toLongLong();
    const auto state = ResumableJob::stateFromKey(query.value(1).toString());
    record.payload = query.value(2).toString().toUtf8();
    record.createdAt = dateTimeFrom(query.value(3));
    QSqlQuery rows(m_database.handle());
    rows.prepare(QStringLiteral("SELECT state, result FROM import_job_batches WHERE job_id = ? ORDER BY batch_index"));
    rows.addBindValue(record.id);
    if (!state || !run(rows))
        return std::nullopt;
    QList<ResumableJob::Batch> batches;
    while (rows.next()) {
        const auto batchState = ResumableJob::batchStateFromKey(rows.value(0).toString());
        if (!batchState)
            return std::nullopt;
        batches.append({ *batchState, rows.value(1).toByteArray() });
    }
    const auto job = ResumableJob::restore(*state, batches);
    if (!job) {
        qCWarning(lcStore, "import job %lld has inconsistent batches", record.id);
        return std::nullopt;
    }
    record.job = *job;
    return record;
}

QList<ImportJobRecord> Store::importJobRows() const
{
    QList<ImportJobRecord> rows;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT id, payload, created_at FROM import_jobs ORDER BY id")))
        return rows;
    while (query.next()) {
        ImportJobRecord record;
        record.id = query.value(0).toLongLong();
        record.payload = query.value(1).toString().toUtf8();
        record.createdAt = dateTimeFrom(query.value(2));
        rows.append(record);
    }
    return rows;
}

bool Store::deleteImportJob(qint64 id)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM import_jobs WHERE id = ?"));
    query.addBindValue(id);
    return run(query);
}

QList<ShelfLifeRule> Store::shelfLifeRules() const
{
    QList<ShelfLifeRule> rules;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT canonical_name, storage, days FROM shelf_life_rules ORDER BY canonical_name, storage")))
        return rules;
    while (query.next()) {
        if (const auto storage = storageFromKey(query.value(1).toString()))
            rules.append({ query.value(0).toString(), *storage, query.value(2).toInt() });
    }
    return rules;
}

bool Store::saveShelfLifeRule(const ShelfLifeRule &rule)
{
    if (!isRuleEligibleName(rule.name) || rule.days <= 0)
        return false;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO shelf_life_rules (canonical_name, storage, days, updated_at) VALUES (?, ?, ?, ?) "
                                 "ON CONFLICT (canonical_name, storage) DO UPDATE SET days = excluded.days, "
                                 "updated_at = excluded.updated_at"));
    query.addBindValue(rule.name);
    query.addBindValue(storageKey(rule.storage));
    query.addBindValue(rule.days);
    query.addBindValue(now());
    return run(query);
}

bool Store::deleteShelfLifeRule(const QString &name, Storage storage)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM shelf_life_rules WHERE canonical_name = ? AND storage = ?"));
    query.addBindValue(name);
    query.addBindValue(storageKey(storage));
    return run(query) && query.numRowsAffected() > 0;
}

QList<StorageRule> Store::storageRules() const
{
    QList<StorageRule> rules;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT canonical_name, storage FROM storage_rules ORDER BY canonical_name")))
        return rules;
    while (query.next()) {
        if (const auto storage = storageFromKey(query.value(1).toString()))
            rules.append({ query.value(0).toString(), *storage });
    }
    return rules;
}

bool Store::saveStorageRule(const StorageRule &rule)
{
    if (!isRuleEligibleName(rule.name))
        return false;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO storage_rules (canonical_name, storage, updated_at) VALUES (?, ?, ?) "
                                 "ON CONFLICT (canonical_name) DO UPDATE SET storage = excluded.storage, "
                                 "updated_at = excluded.updated_at"));
    query.addBindValue(rule.name);
    query.addBindValue(storageKey(rule.storage));
    query.addBindValue(now());
    return run(query);
}

bool Store::deleteStorageRule(const QString &name)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM storage_rules WHERE canonical_name = ?"));
    query.addBindValue(name);
    return run(query) && query.numRowsAffected() > 0;
}

RuleBook Store::ruleBook() const
{
    RuleBook book;
    for (const ShelfLifeRule &rule : shelfLifeRules())
        book.setShelfLife(rule);
    for (const StorageRule &rule : storageRules())
        book.setStorage(rule);
    return book;
}

AiEstimateCache Store::aiEstimateCache() const
{
    AiEstimateCache cache;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT canonical_name, storage, days FROM ai_shelf_life_cache")))
        return cache;
    while (query.next()) {
        if (const auto storage = storageFromKey(query.value(1).toString()))
            cache.insert(query.value(0).toString(), *storage, query.value(2).toInt());
    }
    return cache;
}

bool Store::cacheAiEstimate(const AiCacheEntry &entry)
{
    if (!isRuleEligibleName(entry.name) || entry.days <= 0)
        return false;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO ai_shelf_life_cache (canonical_name, storage, days, created_at) "
                                 "VALUES (?, ?, ?, ?)"));
    query.addBindValue(entry.name);
    query.addBindValue(storageKey(entry.storage));
    query.addBindValue(entry.days);
    query.addBindValue(now());
    return run(query);
}

QList<PantryOverride> Store::pantryOverrides() const
{
    QList<PantryOverride> overrides;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT canonical_name, is_pantry, is_bulk, is_out FROM pantry_overrides ORDER BY canonical_name")))
        return overrides;
    while (query.next())
        overrides.append({ query.value(0).toString(), boolFrom(query.value(1)), boolFrom(query.value(2)),
                           query.value(3).toInt() != 0 });
    return overrides;
}

bool Store::savePantryOverride(const PantryOverride &override)
{
    if (override.name.trimmed().isEmpty())
        return false;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO pantry_overrides (canonical_name, is_pantry, is_bulk, is_out, updated_at) "
                                 "VALUES (?, ?, ?, ?, ?) ON CONFLICT (canonical_name) DO UPDATE SET "
                                 "is_pantry = excluded.is_pantry, is_bulk = excluded.is_bulk, "
                                 "is_out = excluded.is_out, updated_at = excluded.updated_at"));
    query.addBindValue(override.name);
    query.addBindValue(boolValue(override.pantry));
    query.addBindValue(boolValue(override.bulk));
    query.addBindValue(override.out ? 1 : 0);
    query.addBindValue(now());
    return run(query);
}

bool Store::deletePantryOverride(const QString &name)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM pantry_overrides WHERE canonical_name = ?"));
    query.addBindValue(name);
    return run(query);
}

PantryRules Store::pantryRules(const IngredientCatalog *catalog) const
{
    PantryRules rules(catalog);
    for (const PantryOverride &override : pantryOverrides())
        rules.setOverride(override);
    return rules;
}

qint64 Store::addSuggestion(Suggestion &suggestion)
{
    QSqlQuery existing(m_database.handle());
    existing.prepare(QStringLiteral("SELECT id FROM suggestions WHERE status = 'pending' AND kind = ? AND canonical_name = ?"));
    existing.addBindValue(suggestionKindKey(suggestion.kind));
    existing.addBindValue(suggestion.name);
    if (run(existing) && existing.next()) {
        suggestion.id = existing.value(0).toLongLong();
        return suggestion.id;
    }
    if (!suggestion.createdAt.isValid())
        suggestion.createdAt = m_clock.now();
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO suggestions (kind, canonical_name, message, import_id, created_at) "
                                 "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(suggestionKindKey(suggestion.kind));
    query.addBindValue(suggestion.name);
    query.addBindValue(nonNull(suggestion.message));
    query.addBindValue(suggestion.importId > 0 ? QVariant(suggestion.importId) : QVariant(QMetaType(QMetaType::LongLong)));
    query.addBindValue(dateTimeValue(suggestion.createdAt));
    if (!run(query))
        return 0;
    suggestion.id = query.lastInsertId().toLongLong();
    return suggestion.id;
}

namespace {

Suggestion suggestionFrom(const QSqlQuery &query)
{
    Suggestion suggestion;
    suggestion.id = query.value(QStringLiteral("id")).toLongLong();
    suggestion.kind = suggestionKindFromKey(query.value(QStringLiteral("kind")).toString())
                          .value_or(SuggestionKind::MarkPantryHave);
    suggestion.name = query.value(QStringLiteral("canonical_name")).toString();
    suggestion.message = query.value(QStringLiteral("message")).toString();
    suggestion.importId = query.value(QStringLiteral("import_id")).toLongLong();
    suggestion.createdAt = dateTimeFrom(query.value(QStringLiteral("created_at")));
    return suggestion;
}

}

QList<Suggestion> Store::pendingSuggestions() const
{
    QList<Suggestion> list;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT * FROM suggestions WHERE status = 'pending' ORDER BY id")))
        return list;
    while (query.next())
        list.append(suggestionFrom(query));
    return list;
}

std::optional<Suggestion> Store::suggestion(qint64 id) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT * FROM suggestions WHERE id = ? AND status = 'pending'"));
    query.addBindValue(id);
    if (!run(query) || !query.next())
        return std::nullopt;
    return suggestionFrom(query);
}

bool Store::resolveSuggestion(qint64 id, bool accepted)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("UPDATE suggestions SET status = ?, resolved_at = ? WHERE id = ? AND status = 'pending'"));
    query.addBindValue(accepted ? QStringLiteral("accepted") : QStringLiteral("dismissed"));
    query.addBindValue(now());
    query.addBindValue(id);
    return run(query) && query.numRowsAffected() == 1;
}

qint64 Store::addShoppingEntry(ShoppingEntry &entry)
{
    if (entry.names.isEmpty())
        return 0;
    if (!entry.createdAt.isValid())
        entry.createdAt = m_clock.now();
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO shopping_items (names, dedupe_key, category, origin, note, created_at) "
                                 "VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(entry.names)).toJson(QJsonDocument::Compact)));
    query.addBindValue(entry.dedupeKey());
    query.addBindValue(nonNull(entry.category));
    query.addBindValue(shoppingOriginKey(entry.origin));
    query.addBindValue(nonNull(entry.note));
    query.addBindValue(dateTimeValue(entry.createdAt));
    if (!run(query) || query.numRowsAffected() == 0)
        return 0;
    entry.id = query.lastInsertId().toLongLong();
    return entry.id;
}

QList<ShoppingEntry> Store::shoppingEntries() const
{
    QList<ShoppingEntry> list;
    QSqlQuery query(m_database.handle());
    if (!run(query, QStringLiteral("SELECT id, names, category, origin, note, created_at FROM shopping_items ORDER BY id")))
        return list;
    while (query.next()) {
        ShoppingEntry entry;
        entry.id = query.value(0).toLongLong();
        for (const QJsonValue &name : QJsonDocument::fromJson(query.value(1).toString().toUtf8()).array())
            entry.names.append(name.toString());
        entry.category = query.value(2).toString();
        entry.origin = shoppingOriginFromKey(query.value(3).toString()).value_or(ShoppingOrigin::Manual);
        entry.note = query.value(4).toString();
        entry.createdAt = dateTimeFrom(query.value(5));
        list.append(entry);
    }
    return list;
}

bool Store::removeShoppingEntry(qint64 id)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM shopping_items WHERE id = ?"));
    query.addBindValue(id);
    return run(query) && query.numRowsAffected() == 1;
}

bool Store::clearShoppingList()
{
    QSqlQuery query(m_database.handle());
    return run(query, QStringLiteral("DELETE FROM shopping_items"));
}

QString Store::setting(const QString &key, const QString &fallback) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!run(query) || !query.next())
        return fallback;
    return query.value(0).toString();
}

bool Store::setSetting(const QString &key, const QString &value)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (?, ?) "
                                 "ON CONFLICT (key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(key);
    query.addBindValue(nonNull(value));
    return run(query);
}

}
