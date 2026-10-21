#include "core/recipe.h"

#include "core-test-support.h"

#include <QTest>

using namespace whilefresh::core;
using namespace whilefresh::core::testing;

namespace {

RecipeIngredient need(const QString &raw, QList<RequirementGroup> groups, bool optional = false,
                      const QString &kind = QStringLiteral("ingredient"))
{
    return { raw, raw, optional, kind, std::move(groups) };
}

Recipe recipe(const QString &name, QList<RecipeIngredient> ingredients)
{
    Recipe r;
    r.id = QStringLiteral("test/") + name;
    r.name = name;
    r.ingredients = std::move(ingredients);
    return r;
}

StockEntry item(const QString &name, qint64 id, bool selected = false, bool nearExpiry = false)
{
    return { name, id, false, selected, nearExpiry };
}

StockEntry pantry(const QString &name)
{
    return { name, 0, true, false, false };
}

QStringList names(const QList<RecipeMatch> &matches)
{
    QStringList list;
    for (const RecipeMatch &match : matches)
        list.append(match.recipe->name);
    return list;
}

}

class RecipeMatcherTest : public QObject
{
    Q_OBJECT

    IngredientCatalog catalog = smallCatalog();

private slots:
    void groupAlternativesAndAllGroups()
    {
        const Recipe r = recipe(QStringLiteral("奶油汤"),
                                { need(QStringLiteral("牛奶或淡奶油"), { { QStringLiteral("牛奶"), QStringLiteral("淡奶油") } }),
                                  need(QStringLiteral("土豆"), { { QStringLiteral("土豆") } }) });
        RecipeMatcher matcher(&catalog, { item(QStringLiteral("淡奶油"), 1) });
        const RecipeMatch match = matcher.match(r);
        QCOMPARE(match.haveCount, 1);
        QCOMPARE(match.missingCount, 1);
        QCOMPARE(match.groups.first().satisfiedBy.first().itemId, qint64(1));
        QCOMPARE(match.missingGroups(), QList<RequirementGroup>{ { QStringLiteral("土豆") } });
    }

    void descendantsSatisfyButNotAncestors()
    {
        const Recipe wantsPork = recipe(QStringLiteral("回锅肉"), { need(QStringLiteral("猪肉"), { { QStringLiteral("猪肉") } }) });
        const Recipe wantsBelly = recipe(QStringLiteral("红烧肉"), { need(QStringLiteral("五花肉"), { { QStringLiteral("五花肉") } }) });
        RecipeMatcher withBelly(&catalog, { item(QStringLiteral("五花肉"), 1) });
        QCOMPARE(withBelly.match(wantsPork).missingCount, 0);
        QCOMPARE(withBelly.match(wantsBelly).missingCount, 0);
        RecipeMatcher withPork(&catalog, { item(QStringLiteral("猪肉"), 2) });
        QCOMPARE(withPork.match(wantsPork).missingCount, 0);
        QCOMPARE(withPork.match(wantsBelly).missingCount, 1);
    }

    void optionalPantryAndIgnored()
    {
        const Recipe r = recipe(QStringLiteral("西红柿炒鸡蛋"),
                                { need(QStringLiteral("西红柿"), { { QStringLiteral("西红柿") } }),
                                  need(QStringLiteral("盐"), { { QStringLiteral("盐") } }),
                                  need(QStringLiteral("生抽"), { { QStringLiteral("酱油") } }),
                                  need(QStringLiteral("糖（可选）"), { { QStringLiteral("白糖") } }, true),
                                  need(QStringLiteral("淡奶油（可选）"), { { QStringLiteral("淡奶油") } }, true),
                                  need(QStringLiteral("冰块"), {}),
                                  need(QStringLiteral("炒锅"), { { QStringLiteral("土豆") } }, false, QStringLiteral("tool")) });
        RecipeMatcher matcher(&catalog, { item(QStringLiteral("圣女果"), 1), pantry(QStringLiteral("盐")),
                                          pantry(QStringLiteral("生抽")) });
        const RecipeMatch match = matcher.match(r);
        QCOMPARE(match.missingCount, 0);
        QCOMPARE(match.haveCount, 3);
        QCOMPARE(match.groups.size(), 5); // ignored and tool lines add no group
        QCOMPARE(match.inventoryUsed, 1);  // pantry entries do not count as fridge items
    }

    void repeatedNeedCountsOnce()
    {
        const Recipe r = recipe(QStringLiteral("双盐"), { need(QStringLiteral("盐"), { { QStringLiteral("盐") } }, true),
                                                         need(QStringLiteral("盐 5g"), { { QStringLiteral("盐") } }) });
        RecipeMatcher matcher(&catalog, {});
        const RecipeMatch match = matcher.match(r);
        QCOMPARE(match.groups.size(), 1);
        QVERIFY(!match.groups.first().optional);
        QCOMPARE(match.missingCount, 1);
    }

    void clearFridgeRanking()
    {
        const QList<Recipe> recipes{
            recipe(QStringLiteral("乙 用两样缺一"), { need(QStringLiteral("五花肉"), { { QStringLiteral("五花肉") } }),
                                                     need(QStringLiteral("西红柿"), { { QStringLiteral("西红柿") } }),
                                                     need(QStringLiteral("牛奶"), { { QStringLiteral("牛奶") } }) }),
            recipe(QStringLiteral("甲 用两样缺一"), { need(QStringLiteral("猪肉"), { { QStringLiteral("猪肉") } }),
                                                     need(QStringLiteral("土豆"), { { QStringLiteral("土豆") } }),
                                                     need(QStringLiteral("牛奶"), { { QStringLiteral("牛奶") } }) }),
            recipe(QStringLiteral("丙 用一样不缺"), { need(QStringLiteral("西红柿"), { { QStringLiteral("西红柿") } }) }),
            recipe(QStringLiteral("丁 用两样缺二"), { need(QStringLiteral("猪肉"), { { QStringLiteral("猪肉") } }),
                                                     need(QStringLiteral("土豆"), { { QStringLiteral("土豆") } }),
                                                     need(QStringLiteral("牛奶"), { { QStringLiteral("牛奶") } }),
                                                     need(QStringLiteral("淡奶油"), { { QStringLiteral("淡奶油") } }) }),
            recipe(QStringLiteral("戊 不用所选"), { need(QStringLiteral("鸡蛋"), { { QStringLiteral("鸡蛋") } }) }),
            recipe(QStringLiteral("己 用两样缺一临期多"), { need(QStringLiteral("五花肉"), { { QStringLiteral("五花肉") } }),
                                                           need(QStringLiteral("土豆"), { { QStringLiteral("土豆") } }),
                                                           need(QStringLiteral("牛奶"), { { QStringLiteral("牛奶") } }) }),
        };
        // Selected: 五花肉 (near expiry), 西红柿, 土豆 (near expiry). 鸡蛋 is in stock but not selected.
        RecipeMatcher matcher(&catalog, { item(QStringLiteral("五花肉"), 1, true, true),
                                          item(QStringLiteral("西红柿"), 2, true, false),
                                          item(QStringLiteral("土豆"), 3, true, true),
                                          item(QStringLiteral("鸡蛋"), 4) });
        const QList<RecipeMatch> ranked = rankForClearFridge(matcher.matchAll(recipes));
        // 己 and 甲 both use 2 selected and miss 1; 己 uses 2 near-expiry items, 甲 too (五花肉 meets
        // 猪肉, 土豆), so they fall back to name order; 乙 uses only 1 near-expiry item.
        QCOMPARE(names(ranked), (QStringList{ QStringLiteral("己 用两样缺一临期多"), QStringLiteral("甲 用两样缺一"),
                                              QStringLiteral("乙 用两样缺一"), QStringLiteral("丁 用两样缺二"),
                                              QStringLiteral("丙 用一样不缺") }));
    }

    void browsingOrder()
    {
        QList<Recipe> recipes{
            recipe(QStringLiteral("B 缺一"), { need(QStringLiteral("牛奶"), { { QStringLiteral("牛奶") } }) }),
            recipe(QStringLiteral("A 不缺用一"), { need(QStringLiteral("土豆"), { { QStringLiteral("土豆") } }) }),
            recipe(QStringLiteral("C 不缺用二"), { need(QStringLiteral("土豆"), { { QStringLiteral("土豆") } }),
                                                   need(QStringLiteral("鸡蛋"), { { QStringLiteral("鸡蛋") } }) }),
        };
        RecipeMatcher matcher(&catalog, { item(QStringLiteral("土豆"), 1), item(QStringLiteral("鸡蛋"), 2) });
        QList<RecipeMatch> matches = matcher.matchAll(recipes);
        sortForBrowsing(matches);
        QCOMPARE(names(matches), (QStringList{ QStringLiteral("C 不缺用二"), QStringLiteral("A 不缺用一"),
                                               QStringLiteral("B 缺一") }));
    }

    void loadsRecipeJson()
    {
        const QByteArray json = R"({"source":{"name":"HowToCook","license":"Unlicense"},
          "recipes":[{"id":"x/a","name":"A","time_minutes":null,"difficulty":2,
                      "ingredients":[{"raw":"葱姜","name":"葱姜","optional":false,"kind":"ingredient",
                                      "canonical":[["葱"],["姜"]]},
                                     {"raw":"旧","name":"旧","optional":false,"kind":"ingredient",
                                      "canonical":["盐","糖"]}],
                      "steps":["1. 做"],"source_url":"https://example.invalid/a"}]})";
        const auto book = RecipeBook::fromJson(json);
        QVERIFY(book);
        QCOMPARE(book->size(), 1);
        const Recipe *a = book->find(QStringLiteral("x/a"));
        QVERIFY(a);
        QVERIFY(!a->timeMinutes);
        QCOMPARE(a->difficulty, std::optional<int>(2));
        QCOMPARE(a->ingredients.at(0).groups.size(), 2);
        QCOMPARE(a->ingredients.at(1).groups, (QList<RequirementGroup>{ { QStringLiteral("盐") }, { QStringLiteral("糖") } }));
        QCOMPARE(book->source().license, QStringLiteral("Unlicense"));
    }
};

QTEST_GUILESS_MAIN(RecipeMatcherTest)
#include "recipe-matcher-test.moc"
