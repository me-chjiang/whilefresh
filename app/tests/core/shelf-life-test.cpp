#include "core/fridge-item.h"
#include "core/shelf-life.h"

#include "core-test-support.h"

#include <QTest>

using namespace whilefresh::core;
using namespace whilefresh::core::testing;

class ShelfLifeTest : public QObject
{
    Q_OBJECT

    IngredientCatalog catalog = smallCatalog();
    ShelfLifeTable table = smallShelfLifeTable();
    RuleBook rules;
    AiEstimateCache cache;

    ShelfLifeContext context() const { return { &catalog, &table, &rules, &cache }; }

    static ShelfLifeInput input(const QString &name, Storage storage = Storage::Fridge)
    {
        ShelfLifeInput in;
        in.name = name;
        in.storage = storage;
        in.entryDate = QDate(2026, 10, 21);
        return in;
    }

private slots:
    void init()
    {
        rules = RuleBook();
        cache = AiEstimateCache();
    }

    void tableLoadsAllLayouts()
    {
        QCOMPARE(table.size(), 5);
        QCOMPARE(table.sourceName(), QStringLiteral("USDA FoodKeeper"));
        QCOMPARE(table.row(QStringLiteral("猪肉"), &catalog)->foodkeeperId, QStringLiteral("74"));
        QCOMPARE(table.row(QStringLiteral("猪肉"), &catalog)->basis.value(QStringLiteral("fridge")),
                 QStringLiteral("DOP_Refrigerate_Min"));
        auto bare = ShelfLifeTable::fromJson(R"({"version":1,"盐":{"room_days":1000,"basis":"manual"}})");
        QVERIFY(bare);
        QCOMPARE(bare->size(), 1);
        auto array = ShelfLifeTable::fromJson(R"({"items":[{"name":"盐","room_days":1000}]})");
        QVERIFY(array);
        QCOMPARE(array->lookup(QStringLiteral("盐"), Storage::Room, nullptr)->days, 1000);
        QVERIFY(!ShelfLifeTable::fromJson("nope"));
    }

    void packageExpiryWins()
    {
        ShelfLifeInput in = input(QStringLiteral("猪肉"));
        in.packageExpiry = QDate(2026, 10, 30);
        in.aiDays.fridge = 5;
        rules.setShelfLife({ QStringLiteral("猪肉"), Storage::Fridge, 2 });
        const auto result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::Package);
        QCOMPARE(result.expiryDate, QDate(2026, 10, 30));
        QCOMPARE(result.days, std::optional<int>(9));
        QVERIFY(!result.isEstimate());
        QCOMPARE(result.newAiEstimates.size(), 1);
    }

    void startDatePriority()
    {
        ShelfLifeInput in = input(QStringLiteral("牛奶"));
        in.aiDays.fridge = 3;
        auto result = resolveShelfLife(in, context());
        QCOMPARE(result.startBasis, StartBasis::EntryDate);
        QCOMPARE(result.expiryDate, QDate(2026, 10, 24));

        in.purchaseDate = QDate(2026, 10, 19);
        result = resolveShelfLife(in, context());
        QCOMPARE(result.startBasis, StartBasis::PurchaseDate);
        QCOMPARE(result.expiryDate, QDate(2026, 10, 22));

        in.productionDate = QDate(2026, 10, 18);
        result = resolveShelfLife(in, context());
        QCOMPARE(result.startBasis, StartBasis::ProductionDate);
        QCOMPARE(result.expiryDate, QDate(2026, 10, 21));
    }

    void chainOrder()
    {
        ShelfLifeInput in = input(QStringLiteral("猪肉"));
        in.aiDays.fridge = 5;

        auto result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::Reference);
        QCOMPARE(result.days, std::optional<int>(3));
        QVERIFY(result.note.contains(QStringLiteral("USDA FoodKeeper")));

        rules.setShelfLife({ QStringLiteral("猪肉"), Storage::Fridge, 4 });
        result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::LocalRule);
        QCOMPARE(result.days, std::optional<int>(4));

        in.labelledDays = 10;
        result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::Package);
        QCOMPARE(result.days, std::optional<int>(10));

        in.manualDays = 6;
        in.packageExpiry = QDate(2026, 12, 1);
        result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::Manual);
        QCOMPARE(result.expiryDate, QDate(2026, 10, 27));
    }

    void referenceFallsBackToParent()
    {
        const auto result = resolveShelfLife(input(QStringLiteral("五花肉"), Storage::Freezer), context());
        QCOMPARE(result.source, ShelfLifeSource::Reference);
        QCOMPARE(result.days, std::optional<int>(120));
        QVERIFY(result.note.contains(QStringLiteral("猪肉")));
    }

    void listedRowWithNullDoesNotUseAncestor()
    {
        // 圣女果 is listed without a fridge value; its parent 西红柿 has one but must not be used.
        ShelfLifeInput in = input(QStringLiteral("圣女果"));
        in.aiDays.fridge = 6;
        auto result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::AiEstimate);
        QCOMPARE(result.days, std::optional<int>(6));
        in.storage = Storage::Room;
        result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::Reference);
        QCOMPARE(result.days, std::optional<int>(2));
    }

    void referenceWithoutValueForStorageFallsThrough()
    {
        ShelfLifeInput in = input(QStringLiteral("猪肉"), Storage::Room);
        in.aiDays.room = 1;
        const auto result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::AiEstimate);
        QCOMPARE(result.days, std::optional<int>(1));
    }

    void missingTableMeansNoReference()
    {
        ShelfLifeInput in = input(QStringLiteral("猪肉"));
        in.aiDays.fridge = 5;
        const ShelfLifeTable empty;
        const auto result = resolveShelfLife(in, { &catalog, &empty, &rules, &cache });
        QCOMPARE(result.source, ShelfLifeSource::AiEstimate);
        QCOMPARE(result.days, std::optional<int>(5));
    }

    void aiEstimateIsCachedAndReused()
    {
        ShelfLifeInput first = input(QStringLiteral("牛奶"));
        first.aiDays = { 7, 30, std::nullopt };
        auto result = resolveShelfLife(first, context());
        QCOMPARE(result.source, ShelfLifeSource::AiEstimate);
        QCOMPARE(result.days, std::optional<int>(7));
        QCOMPARE(result.newAiEstimates.size(), 2);
        for (const AiCacheEntry &entry : result.newAiEstimates)
            cache.insert(entry.name, entry.storage, entry.days);

        // A later import gets a different AI answer; the first one is kept for consistency.
        ShelfLifeInput second = input(QStringLiteral("牛奶"));
        second.aiDays = { 14, 60, 3 };
        result = resolveShelfLife(second, context());
        QCOMPARE(result.days, std::optional<int>(7));
        QCOMPARE(result.newAiEstimates.size(), 1); // only the room value is new
        QCOMPARE(result.newAiEstimates.first().storage, Storage::Room);

        second.storage = Storage::Freezer;
        QCOMPARE(resolveShelfLife(second, context()).days, std::optional<int>(30));
    }

    void uncategorizedSkipsRulesAndCache()
    {
        ShelfLifeInput in = input(QStringLiteral("未归类"));
        in.aiDays.fridge = 4;
        rules.setShelfLife({ QStringLiteral("未归类"), Storage::Fridge, 99 });
        const auto result = resolveShelfLife(in, context());
        QCOMPARE(result.source, ShelfLifeSource::AiEstimate);
        QCOMPARE(result.days, std::optional<int>(4));
        QVERIFY(result.newAiEstimates.isEmpty());
    }

    void nothingKnown()
    {
        const auto result = resolveShelfLife(input(QStringLiteral("淡奶油")), context());
        QCOMPARE(result.source, ShelfLifeSource::None);
        QVERIFY(!result.expiryDate.isValid());
        QVERIFY(!result.days);
    }

    void storageChangeRecomputesWithoutAi()
    {
        FridgeItem item;
        item.canonicalName = QStringLiteral("五花肉");
        item.entryDate = QDate(2026, 10, 21);
        item.purchaseDate.date = item.entryDate;
        item.referenceConditionsConfirmed = true;
        item.aiDays = { 2, 90, std::nullopt };
        item.applyShelfLife(resolveShelfLife(item.shelfLifeInput(), context()));
        QCOMPARE(item.shelfLifeSource, ShelfLifeSource::Reference);
        QCOMPARE(item.expiryDate, QDate(2026, 10, 24));

        item.storage = Storage::Freezer;
        item.applyShelfLife(resolveShelfLife(item.shelfLifeInput(), context()));
        QCOMPARE(item.shelfLifeDays, std::optional<int>(120));
        QCOMPARE(item.expiryDate, QDate(2027, 2, 18));

        item.storage = Storage::Room;
        item.applyShelfLife(resolveShelfLife(item.shelfLifeInput(), context()));
        QCOMPARE(item.shelfLifeSource, ShelfLifeSource::None);
    }

    void referenceRequiresPurchaseDate()
    {
        auto in = input(QStringLiteral("猪肉"));
        in.productionDate = QDate(2026, 10, 1);
        auto result = resolveShelfLife(in, context());
        QVERIFY(!result.expiryDate.isValid());
        QCOMPARE(result.startBasis, StartBasis::None);
        in.purchaseDate = QDate(2026, 10, 20);
        QVERIFY(!resolveShelfLife(in, context()).expiryDate.isValid());
        in.referenceConditionsConfirmed = true;
        result = resolveShelfLife(in, context());
        QCOMPARE(result.expiryDate, QDate(2026, 10, 23));
        QCOMPARE(result.startBasis, StartBasis::PurchaseDate);
    }

    void storageSuggestion()
    {
        const ShelfLifeContext ctx = context();
        QCOMPARE(suggestStorage(QStringLiteral("土豆"), Storage::Fridge, false, ctx).storage, Storage::Room);
        QCOMPARE(suggestStorage(QStringLiteral("土豆"), Storage::Fridge, false, ctx).source, StorageSource::Reference);
        QCOMPARE(suggestStorage(QStringLiteral("猪肉"), Storage::Freezer, false, ctx).storage, Storage::Freezer);
        QCOMPARE(suggestStorage(QStringLiteral("猪肉"), std::nullopt, false, ctx).storage, Storage::Fridge);
        QCOMPARE(suggestStorage(QStringLiteral("牛奶"), Storage::Fridge, false, ctx).source, StorageSource::AiEstimate);
        QCOMPARE(suggestStorage(QStringLiteral("豆瓣酱"), std::nullopt, true, ctx).storage, Storage::Room);

        rules.setStorage({ QStringLiteral("猪肉"), Storage::Freezer });
        const auto ruled = suggestStorage(QStringLiteral("猪肉"), Storage::Fridge, false, context());
        QCOMPARE(ruled.storage, Storage::Freezer);
        QCOMPARE(ruled.source, StorageSource::LocalRule);
    }

    void proposalsNeedConfirmation()
    {
        const auto proposal = proposeShelfLifeRule(QStringLiteral("鸡蛋"), Storage::Fridge, 30);
        QVERIFY(proposal);
        QCOMPARE(proposal->question, QStringLiteral("以后「鸡蛋」冷藏都按 30 天算吗？"));
        // A proposal alone changes nothing.
        QVERIFY(!rules.shelfLifeDays(QStringLiteral("鸡蛋"), Storage::Fridge));
        QVERIFY(!proposeShelfLifeRule(QStringLiteral("未归类"), Storage::Fridge, 3));
        QVERIFY(!proposeShelfLifeRule(QStringLiteral("鸡蛋"), Storage::Fridge, 0));
        QVERIFY(proposeStorageRule(QStringLiteral("鸡蛋"), Storage::Room));
    }

    void keysRoundTrip()
    {
        for (auto source : { ShelfLifeSource::None, ShelfLifeSource::Package, ShelfLifeSource::Manual,
                             ShelfLifeSource::LocalRule, ShelfLifeSource::Reference, ShelfLifeSource::AiEstimate }) {
            QCOMPARE(shelfLifeSourceFromKey(shelfLifeSourceKey(source)), source);
            QVERIFY(!shelfLifeSourceLabel(source).isEmpty());
        }
        QCOMPARE(shelfLifeSourceLabel(ShelfLifeSource::Package), QStringLiteral("包装标注"));
        QCOMPARE(shelfLifeSourceLabel(ShelfLifeSource::LocalRule), QStringLiteral("你的设置"));
        QCOMPARE(shelfLifeSourceLabel(ShelfLifeSource::Reference), QStringLiteral("参考表"));
        QCOMPARE(shelfLifeSourceLabel(ShelfLifeSource::AiEstimate), QStringLiteral("AI 估计"));
    }
};

QTEST_GUILESS_MAIN(ShelfLifeTest)
#include "shelf-life-test.moc"
