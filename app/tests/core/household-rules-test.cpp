#include "core/import-duplicates.h"
#include "core/pantry.h"
#include "core/shopping-list.h"

#include "core-test-support.h"

#include <QTest>

using namespace whilefresh::core;
using namespace whilefresh::core::testing;

class HouseholdRulesTest : public QObject
{
    Q_OBJECT

    IngredientCatalog catalog = smallCatalog();

private slots:
    void pantryDefaultsAndOverrides()
    {
        PantryRules rules(&catalog);
        QVERIFY(rules.isPantry(QStringLiteral("盐")));
        QVERIFY(rules.isAtHome(QStringLiteral("盐")));
        QVERIFY(!rules.isPantry(QStringLiteral("豆瓣酱")));
        QVERIFY(rules.isBulk(QStringLiteral("鸡蛋")));
        QVERIFY(!rules.isBulk(QStringLiteral("猪肉")));

        rules.setOverride({ QStringLiteral("盐"), std::nullopt, std::nullopt, true });
        QVERIFY(rules.isPantry(QStringLiteral("盐")));
        QVERIFY(!rules.isAtHome(QStringLiteral("盐")));
        QVERIFY(!rules.namesAtHome().contains(QStringLiteral("盐")));

        rules.setOverride({ QStringLiteral("豆瓣酱"), true, std::nullopt, false });
        rules.setOverride({ QStringLiteral("白糖"), false, std::nullopt, false });
        rules.setOverride({ QStringLiteral("鸡蛋"), std::nullopt, false, false });
        rules.setOverride({ QStringLiteral("自制辣酱"), true, std::nullopt, false });
        QVERIFY(rules.pantryNames().contains(QStringLiteral("豆瓣酱")));
        QVERIFY(!rules.pantryNames().contains(QStringLiteral("白糖")));
        QCOMPARE(rules.pantryNames().last(), QStringLiteral("自制辣酱"));
        QVERIFY(!rules.isBulk(QStringLiteral("鸡蛋")));
    }

    void bulkItemsSurviveUse()
    {
        QCOMPARE(statusAfterUse(true), ItemStatus::InStock);
        QCOMPARE(statusAfterUse(false), ItemStatus::Used);
    }

    void suggestionsAreProposalsOnly()
    {
        PantryRules rules(&catalog);
        QVERIFY(!suggestionForPurchase(QStringLiteral("生抽"), rules)); // already at home

        rules.setOverride({ QStringLiteral("生抽"), std::nullopt, std::nullopt, true });
        const auto have = suggestionForPurchase(QStringLiteral("生抽"), rules);
        QVERIFY(have);
        QCOMPARE(have->kind, SuggestionKind::MarkPantryHave);
        QVERIFY(rules.isOut(QStringLiteral("生抽"))); // nothing changed by itself

        const auto add = suggestionForPurchase(QStringLiteral("豆瓣酱"), rules);
        QVERIFY(add);
        QCOMPARE(add->kind, SuggestionKind::AddToPantry);

        const auto buy = suggestionForAbsent(QStringLiteral("盐"), rules);
        QVERIFY(buy);
        QCOMPARE(buy->kind, SuggestionKind::AddToShopping);
        QVERIFY(suggestionForAbsent(QStringLiteral("鸡蛋"), rules)); // bulk
        QVERIFY(!suggestionForAbsent(QStringLiteral("猪肉"), rules));
        QVERIFY(!suggestionForPurchase(QStringLiteral("未归类"), rules));
    }

    void shoppingFromMissingGroups()
    {
        Recipe recipe;
        recipe.id = QStringLiteral("t/汤");
        recipe.name = QStringLiteral("奶油土豆汤");
        recipe.ingredients = { { QStringLiteral("牛奶或淡奶油"), {}, false, QStringLiteral("ingredient"),
                                 { { QStringLiteral("牛奶"), QStringLiteral("淡奶油") } } },
                               { QStringLiteral("土豆"), {}, false, QStringLiteral("ingredient"), { { QStringLiteral("土豆") } } },
                               { QStringLiteral("盐"), {}, false, QStringLiteral("ingredient"), { { QStringLiteral("盐") } } },
                               { QStringLiteral("五花肉"), {}, true, QStringLiteral("ingredient"), { { QStringLiteral("五花肉") } } } };
        RecipeMatcher matcher(&catalog, { { QStringLiteral("盐"), 0, true, false, false } });
        const QList<ShoppingEntry> entries = shoppingEntriesForMissing(matcher.match(recipe), &catalog);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).displayName(), QStringLiteral("牛奶 或 淡奶油"));
        QCOMPARE(entries.at(0).category, QStringLiteral("egg_dairy"));
        QCOMPARE(entries.at(0).note, QStringLiteral("奶油土豆汤"));
        QCOMPARE(entries.at(1).category, QStringLiteral("vegetable"));
    }

    void shoppingGroupsAndText()
    {
        const QList<ShoppingEntry> entries{
            makeShoppingEntry({ QStringLiteral("生抽") }, ShoppingOrigin::Pantry, {}, &catalog),
            makeShoppingEntry({ QStringLiteral("牛奶"), QStringLiteral("淡奶油") }, ShoppingOrigin::Recipe, {}, &catalog),
            makeShoppingEntry({ QStringLiteral("土豆") }, ShoppingOrigin::Recipe, {}, &catalog),
            makeShoppingEntry({ QStringLiteral("西红柿") }, ShoppingOrigin::Recipe, {}, &catalog),
            makeShoppingEntry({ QStringLiteral("不认识的") }, ShoppingOrigin::Manual, {}, &catalog),
        };
        const QList<ShoppingSection> sections = groupShoppingEntries(entries, &catalog);
        QCOMPARE(sections.size(), 4);
        QCOMPARE(sections.at(0).title, QStringLiteral("蔬菜"));
        QCOMPARE(sections.at(0).entries.size(), 2);
        QCOMPARE(sections.last().title, QStringLiteral("其他"));
        QCOMPARE(shoppingListText(entries, &catalog),
                 QStringLiteral("待买清单\n\n【蔬菜】\n- 土豆\n- 西红柿\n\n【蛋奶】\n- 牛奶 或 淡奶油\n\n"
                                "【调料】\n- 生抽\n\n【其他】\n- 不认识的\n"));
        QCOMPARE(entries.at(1).dedupeKey(),
                 makeShoppingEntry({ QStringLiteral("淡奶油"), QStringLiteral("牛奶") }, ShoppingOrigin::Manual, {}, &catalog)
                     .dedupeKey());
    }

    void duplicateImages()
    {
        const QList<ImportFingerprint> previous{ { 7, QDateTime(QDate(2026, 10, 1), QTime(8, 0)), { QStringLiteral("abc") }, {} } };
        const DuplicateCheck same = checkSameImage({ QStringLiteral("zzz"), QStringLiteral("abc") }, previous);
        QCOMPARE(same.kind, DuplicateCheck::Kind::SameImage);
        QCOMPARE(same.importId, qint64(7));
        QVERIFY(!same.message().isEmpty());
        QVERIFY(!checkSameImage({ QStringLiteral("zzz") }, previous).isDuplicate());
    }

    void similarItems()
    {
        using Pair = QPair<QString, QString>;
        const QDateTime now(QDate(2026, 10, 21), QTime(12, 0));
        const QList<Pair> earlierItems{ { "鸡蛋", "10枚" }, { "牛奶", "950ml" }, { "土豆", "500g" },
                                        { "西红柿", "500g" }, { "五花肉", "300g" } };
        const QList<ImportFingerprint> previous{ { 3, now.addDays(-1), {}, earlierItems },
                                                 { 2, now.addDays(-10), {}, earlierItems } };
        QList<Pair> current = earlierItems;
        current[4] = { "五花肉", "500g" }; // 4 of 5 match = 80%
        const DuplicateCheck similar = checkSimilarItems(current, now, previous);
        QCOMPARE(similar.kind, DuplicateCheck::Kind::SimilarItems);
        QCOMPARE(similar.importId, qint64(3));
        QCOMPARE(similar.overlap, 0.8);

        current[3] = { "西红柿", "1kg" }; // 60%
        QVERIFY(!checkSimilarItems(current, now, previous).isDuplicate());
        // Same items but only an old import to compare with.
        QVERIFY(!checkSimilarItems(earlierItems, now, { previous.at(1) }).isDuplicate());
        QVERIFY(!checkSimilarItems({ { "鸡蛋", "10枚" } }, now, { { 9, now, {}, { { "鸡蛋", "10枚" } } } }).isDuplicate());
    }
};

QTEST_GUILESS_MAIN(HouseholdRulesTest)
#include "household-rules-test.moc"
