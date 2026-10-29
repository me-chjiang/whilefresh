#include "data/database.h"
#include "data/store.h"

#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace whilefresh::core;
using namespace whilefresh::data;

namespace {

bool touch(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("x") == 1;
}

FridgeItem sampleItem(qint64 importId, const QString &name, const QString &quantity)
{
    FridgeItem item;
    item.importId = importId;
    item.canonicalName = name;
    item.displayName = name;
    item.quantityText = quantity;
    item.category = QStringLiteral("vegetable");
    return item;
}

}

class StoreTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_database;
    std::unique_ptr<FixedClock> m_clock;
    std::unique_ptr<Store> m_store;

    QString importsDir() const { return m_dir->filePath(QStringLiteral("imports")); }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(QDir().mkpath(importsDir()));
        m_database = std::make_unique<Database>(QStringLiteral(":memory:"));
        QVERIFY(m_database->open());
        QVERIFY(m_database->migrate());
        m_clock = std::make_unique<FixedClock>(QDateTime(QDate(2026, 10, 21), QTime(10, 0)));
        m_store = std::make_unique<Store>(*m_database, *m_clock, importsDir());
    }

    void cleanup()
    {
        m_store.reset();
        m_database.reset();
        m_dir.reset();
    }

    void itemRoundTrip()
    {
        FridgeItem item = sampleItem(0, QStringLiteral("牛奶"), QStringLiteral("950ml"));
        item.rawName = QStringLiteral("全脂鲜牛奶");
        item.storage = Storage::Fridge;
        item.storageSource = StorageSource::Reference;
        item.useFirst = true;
        item.purchaseDate = { QDate(2026, 10, 20), DateOrigin::Visible, QStringLiteral("10月20日") };
        item.productionDate = { QDate(), DateOrigin::None, QStringLiteral("看不清") };
        item.packageExpiry = { QDate(2026, 11, 3), DateOrigin::User, {} };
        item.labelledDays = 7;
        item.aiDays = { 7, std::nullopt, 1 };
        item.shelfLifeSource = ShelfLifeSource::Package;
        item.shelfLifeDays = 14;
        item.expiryDate = QDate(2026, 11, 3);
        item.startBasis = StartBasis::PurchaseDate;
        item.shelfLifeNote = QStringLiteral("包装到期日");
        item.kind = ItemKind::StapleFood;
        item.amount = 950;
        item.unit = QStringLiteral("ml");
        item.snoozedUntil = QDate(2026, 10, 22);
        QVERIFY(m_store->insertItem(item) > 0);
        QCOMPARE(item.entryDate, QDate(2026, 10, 21)); // filled from the clock

        const auto loaded = m_store->item(item.id);
        QVERIFY(loaded);
        QCOMPARE(loaded->importId, qint64(0));
        QCOMPARE(loaded->rawName, item.rawName);
        QCOMPARE(loaded->storageSource, StorageSource::Reference);
        QVERIFY(loaded->useFirst);
        QCOMPARE(loaded->purchaseDate.date, QDate(2026, 10, 20));
        QCOMPARE(loaded->purchaseDate.origin, DateOrigin::Visible);
        QCOMPARE(loaded->productionDate.text, QStringLiteral("看不清"));
        QVERIFY(!loaded->productionDate.isSet());
        QCOMPARE(loaded->packageExpiry.origin, DateOrigin::User);
        QCOMPARE(loaded->labelledDays, std::optional<int>(7));
        QVERIFY(!loaded->manualDays);
        QCOMPARE(loaded->aiDays, item.aiDays);
        QCOMPARE(loaded->shelfLifeSource, ShelfLifeSource::Package);
        QCOMPARE(loaded->expiryDate, QDate(2026, 11, 3));
        QCOMPARE(loaded->startBasis, StartBasis::PurchaseDate);
        QCOMPARE(loaded->createdAt, m_clock->now());
        QCOMPARE(loaded->kind, ItemKind::StapleFood);
        QCOMPARE(loaded->amount, 950.0);
        QCOMPARE(loaded->unit, QStringLiteral("ml"));
        QCOMPARE(loaded->snoozedUntil, QDate(2026, 10, 22));
        QVERIFY(loaded->isSnoozed(QDate(2026, 10, 21)));
        QVERIFY(!loaded->isSnoozed(QDate(2026, 10, 22)));

        FridgeItem changed = *loaded;
        changed.status = ItemStatus::Used;
        QVERIFY(m_store->updateItem(changed));
        QVERIFY(m_store->items().isEmpty());
        QCOMPARE(m_store->items(std::nullopt).size(), 1);
        QCOMPARE(m_store->items(ItemStatus::Used).size(), 1);
        changed.status = ItemStatus::Discarded;
        QVERIFY(m_store->updateItem(changed));
        QCOMPARE(m_store->items(ItemStatus::Discarded).size(), 1);
    }

    void deleteImportKeepsWhatWasEaten()
    {
        const QString image = QDir(importsDir()).filePath(QStringLiteral("a.jpg"));
        const QString other = QDir(importsDir()).filePath(QStringLiteral("b.jpg"));
        const QString outside = m_dir->filePath(QStringLiteral("outside.jpg"));
        QVERIFY(touch(image) && touch(other) && touch(outside));

        ImportRecord first;
        first.inputKind = QStringLiteral("screenshot");
        first.images = { { image, QStringLiteral("sha-a"), QStringLiteral("image/jpeg"), m_clock->now(), QStringLiteral("Exif") },
                         { outside, QStringLiteral("sha-x"), {}, {}, {} } };
        QVERIFY(m_store->insertImport(first) > 0);
        QVERIFY(m_store->saveImportProducts(first.id, { { QStringLiteral("土豆"), QStringLiteral("500g") } }));
        ImportRecord second;
        second.inputKind = QStringLiteral("screenshot");
        second.images = { { other, QStringLiteral("sha-b"), {}, {}, {} } };
        QVERIFY(m_store->insertImport(second) > 0);

        FridgeItem inStock = sampleItem(first.id, QStringLiteral("土豆"), QStringLiteral("500g"));
        FridgeItem used = sampleItem(first.id, QStringLiteral("西红柿"), QStringLiteral("500g"));
        used.status = ItemStatus::Used;
        FridgeItem discarded = sampleItem(first.id, QStringLiteral("菠菜"), QStringLiteral("300g"));
        discarded.status = ItemStatus::Discarded;
        FridgeItem fromSecond = sampleItem(second.id, QStringLiteral("西红柿"), QStringLiteral("500g"));
        FridgeItem manual = sampleItem(0, QStringLiteral("鸡蛋"), QStringLiteral("10枚"));
        for (FridgeItem *item : { &inStock, &used, &discarded, &fromSecond, &manual })
            QVERIFY(m_store->insertItem(*item) > 0);
        Suggestion suggestion{ 0, SuggestionKind::MarkPantryHave, QStringLiteral("生抽"), QStringLiteral("?"), first.id, {} };
        QVERIFY(m_store->addSuggestion(suggestion));

        const auto loaded = m_store->importRecord(first.id);
        QVERIFY(loaded);
        QCOMPARE(loaded->images.size(), 2);
        QCOMPARE(loaded->images.first().capturedAtSource, QStringLiteral("Exif"));

        QVERIFY(!m_store->deleteImport(first.id)); // unsafe external path stays queued, never deleted
        QVERIFY(!QFile::exists(image));
        QVERIFY(QFile::exists(outside)); // never touches files outside the imports directory
        QVERIFY(QFile::exists(other));

        // Only what was still in the fridge goes; manual and other imports' items stay.
        QVERIFY(!m_store->item(inStock.id));
        QCOMPARE(m_store->items().size(), 2);
        QVERIFY(m_store->item(fromSecond.id));
        QVERIFY(m_store->item(manual.id));
        const auto kept = m_store->itemsForImport(first.id);
        QCOMPARE(kept.size(), 2);
        for (const auto &item : kept) {
            QCOMPARE(item.importId, first.id);
            QVERIFY(item.status == ItemStatus::Used || item.status == ItemStatus::Discarded);
        }
        QCOMPARE(m_store->pendingSuggestions().size(), 0);

        // Hidden from the history and from duplicate detection, but the row remains.
        QVERIFY(!m_store->importRecord(first.id));
        QCOMPARE(m_store->imports().size(), 1);
        const auto all = m_store->imports(true);
        QCOMPARE(all.size(), 2);
        const auto deleted = std::find_if(all.begin(), all.end(), [&](const ImportRecord &r) { return r.id == first.id; });
        QCOMPARE(deleted->deletedAt, m_clock->now());
        QVERIFY(deleted->images.isEmpty());
        QCOMPARE(m_store->importFingerprints().size(), 1);
        QCOMPARE(m_store->importFingerprints().first().importId, second.id);
        QVERIFY(!m_store->deleteImport(first.id));
    }

    void clearingHistoryNeverEmptiesTheFridge()
    {
        const QString a = QDir(importsDir()).filePath(QStringLiteral("a.jpg"));
        const QString b = QDir(importsDir()).filePath(QStringLiteral("b.jpg"));
        QVERIFY(touch(a) && touch(b));
        QList<qint64> ids;
        for (const QString &path : { a, b }) {
            ImportRecord record;
            record.inputKind = QStringLiteral("screenshot");
            record.images = { { path, path.right(5), {}, {}, {} } };
            QVERIFY(m_store->insertImport(record) > 0);
            FridgeItem item = sampleItem(record.id, QStringLiteral("土豆"), QStringLiteral("500g"));
            QVERIFY(m_store->insertItem(item) > 0);
            ids.append(record.id);
        }
        FridgeItem manual = sampleItem(0, QStringLiteral("鸡蛋"), QStringLiteral("10枚"));
        QVERIFY(m_store->insertItem(manual) > 0);

        QVERIFY(m_store->clearImportHistory());
        QVERIFY(m_store->imports().isEmpty());
        QCOMPARE(m_store->imports(true).size(), 2);
        QCOMPARE(m_store->items().size(), 3);
        for (qint64 id : ids)
            QCOMPARE(m_store->itemsForImport(id).size(), 1);
        QVERIFY(!QFile::exists(a));
        QVERIFY(!QFile::exists(b));
        QVERIFY(m_store->importFingerprints().isEmpty());
    }

    void skippedDuplicateRecord()
    {
        ImportRecord original;
        original.inputKind = QStringLiteral("screenshot");
        QVERIFY(m_store->insertImport(original) > 0);
        ImportRecord skipped;
        skipped.inputKind = QStringLiteral("screenshot");
        skipped.status = ImportStatus::SkippedDuplicate;
        skipped.duplicateOf = original.id;
        QVERIFY(m_store->insertImport(skipped) > 0);
        const auto loaded = m_store->importRecord(skipped.id);
        QVERIFY(loaded);
        QCOMPARE(loaded->status, ImportStatus::SkippedDuplicate);
        QCOMPARE(loaded->duplicateOf, original.id);
        QCOMPARE(m_store->importRecord(original.id)->status, ImportStatus::Confirmed);
        QCOMPARE(m_store->importRecord(original.id)->duplicateOf, qint64(0));
    }

    void importJobRoundTrip()
    {
        QVERIFY(!m_store->importJob());
        ResumableJob job;
        QVERIFY(job.start());
        const qint64 id = m_store->insertImportJob(R"({"images":[]})", job);
        QVERIFY(id > 0);
        QVERIFY(job.plan(3));
        QVERIFY(job.completeBatch(0, "first"));
        QVERIFY(m_store->saveImportJob(id, job));

        auto loaded = m_store->importJob();
        QVERIFY(loaded);
        QCOMPARE(loaded->id, id);
        QCOMPARE(loaded->payload, QByteArray(R"({"images":[]})"));
        QCOMPARE(loaded->createdAt, m_clock->now());
        // Saved while running: the process died with a request in flight.
        QCOMPARE(loaded->job.state(), ResumableJob::State::Failed);
        QCOMPARE(loaded->job.batchCount(), 3);
        QCOMPARE(loaded->job.nextBatch(), 1);
        QCOMPARE(loaded->job.results(), QList<QByteArray>{ "first" });

        job.reset();
        QVERIFY(job.start());
        QVERIFY(job.plan(1));
        QVERIFY(job.completeBatch(0, "whole"));
        QVERIFY(m_store->saveImportJob(id, job));
        loaded = m_store->importJob();
        QCOMPARE(loaded->job.state(), ResumableJob::State::Done);
        QCOMPARE(loaded->job.results(), QList<QByteArray>{ "whole" });

        QVERIFY(m_store->deleteImportJob(id));
        QVERIFY(!m_store->importJob());
        QSqlQuery query(m_database->handle());
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM import_job_batches")) && query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }

    void deleteAllData()
    {
        const QString image = QDir(importsDir()).filePath(QStringLiteral("a.jpg"));
        const QString stray = QDir(importsDir()).filePath(QStringLiteral("old.part"));
        QVERIFY(touch(image) && touch(stray));
        ImportRecord record;
        record.inputKind = QStringLiteral("receipt");
        record.images = { { image, QStringLiteral("sha"), {}, {}, {} } };
        m_store->insertImport(record);
        FridgeItem item = sampleItem(record.id, QStringLiteral("土豆"), {});
        m_store->insertItem(item);
        m_store->saveShelfLifeRule({ QStringLiteral("土豆"), Storage::Room, 20 });
        m_store->setSetting(QStringLiteral("demo"), QStringLiteral("1"));
        ShoppingEntry entry;
        entry.names = { QStringLiteral("盐") };
        m_store->addShoppingEntry(entry);
        ResumableJob job;
        QVERIFY(m_store->insertImportJob("{}", job) > 0);

        QVERIFY(m_store->deleteAllData());
        QVERIFY(!m_store->importJob());
        QVERIFY(m_store->imports(true).isEmpty());
        QVERIFY(m_store->items(std::nullopt).isEmpty());
        QVERIFY(m_store->imports().isEmpty());
        QVERIFY(m_store->shelfLifeRules().isEmpty());
        QVERIFY(m_store->shoppingEntries().isEmpty());
        QCOMPARE(m_store->setting(QStringLiteral("demo"), QStringLiteral("0")), QStringLiteral("0"));
        QVERIFY(!QFile::exists(image));
        QVERIFY(!QFile::exists(stray));
        QCOMPARE(m_database->schemaVersion(), Database::latestVersion());
    }

    void rulesAndCache()
    {
        QVERIFY(m_store->saveShelfLifeRule({ QStringLiteral("鸡蛋"), Storage::Fridge, 30 }));
        QVERIFY(m_store->saveShelfLifeRule({ QStringLiteral("鸡蛋"), Storage::Fridge, 25 }));
        QVERIFY(!m_store->saveShelfLifeRule({ QStringLiteral("未归类"), Storage::Fridge, 5 }));
        QVERIFY(m_store->saveStorageRule({ QStringLiteral("香蕉"), Storage::Room }));
        const RuleBook book = m_store->ruleBook();
        QCOMPARE(book.shelfLifeDays(QStringLiteral("鸡蛋"), Storage::Fridge), std::optional<int>(25));
        QCOMPARE(book.storage(QStringLiteral("香蕉")), std::optional<Storage>(Storage::Room));
        QVERIFY(m_store->deleteShelfLifeRule(QStringLiteral("鸡蛋"), Storage::Fridge));
        QVERIFY(m_store->deleteStorageRule(QStringLiteral("香蕉")));
        QVERIFY(m_store->shelfLifeRules().isEmpty());

        QVERIFY(m_store->cacheAiEstimate({ QStringLiteral("牛奶"), Storage::Fridge, 7 }));
        QVERIFY(m_store->cacheAiEstimate({ QStringLiteral("牛奶"), Storage::Fridge, 14 }));
        QCOMPARE(m_store->aiEstimateCache().days(QStringLiteral("牛奶"), Storage::Fridge), std::optional<int>(7));
    }

    void pantryOverrides()
    {
        QVERIFY(m_store->savePantryOverride({ QStringLiteral("盐"), std::nullopt, std::nullopt, true }));
        QVERIFY(m_store->savePantryOverride({ QStringLiteral("豆瓣酱"), true, false, false }));
        const QList<PantryOverride> overrides = m_store->pantryOverrides();
        QCOMPARE(overrides.size(), 2);
        QCOMPARE(overrides.at(0).name, QStringLiteral("盐"));
        QVERIFY(overrides.at(0).out);
        QVERIFY(!overrides.at(0).pantry);
        QCOMPARE(overrides.at(1).bulk, std::optional<bool>(false));
        const PantryRules rules = m_store->pantryRules(nullptr);
        QVERIFY(rules.isPantry(QStringLiteral("豆瓣酱")));
        QVERIFY(m_store->deletePantryOverride(QStringLiteral("盐")));
        QCOMPARE(m_store->pantryOverrides().size(), 1);
    }

    void suggestions()
    {
        Suggestion first{ 0, SuggestionKind::AddToShopping, QStringLiteral("盐"), QStringLiteral("加入待买吗？"), 0, {} };
        Suggestion again = first;
        const qint64 id = m_store->addSuggestion(first);
        QVERIFY(id > 0);
        QCOMPARE(m_store->addSuggestion(again), id);
        QCOMPARE(m_store->pendingSuggestions().size(), 1);
        QVERIFY(m_store->suggestion(id));
        QVERIFY(m_store->resolveSuggestion(id, true));
        QVERIFY(!m_store->resolveSuggestion(id, false));
        QVERIFY(m_store->pendingSuggestions().isEmpty());
        QVERIFY(!m_store->suggestion(id));
        Suggestion later = first;
        QVERIFY(m_store->addSuggestion(later) != id);
    }

    void shoppingList()
    {
        ShoppingEntry a = makeShoppingEntry({ QStringLiteral("牛奶"), QStringLiteral("淡奶油") }, ShoppingOrigin::Recipe,
                                            QStringLiteral("奶油汤"), nullptr);
        ShoppingEntry b = makeShoppingEntry({ QStringLiteral("淡奶油"), QStringLiteral("牛奶") }, ShoppingOrigin::Manual, {}, nullptr);
        QVERIFY(m_store->addShoppingEntry(a) > 0);
        QCOMPARE(m_store->addShoppingEntry(b), qint64(0));
        const QList<ShoppingEntry> entries = m_store->shoppingEntries();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().names, (QStringList{ QStringLiteral("牛奶"), QStringLiteral("淡奶油") }));
        QCOMPARE(entries.first().origin, ShoppingOrigin::Recipe);
        QCOMPARE(entries.first().note, QStringLiteral("奶油汤"));
        QVERIFY(m_store->removeShoppingEntry(a.id));
        QVERIFY(m_store->shoppingEntries().isEmpty());
    }

    void fingerprints()
    {
        ImportRecord record;
        record.inputKind = QStringLiteral("screenshot");
        record.images = { { QStringLiteral("/nowhere/a.jpg"), QStringLiteral("sha-a"), {}, {}, {} } };
        m_store->insertImport(record);
        FridgeItem item = sampleItem(record.id, QStringLiteral("土豆"), QStringLiteral("500g"));
        m_store->insertItem(item);
        const QList<ImportFingerprint> list = m_store->importFingerprints();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().imageSha256, QStringList{ QStringLiteral("sha-a") });
        QCOMPARE(list.first().items.size(), 1);
        QCOMPARE(list.first().createdAt, m_clock->now());
    }
};

QTEST_GUILESS_MAIN(StoreTest)
#include "store-test.moc"
