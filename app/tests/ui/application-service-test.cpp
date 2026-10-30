#include "ui/import-service.h"
#include "ui/kitchen-service.h"
#include "ai/recorded-chat-client.h"
#include "core/fridge-groups.h"
#include "data/database.h"
#include "recognition/deepseek-recognizer.h"
#include "recognition/demo-recognizer.h"
#include "recognition/replay-recognizer.h"
#include "recognition/recording.h"

#include <QDir>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh;
using namespace whilefresh::core;
using namespace whilefresh::ui;
namespace rec = whilefresh::recognition;
namespace ai = whilefresh::ai;

class ApplicationServiceTest : public QObject
{
    Q_OBJECT
    std::shared_ptr<const data::BuiltinData> builtin;
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<data::Database> database;
    std::unique_ptr<FixedClock> clock;
    std::unique_ptr<data::Store> store;
    std::unique_ptr<KitchenService> kitchen;

    platform::ImportPayload payload(bool withDate = true, const QByteArray &bytes = "synthetic image")
    {
        platform::ImportPayload payload;
        platform::ImportedImage image;
        image.localPath = QDir(store->importsDirectory()).filePath(QUuid::createUuid().toString() + QStringLiteral(".jpg"));
        QFile file(image.localPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
            qFatal("cannot write synthetic image");
        image.mimeType = QStringLiteral("image/jpeg");
        if (withDate) {
            image.capturedAt = QDateTime(QDate(2026, 10, 21), QTime(9, 0));
            image.capturedAtSource = platform::ImportedImage::DateSource::Exif;
        }
        payload.images.append(image);
        return payload;
    }

    QString recording(const QString &items, bool truncated = false)
    {
        const QString content = QStringLiteral("{\"source_type\":\"cart\",\"items\":[%1]}").arg(items);
        QJsonObject response{
            { "choices", QJsonArray{ QJsonObject{
                { "message", QJsonObject{ { "role", "assistant" }, { "content", content } } },
                { "finish_reason", truncated ? "length" : "stop" } } } }
        };
        rec::Recording recording;
        recording.inputKind = rec::InputKind::Screenshot;
        recording.imageCount = 1;
        recording.responses.append(QJsonDocument(response).toJson());
        recording.batches.append({ 0, 0 });
        const QString path = dir->filePath(QUuid::createUuid().toString() + QStringLiteral(".json"));
        if (!rec::writeRecording(path, recording))
            qFatal("cannot write synthetic recording");
        return path;
    }

    QString mixedRecording(bool truncated = false)
    {
        return recording(QString::fromUtf8(R"(
          {"raw_name":"鲜鸡蛋10枚","canonical_name":"鸡蛋","quantity_text":"10枚","count":2,
           "category":"food","selected":true,"production_date_text":"昨天生产","image_index":1,
           "storage":"fridge","shelf_life_days":{"fridge":30,"freezer":60,"room":10}},
          {"raw_name":"生抽","canonical_name":"生抽","category":"seasoning","selected":true},
          {"raw_name":"洗洁精","canonical_name":"未归类","category":"irrelevant","selected":true}
        )"), truncated);
    }

    FridgeItem manual(const QString &name)
    {
        FridgeItem item;
        item.canonicalName = name;
        item.displayName = name;
        item.quantityText = QStringLiteral("1份");
        return item;
    }

private slots:
    void initTestCase()
    {
        builtin = data::BuiltinData::builtIn();
        QVERIFY(builtin);
        qRegisterMetaType<rec::RecognitionError>();
    }

    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        database = std::make_unique<data::Database>(dir->filePath(QStringLiteral("test.db")));
        QVERIFY(database->open());
        QVERIFY(database->migrate());
        clock = std::make_unique<FixedClock>(QDateTime(QDate(2026, 10, 21), QTime(12, 0)));
        const QString imports = dir->filePath(QStringLiteral("imports"));
        QVERIFY(QDir().mkpath(imports));
        store = std::make_unique<data::Store>(*database, *clock, imports);
        kitchen = std::make_unique<KitchenService>(*store, builtin, *clock);
    }

    void cleanup()
    {
        kitchen.reset();
        store.reset();
        database.reset();
        dir.reset();
    }

    void replayDraftConfirmAndDelete()
    {
        rec::ReplayRecognizer recognizer(QStringLiteral(WHILEFRESH_TEST_DATA_DIR "/recordings/synthetic-cart.json"));
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QSignalSpy committed(&service, &ImportService::committed);
        const auto input = payload();
        QVERIFY(service.receive(input));
        QVERIFY(!service.start(false));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(store->items().isEmpty());
        QVERIFY(store->imports().isEmpty());
        QCOMPARE(service.draft().ignoredCount(), 1);
        QCOMPARE(service.draft().items.first().item.productionDate.date, QDate(2026, 10, 21));
        QVERIFY(service.confirm());
        QCOMPARE(committed.size(), 1);
        QCOMPARE(store->items().size(), 6);
        const auto record = store->imports().first();
        QVERIFY(!record.isDemo);
        QCOMPARE(record.sourceType, QStringLiteral("cart"));
        QCOMPARE(record.images.size(), 1);
        QCOMPARE(record.images.first().sha256.size(), 64);
        QVERIFY(kitchen->deleteImport(record.id));
        QVERIFY(store->items().isEmpty());
        QVERIFY(!QFile::exists(input.images.first().localPath));
    }

    void noPhotoDateNeedsConfirmation()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload(false)));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        const auto &draft = service.draft();
        QVERIFY(draft.items.first().needsDateConfirmation());
        QVERIFY(!draft.items.first().item.productionDate.date.isValid());
        QVERIFY(!service.confirm());
        QVERIFY(store->imports().isEmpty());
        QVERIFY(service.setCaptureDate(0, QDate(2026, 10, 19)));
        QCOMPARE(service.draft().items.first().item.productionDate.date, QDate(2026, 10, 18));
        QVERIFY(service.confirm());
        QCOMPARE(store->items().first().entryDate, QDate(2026, 10, 21));
        QCOMPARE(store->items().first().productionDate.date, QDate(2026, 10, 18));
        // The visible production date is the start date, the conservative side of a purchase-based reference.
        QVERIFY(store->items().first().expiryDate > QDate(2026, 10, 18));
        QCOMPARE(store->imports().first().images.first().capturedAtSource, QStringLiteral("user"));
    }

    void seasoningsSuggestButNeverSilentlyChangePantry()
    {
        QVERIFY(kitchen->markPantryAbsent(QStringLiteral("生抽")));
        const auto buy = kitchen->suggestions().first();
        QVERIFY(kitchen->decideSuggestion(buy.id, false));
        rec::ReplayRecognizer recognizer(mixedRecording(true));
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload()));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(service.draft().record.truncated);
        QCOMPARE(service.draft().warning, QStringLiteral("可能有遗漏，请检查"));
        QVERIFY(service.confirm());
        QCOMPARE(store->items().size(), 2); // the seasoning is stock too (常备调料)
        QVERIFY(!kitchen->pantryRules().isAtHome(QStringLiteral("生抽")));
        QCOMPARE(kitchen->suggestions().size(), 1);
        const auto suggestion = kitchen->suggestions().first();
        QCOMPARE(suggestion.kind, SuggestionKind::MarkPantryHave);
        QVERIFY(kitchen->decideSuggestion(suggestion.id, true));
        QVERIFY(kitchen->pantryRules().isAtHome(QStringLiteral("生抽")));
        QVERIFY(store->imports().first().truncated);
        QCOMPARE(store->aiEstimateCache().days(QStringLiteral("鸡蛋"), Storage::Fridge), std::optional<int>(30));
    }

    void duplicateImagesStopBeforeUploadAndItemsAfterRecognition()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload()));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(service.confirm());
        const auto firstImport = store->imports().first().id;
        QVERIFY(service.receive(payload()));
        QCOMPARE(service.draft().duplicate.kind, DuplicateCheck::Kind::SameImage);
        QVERIFY(!service.start(true));
        QVERIFY(service.discard());
        // A different image of the same purchases still triggers the item fingerprint.
        QVERIFY(service.receive(payload(true, "different bytes")));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 2);
        QCOMPARE(service.draft().duplicate.kind, DuplicateCheck::Kind::SimilarItems);
        QCOMPARE(service.draft().duplicate.importId, firstImport);
        QVERIFY(!service.confirm());
        QVERIFY(service.confirm(true));
        QCOMPARE(store->imports().size(), 2);
    }

    void rejectedSecondPayloadIsCleanedWithoutLosingDraft()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        const auto first = payload();
        const auto second = payload(true, "another");
        QVERIFY(service.receive(first));
        QVERIFY(!service.receive(second));
        QVERIFY(QFile::exists(first.images.first().localPath));
        QVERIFY(!QFile::exists(second.images.first().localPath));
        QVERIFY(service.discard());
    }

    void failedCommitRollsBackAndCanBeRetried()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        const auto input = payload();
        QVERIFY(service.receive(input));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QSqlQuery query(database->handle());
        QVERIFY(query.exec(QStringLiteral("CREATE TRIGGER fail_item BEFORE INSERT ON items "
                                          "BEGIN SELECT RAISE(ABORT, 'synthetic failure'); END")));
        QVERIFY(!service.confirm());
        QVERIFY(store->imports().isEmpty());
        QVERIFY(store->items().isEmpty());
        QVERIFY(!store->aiEstimateCache().days(QStringLiteral("鸡蛋"), Storage::Fridge));
        QVERIFY(QFile::exists(input.images.first().localPath));
        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER fail_item")));
        QVERIFY(service.confirm());
        QCOMPARE(store->imports().size(), 1);
    }

    void identicalNamesUseFirstAiEstimateAcrossImports()
    {
        // 土豆 has no fridge reference; two rows return conflicting AI estimates.
        const auto path = recording(QString::fromUtf8(R"(
          {"raw_name":"土豆500g","canonical_name":"土豆","category":"food","storage":"fridge",
           "shelf_life_days":{"fridge":10,"freezer":40}},
          {"raw_name":"土豆1kg","canonical_name":"土豆","category":"food","storage":"fridge",
           "shelf_life_days":{"fridge":20,"freezer":80}}
        )"));
        rec::ReplayRecognizer recognizer(path);
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload()));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        // Storage suggestion for potato is room; changing it to fridge is local.
        for (int i = 0; i < service.draft().items.size(); ++i) {
            auto item = service.draft().items.at(i);
            item.item.storage = Storage::Fridge;
            QVERIFY(service.updateItem(i, item));
        }
        QCOMPARE(service.draft().items.at(0).item.shelfLifeDays, std::optional<int>(10));
        QCOMPARE(service.draft().items.at(1).item.shelfLifeDays, std::optional<int>(10));
        QVERIFY(service.confirm());
        for (const auto &item : kitchen->fridge()) {
            QCOMPARE(item.shelfLifeDays, std::optional<int>(10));
            QVERIFY(kitchen->changeStorage(item.id, Storage::Freezer));
            QCOMPARE(store->item(item.id)->shelfLifeDays, std::optional<int>(40));
        }
    }

    void discardDuringRecognitionDeletesOwnedFiles()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        const auto input = payload();
        QVERIFY(service.receive(input));
        QVERIFY(service.start(true));
        QVERIFY(service.discard());
        QCoreApplication::processEvents();
        QCOMPARE(ready.size(), 0);
        QVERIFY(!QFile::exists(input.images.first().localPath));
        QVERIFY(store->items().isEmpty());
    }

    void deletingAllCancelsActiveRecognition()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QObject::connect(kitchen.get(), &KitchenService::aboutToDeleteAllData,
                         &service, &ImportService::discard);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload()));
        QVERIFY(service.start(true));
        QVERIFY(kitchen->deleteAllData());
        QCoreApplication::processEvents();
        QCOMPARE(ready.size(), 0);
        QVERIFY(!service.draft().ready);
        QVERIFY(store->imports().isEmpty());
    }

    void failuresPassThroughAndRemainDiscardable()
    {
        rec::ReplayRecognizer recognizer(dir->filePath(QStringLiteral("missing.json")));
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy failed(&service, &ImportService::failed);
        const auto input = payload();
        QVERIFY(service.receive(input));
        QVERIFY(service.start(true));
        QTRY_COMPARE(failed.size(), 1);
        const auto error = qvariant_cast<rec::RecognitionError>(failed.first().first());
        QCOMPARE(error.kind, rec::ErrorKind::BadResponse);
        QCOMPARE(error.userMessage, rec::userMessageFor(rec::ErrorKind::BadResponse));
        QVERIFY(QFile::exists(input.images.first().localPath));
        QVERIFY(service.discard());
        QVERIFY(!QFile::exists(input.images.first().localPath));
    }

    void demoUsesTheSameConfirmationPath()
    {
        rec::DemoRecognizer recognizer;
        ImportService service(*store, builtin, *clock, recognizer, true);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload()));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(service.draft().record.isDemo);
        QVERIFY(service.confirm());
        QVERIFY(store->imports().first().isDemo);
        QCOMPARE(store->items().size(), 3);
    }

    void textImportAndManualCorrection()
    {
        rec::ReplayRecognizer recognizer(recording(QString::fromUtf8(R"(
          {"raw_name":"鸡蛋","canonical_name":"鸡蛋","category":"food","production_date_text":"今天生产"}
        )")));
        ImportService service(*store, builtin, *clock, recognizer);
        platform::ImportPayload input;
        input.kind = platform::ImportPayload::Kind::Text;
        input.text = QStringLiteral("鸡蛋 今天生产");
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(input));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(!service.confirm());
        auto draft = service.draft().items.first();
        draft.item.productionDate.date = QDate(2026, 10, 18);
        draft.item.purchaseDate.date = QDate(2026, 10, 20);
        QVERIFY(service.updateItem(0, draft));
        QVERIFY(service.confirm());
        const auto item = store->items().first();
        QCOMPARE(item.productionDate.origin, DateOrigin::User);
        QCOMPARE(item.purchaseDate.origin, DateOrigin::User);
        QCOMPARE(item.expiryDate, QDate(2026, 11, 8)); // production date wins over purchase date
        QVERIFY(store->imports().first().images.isEmpty());
        QCOMPARE(store->imports().first().textLength, input.text.size());
    }

    void deletingAnImportKeepsWhatWasEaten()
    {
        rec::ReplayRecognizer recognizer(QStringLiteral(WHILEFRESH_TEST_DATA_DIR "/recordings/synthetic-cart.json"));
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload()));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(service.confirm());
        auto manualEgg = manual(QStringLiteral("鸡蛋"));
        QVERIFY(kitchen->saveItem(manualEgg));
        const qint64 importId = store->imports().first().id;
        const auto imported = store->itemsForImport(importId);
        QVERIFY(imported.size() >= 3);
        auto used = imported.at(0);
        used.status = ItemStatus::Used;
        QVERIFY(store->updateItem(used));
        QVERIFY(kitchen->discardItem(imported.at(1).id));

        QVERIFY(kitchen->deleteImport(importId));
        QVERIFY(kitchen->imports().isEmpty());
        QCOMPARE(kitchen->fridge().size(), 1); // only the manual egg remains in the fridge
        QCOMPARE(kitchen->fridge().first().id, manualEgg.id);
        const auto kept = store->itemsForImport(importId);
        QCOMPARE(kept.size(), 2);
        for (const auto &item : kept)
            QVERIFY(item.status != ItemStatus::InStock);
    }

    void clearingHistoryKeepsTheFridge()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        const auto input = payload();
        QVERIFY(service.receive(input));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(service.confirm());
        const auto before = kitchen->fridge().size();
        QVERIFY(before > 0);
        QVERIFY(kitchen->clearImportHistory());
        QVERIFY(kitchen->imports().isEmpty());
        QCOMPARE(kitchen->fridge().size(), before);
        QVERIFY(!QFile::exists(input.images.first().localPath));
    }

    void skippedDuplicatesAreRecorded()
    {
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QSignalSpy skipped(&service, &ImportService::skipped);
        const auto first = payload();
        QVERIFY(service.receive(first));
        QVERIFY(!service.skipDuplicate()); // nothing to skip yet
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(service.confirm());
        const qint64 original = store->imports().first().id;
        const auto itemCount = store->items().size();

        // Same image: caught before upload, skipped without recognition.
        const auto again = payload();
        QVERIFY(service.receive(again));
        QCOMPARE(service.draft().duplicate.kind, DuplicateCheck::Kind::SameImage);
        QVERIFY(service.skipDuplicate());
        QCOMPARE(skipped.size(), 1);
        QVERIFY(!QFile::exists(again.images.first().localPath));

        // Similar items in another screenshot: caught after recognition, skipped from the draft.
        QVERIFY(service.receive(payload(true, "different bytes")));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 2);
        QCOMPARE(service.draft().duplicate.kind, DuplicateCheck::Kind::SimilarItems);
        QVERIFY(service.skipDuplicate());
        QCOMPARE(skipped.size(), 2);
        QVERIFY(!service.draft().ready);
        QVERIFY(!service.hasUnfinishedJob());

        const auto records = store->imports();
        QCOMPARE(records.size(), 3);
        for (int i = 0; i < 2; ++i) {
            QCOMPARE(records.at(i).status, data::ImportStatus::SkippedDuplicate);
            QCOMPARE(records.at(i).duplicateOf, original);
            QVERIFY(records.at(i).images.isEmpty());
            QVERIFY(store->itemsForImport(records.at(i).id).isEmpty());
        }
        QCOMPARE(records.at(2).status, data::ImportStatus::Confirmed);
        QCOMPARE(store->items().size(), itemCount);
    }

    void seasoningsAndStaplesBecomeStock()
    {
        rec::ReplayRecognizer recognizer(recording(QString::fromUtf8(R"(
          {"raw_name":"生抽500ml×2","canonical_name":"生抽","quantity_text":"500ml×2","category":"seasoning"},
          {"raw_name":"方便面5连包","canonical_name":"方便面","quantity_text":"5包","category":"food","storage":"room"},
          {"raw_name":"大米5kg","canonical_name":"大米","quantity_text":"5kg","category":"food","storage":"room"},
          {"raw_name":"鲜鸡蛋","canonical_name":"鸡蛋","quantity_text":"10枚","count":2,"category":"food",
           "storage":"fridge","shelf_life_days":{"fridge":30}},
          {"raw_name":"某牌秘制酱","canonical_name":"未归类","quantity_text":"约350克","category":"seasoning"}
        )")));
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.receive(payload()));
        QVERIFY(service.start(true));
        QTRY_COMPARE(ready.size(), 1);
        const auto &draft = service.draft();
        QCOMPARE(draft.items.at(0).item.kind, ItemKind::StapleSeasoning);
        QCOMPARE(draft.items.at(1).item.kind, ItemKind::StapleFood);
        QVERIFY(draft.items.at(1).needsShelfLife()); // no reference, no AI value: 待录保质期
        QCOMPARE(draft.items.at(2).item.kind, ItemKind::StapleFood);
        QCOMPARE(draft.items.at(3).item.kind, ItemKind::Food);
        QVERIFY(!draft.items.at(3).needsShelfLife());
        QVERIFY(service.confirm());

        QHash<QString, FridgeItem> byName;
        for (const auto &item : store->items())
            byName.insert(item.rawName, item);
        QCOMPARE(byName.size(), 5);
        QCOMPARE(byName.value(QStringLiteral("生抽500ml×2")).amount, 1000.0);
        QCOMPARE(byName.value(QStringLiteral("生抽500ml×2")).unit, QStringLiteral("ml"));
        QCOMPARE(byName.value(QStringLiteral("方便面5连包")).amount, 5.0);
        QCOMPARE(byName.value(QStringLiteral("方便面5连包")).unit, QStringLiteral("包"));
        QVERIFY(byName.value(QStringLiteral("方便面5连包")).needsShelfLife());
        QCOMPARE(byName.value(QStringLiteral("大米5kg")).amount, 5000.0);
        QCOMPARE(byName.value(QStringLiteral("鲜鸡蛋")).amount, 20.0);
        QCOMPARE(byName.value(QStringLiteral("鲜鸡蛋")).unit, QStringLiteral("枚"));
        const auto sauce = byName.value(QStringLiteral("某牌秘制酱"));
        QCOMPARE(sauce.kind, ItemKind::StapleSeasoning);
        QCOMPARE(sauce.amount, 350.0);
        // The pantry flow still runs for named seasonings, and never for unknown ones.
        QStringList suggested;
        for (const auto &suggestion : kitchen->suggestions())
            suggested.append(suggestion.name);
        QVERIFY(!suggested.contains(QStringLiteral("未归类")));

        const auto fridge = kitchen->fridge();
        QStringList groups;
        for (const auto &item : fridge)
            groups.append(fridgeGroupName(fridgeGroup(item)));
        QCOMPARE(groups.first(), QStringLiteral("冷藏"));
        QCOMPARE(groups.count(QStringLiteral("常备调料")), 2);
        QCOMPARE(groups.count(QStringLiteral("常备食材")), 2);
    }

    void discardAndSnooze()
    {
        auto milk = manual(QStringLiteral("牛奶"));
        milk.manualDays = 8;
        QVERIFY(kitchen->saveItem(milk));
        auto pork = manual(QStringLiteral("五花肉"));
        pork.manualDays = 2;
        QVERIFY(kitchen->saveItem(pork));
        clock->advanceDays(6); // milk near expiry, pork beyond the estimate
        QCOMPARE(kitchen->attention().size(), 2);
        QVERIFY(!kitchen->snoozeItem(pork.id)); // expired: only a new shelf life or 扔掉
        QVERIFY(kitchen->snoozeItem(milk.id));
        QCOMPARE(kitchen->attention().size(), 1);
        QCOMPARE(kitchen->attention().first().id, pork.id);
        QCOMPARE(kitchen->fridge().size(), 2); // snoozed items stay in the fridge
        clock->advanceDays(1);
        QCOMPARE(kitchen->attention().size(), 2); // 忽略 lasts for today only

        QVERIFY(kitchen->discardItem(pork.id));
        QCOMPARE(store->item(pork.id)->status, ItemStatus::Discarded);
        QCOMPARE(kitchen->fridge().size(), 1);
        QVERIFY(!kitchen->discardItem(pork.id));
    }

    void manualItemsGetAnAmount()
    {
        auto item = manual(QStringLiteral("土豆"));
        item.quantityText = QStringLiteral("1.5kg");
        QVERIFY(kitchen->saveItem(item));
        QCOMPARE(store->item(item.id)->amount, 1500.0);
        item.quantityText = QStringLiteral("3个");
        QVERIFY(kitchen->saveItem(item));
        QCOMPARE(store->item(item.id)->amount, 3.0);
        QCOMPARE(store->item(item.id)->unit, QStringLiteral("个"));
    }

    void resumeSendsOnlyUnfinishedBatches()
    {
        const auto body = [](const QString &raw, const QString &canonical) {
            const QString content = QStringLiteral("{\"source_type\":\"cart\",\"items\":[{\"raw_name\":\"%1\",\"canonical_name\":\"%2\","
                                                   "\"category\":\"food\",\"storage\":\"fridge\"}]}").arg(raw, canonical);
            const QJsonObject message{ { "role", "assistant" }, { "content", content } };
            return QJsonDocument(QJsonObject{ { "choices", QJsonArray{ QJsonObject{ { "message", message }, { "finish_reason", "stop" } } } } })
                .toJson(QJsonDocument::Compact);
        };
        const auto answer = [](const QByteArray &response) {
            ai::ChatExchange exchange;
            exchange.response = response;
            return exchange;
        };
        ai::ChatExchange serverError;
        serverError.error = ai::ChatError{ rec::ErrorKind::ServerError, QStringLiteral("scripted 503"), 503 };

        // Three images, one per request.
        platform::ImportPayload input;
        for (int i = 0; i < 3; ++i) {
            QImage image(400, 600, QImage::Format_RGB32);
            image.fill(QColor::fromHsv(i * 100, 200, 200));
            platform::ImportedImage file;
            file.localPath = QDir(store->importsDirectory()).filePath(QStringLiteral("cart-%1.png").arg(i));
            QVERIFY(image.save(file.localPath));
            file.mimeType = QStringLiteral("image/png");
            file.capturedAt = QDateTime(QDate(2026, 10, 21), QTime(9, 0));
            file.capturedAtSource = platform::ImportedImage::DateSource::Exif;
            input.images.append(file);
        }
        rec::BatchLimits onePerRequest;
        onePerRequest.maxImages = 1;

        QByteArray secondBatchBody;
        {
            ai::RecordedChatClient client(ai::ChatRecording{ { answer(body(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋"))), serverError } });
            rec::DeepSeekRecognizer recognizer(&client);
            recognizer.setBatchLimits(onePerRequest);
            ImportService service(*store, builtin, *clock, recognizer);
            QSignalSpy failed(&service, &ImportService::failed);
            QVERIFY(service.receive(input));
            QVERIFY(service.start(true));
            QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 20000);
            QCOMPARE(client.requests().size(), 2);
            secondBatchBody = client.requests().at(1).body;
        } // the process "dies": a new service over the same database follows

        for (const auto &image : input.images)
            QVERIFY(QFile::exists(image.localPath));
        ai::RecordedChatClient client(ai::ChatRecording{ { answer(body(QStringLiteral("纯牛奶"), QStringLiteral("牛奶"))),
                                                           answer(body(QStringLiteral("土豆"), QStringLiteral("土豆"))) } });
        rec::DeepSeekRecognizer recognizer(&client);
        recognizer.setBatchLimits(onePerRequest);
        ImportService service(*store, builtin, *clock, recognizer);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QVERIFY(service.hasUnfinishedJob());
        QVERIFY(service.resume());
        QVERIFY(!service.hasUnfinishedJob());
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 20000);
        QCOMPARE(client.requests().size(), 2); // batches 2 and 3 only
        QCOMPARE(client.requests().at(0).body, secondBatchBody);

        QStringList names;
        for (const auto &draft : service.draft().items)
            names.append(draft.item.canonicalName);
        QCOMPARE(names, (QStringList{ "鸡蛋", "牛奶", "土豆" }));
        QCOMPARE(service.draft().record.images.size(), 3);
        QVERIFY(service.confirm());
        QVERIFY(!store->importJob());
        QCOMPARE(store->imports().first().images.size(), 3);
        QCOMPARE(store->items().size(), 3);
    }

    void finishedJobResumesWithoutRecognition()
    {
        const auto input = payload();
        {
            rec::ReplayRecognizer recognizer(mixedRecording());
            ImportService service(*store, builtin, *clock, recognizer);
            QSignalSpy ready(&service, &ImportService::draftReady);
            QVERIFY(service.receive(input));
            QVERIFY(service.start(true));
            QTRY_COMPARE(ready.size(), 1);
        } // closed while the draft waited for review
        QVERIFY(QFile::exists(input.images.first().localPath));

        rec::ReplayRecognizer unusable(dir->filePath(QStringLiteral("missing.json")));
        ImportService service(*store, builtin, *clock, unusable);
        QSignalSpy ready(&service, &ImportService::draftReady);
        QSignalSpy failed(&service, &ImportService::failed);
        QVERIFY(service.resume());
        QCOMPARE(ready.size(), 1);
        QCOMPARE(service.draft().items.size(), 3);
        QCOMPARE(service.draft().items.first().item.productionDate.date, QDate(2026, 10, 20));
        QVERIFY(service.confirm());
        QCoreApplication::processEvents();
        QCOMPARE(failed.size(), 0);
        QVERIFY(!store->importJob());
    }

    void unfinishedJobsCanBeDiscardedOrReplaced()
    {
        const auto first = payload();
        {
            rec::ReplayRecognizer recognizer(dir->filePath(QStringLiteral("missing.json")));
            ImportService service(*store, builtin, *clock, recognizer);
            QSignalSpy failed(&service, &ImportService::failed);
            QVERIFY(service.receive(first));
            QVERIFY(service.start(true));
            QTRY_COMPARE(failed.size(), 1);
        }
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);
        QVERIFY(service.hasUnfinishedJob());
        QVERIFY(service.discardUnfinishedJob());
        QVERIFY(!service.hasUnfinishedJob());
        QVERIFY(!QFile::exists(first.images.first().localPath));

        // A job nobody resumed is replaced by the next import.
        const auto second = payload(true, "second");
        {
            rec::ReplayRecognizer missing(dir->filePath(QStringLiteral("missing.json")));
            ImportService stale(*store, builtin, *clock, missing);
            QSignalSpy failed(&stale, &ImportService::failed);
            QVERIFY(stale.receive(second));
            QVERIFY(stale.start(true));
            QTRY_COMPARE(failed.size(), 1);
        }
        QVERIFY(service.receive(payload(true, "third")));
        QVERIFY(!QFile::exists(second.images.first().localPath));
        QVERIFY(service.discard());
        QVERIFY(!store->importJob());
    }

    void unrestorableJobsStillFreeTheirImages()
    {
        const auto orphan = [this] {
            const auto image = payload().images.first().localPath;
            const QByteArray json = QJsonDocument(QJsonObject{ { "images", QJsonArray{ QJsonObject{ { "path", image } } } } })
                                        .toJson(QJsonDocument::Compact);
            ResumableJob job;
            const qint64 id = store->insertImportJob(json, job);
            QSqlQuery query(database->handle());
            // "done" without any batch: ResumableJob::restore() rejects it.
            query.prepare(QStringLiteral("UPDATE import_jobs SET state = 'done' WHERE id = ?"));
            query.addBindValue(id);
            if (!query.exec())
                qFatal("cannot corrupt job");
            return image;
        };
        rec::ReplayRecognizer recognizer(mixedRecording());
        ImportService service(*store, builtin, *clock, recognizer);

        const QString first = orphan();
        QVERIFY(!store->importJob());
        QVERIFY(!service.hasUnfinishedJob());
        QVERIFY(service.discardUnfinishedJob());
        QVERIFY(store->importJobRows().isEmpty());
        QVERIFY(!QFile::exists(first));

        const QString second = orphan();
        QVERIFY(service.receive(payload(true, "next")));
        QVERIFY(store->importJobRows().isEmpty());
        QVERIFY(!QFile::exists(second));
        QVERIFY(service.discard());
    }

    void textImportsAreNotPersisted()
    {
        rec::ReplayRecognizer recognizer(dir->filePath(QStringLiteral("missing.json")));
        {
            ImportService service(*store, builtin, *clock, recognizer);
            QSignalSpy failed(&service, &ImportService::failed);
            platform::ImportPayload input;
            input.kind = platform::ImportPayload::Kind::Text;
            input.text = QStringLiteral("鸡蛋 两盒");
            QVERIFY(service.receive(input));
            QVERIFY(service.start(true));
            QTRY_COMPARE(failed.size(), 1);
            QVERIFY(!store->importJob());
        }
        QSqlQuery query(database->handle());
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM import_jobs")) && query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }

    void bulkUseAbsenceAndShoppingSuggestion()
    {
        auto egg = manual(QStringLiteral("鸡蛋"));
        auto pork = manual(QStringLiteral("五花肉"));
        QVERIFY(kitchen->saveItem(egg));
        QVERIFY(kitchen->saveItem(pork));
        QVERIFY(kitchen->useItem(egg.id));
        QCOMPARE(store->item(egg.id)->status, ItemStatus::InStock);
        QVERIFY(kitchen->useItem(pork.id));
        QCOMPARE(store->item(pork.id)->status, ItemStatus::Used);
        QVERIFY(kitchen->markAbsent(egg.id));
        QVERIFY(kitchen->fridge().isEmpty());
        QVERIFY(kitchen->shopping().isEmpty());
        QCOMPARE(kitchen->suggestions().size(), 1);
        QVERIFY(kitchen->decideSuggestion(kitchen->suggestions().first().id, true));
        QCOMPARE(kitchen->shopping().first().names, QStringList{ QStringLiteral("鸡蛋") });
        QVERIFY(kitchen->shoppingText().contains(QStringLiteral("【蛋奶】")));
    }

    void editedRulesNeedSeparateConsent()
    {
        auto item = manual(QStringLiteral("鸡蛋"));
        QVERIFY(kitchen->saveItem(item));
        auto before = item;
        item.manualDays = 8;
        QVERIFY(kitchen->saveItem(item));
        const auto proposals = kitchen->proposalsForEdit(before, item);
        QCOMPARE(proposals.size(), 1);
        QVERIFY(kitchen->shelfLifeRules().isEmpty());
        QVERIFY(kitchen->confirmRule(proposals.first()));
        QCOMPARE(kitchen->shelfLifeRules().first().days, 8);
        auto next = manual(QStringLiteral("鸡蛋"));
        QVERIFY(kitchen->saveItem(next));
        QCOMPARE(next.shelfLifeSource, ShelfLifeSource::LocalRule);
        QCOMPARE(next.shelfLifeDays, std::optional<int>(8));
        clock->advanceDays(6);
        QCOMPARE(kitchen->freshness(next).freshness, Freshness::NearExpiry);
        QVERIFY(kitchen->defaultSelection().contains(next.id));
        clock->advanceDays(2);
        QCOMPARE(kitchen->freshness(next).freshness, Freshness::BeyondEstimate);
        QVERIFY(kitchen->deleteShelfLifeRule(QStringLiteral("鸡蛋"), Storage::Fridge));
        QVERIFY(kitchen->shelfLifeRules().isEmpty());
    }

    void storageChangeUsesStoredAiAndDoesNotSetRule()
    {
        auto item = manual(QStringLiteral("土豆"));
        item.aiDays.fridge = 14;
        item.aiDays.freezer = 40;
        QVERIFY(kitchen->saveItem(item));
        QCOMPARE(item.shelfLifeDays, std::optional<int>(14));
        QVERIFY(kitchen->changeStorage(item.id, Storage::Freezer));
        QCOMPARE(store->item(item.id)->shelfLifeDays, std::optional<int>(40));
        QVERIFY(kitchen->storageRules().isEmpty());
    }

    void recipesShoppingSettingsAndDeleteAll()
    {
        for (const auto &name : { QStringLiteral("鸡蛋"), QStringLiteral("西红柿"), QStringLiteral("五花肉"), QStringLiteral("土豆") }) {
            auto item = manual(name);
            QVERIFY(kitchen->saveItem(item));
        }
        const auto matches = kitchen->recipes(QStringLiteral("西红柿炒鸡蛋"));
        QCOMPARE(matches.size(), 1);
        QCOMPARE(matches.first().missingCount, 0);
        QVERIFY(!kitchen->recipeSource().license.isEmpty());
        const auto egg = kitchen->fridge().first();
        QVERIFY(kitchen->setUseFirst(egg.id, true));
        QVERIFY(!kitchen->clearFridge(kitchen->defaultSelection()).isEmpty());
        QVERIFY(kitchen->clearFridge({}).isEmpty());
        QVERIFY(kitchen->setDemoMode(true));
        QVERIFY(kitchen->demoMode());
        const auto id = kitchen->addShopping({ QStringLiteral("牛奶"), QStringLiteral("淡奶油") });
        QVERIFY(id > 0);
        QCOMPARE(kitchen->addShopping({ QStringLiteral("淡奶油"), QStringLiteral("牛奶") }), qint64(0));
        QVERIFY(kitchen->removeShopping(id));
        QVERIFY(kitchen->deleteAllData());
        QVERIFY(!kitchen->demoMode());
        QVERIFY(kitchen->fridge().isEmpty());
        QVERIFY(kitchen->shopping().isEmpty());
    }
};

QTEST_GUILESS_MAIN(ApplicationServiceTest)
#include "application-service-test.moc"
