#include "recipes/xiachufang-yaml-parser.h"

#include <QFile>
#include <QTest>

using namespace whilefresh::recipes;

namespace {

QByteArray sample(const char *name)
{
    QFile file(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR "/") + QLatin1String(name));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QByteArray minimalDocument(const QByteArray &version, const QByteArray &status)
{
    return "schema_version: '" + version + "'\nquery: {keyword: x, page: 1}\noperation: search\n"
           "source: {name: xiachufang}\nstatus:\n  code: " + status + "\n  message: m\n  incomplete: []\n"
           "  warnings: []\nresults: []\n";
}

}

class XiachufangYamlParserTest : public QObject
{
    Q_OBJECT

private slots:
    void searchSampleGivesSummaries()
    {
        const QByteArray yaml = sample("search-tomato-egg.yaml");
        QVERIFY(!yaml.isEmpty());
        const ParsedRecipeDocument parsed = parseXiachufangYaml(yaml);
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.queryKeyword, QStringLiteral("番茄炒蛋"));
        QCOMPARE(*parsed.operation, RecipeDocument::Operation::Search);
        const RecipeSearchResult &search = parsed.document->search;
        QCOMPARE(parsed.document->schemaVersion, QStringLiteral("1.0"));
        QCOMPARE(search.keyword, QStringLiteral("番茄炒蛋"));
        QCOMPARE(search.page, 1);
        QCOMPARE(search.items.size(), 2);
        QCOMPARE(search.totalCandidates, 2);
        QVERIFY(search.partial);
        QCOMPARE(search.incomplete, QStringList { QStringLiteral("full_details") });
        const RecipeSummary &first = search.items.first();
        QCOMPARE(first.id, QStringLiteral("104493058"));
        QCOMPARE(first.name, QStringLiteral("超嫩滑的番茄炒蛋"));
        QCOMPARE(first.attribution.sourceUrl, QStringLiteral("https://www.xiachufang.com/recipe/104493058/"));
        QCOMPARE(first.attribution.sourceName, QStringLiteral("xiachufang"));
        QCOMPARE(first.attribution.provider, QStringLiteral("direct"));
        QVERIFY(first.imageUrl.startsWith(QStringLiteral("https://i2.chuimg.com/")));
    }

    void secondSearchSample()
    {
        const ParsedRecipeDocument parsed = parseXiachufangYaml(sample("search-shrimp.yaml"));
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.document->search.items.size(), 2);
        QCOMPARE(parsed.document->search.items.first().id, QStringLiteral("103468406"));
        QVERIFY(parsed.document->search.items.first().name.contains(QStringLiteral("虾仁")));
    }

    void detailSampleKeepsStepsIngredientsAndTipsApart()
    {
        const ParsedRecipeDocument parsed = parseXiachufangYaml(sample("detail-lexmount-103468406.yaml"));
        QVERIFY(parsed.ok());
        QCOMPARE(*parsed.operation, RecipeDocument::Operation::Detail);
        QCOMPARE(parsed.queryId, QStringLiteral("103468406"));
        QVERIFY(parsed.document->recipe.has_value());
        const WebRecipe &recipe = *parsed.document->recipe;
        QVERIFY(!recipe.partial);
        QCOMPARE(recipe.id, QStringLiteral("103468406"));
        QCOMPARE(recipe.name, QStringLiteral("【虾仁滑蛋】超嫩滑，复刻茶餐厅经典"));
        QCOMPARE(recipe.attribution.sourceUrl, QStringLiteral("https://www.xiachufang.com/recipe/103468406/"));
        QCOMPARE(recipe.attribution.provider, QStringLiteral("lexmount"));
        QVERIFY(recipe.imageUrl.startsWith(QStringLiteral("https://i2.chuimg.com/")));
        QVERIFY(recipe.rating.has_value());
        QCOMPARE(*recipe.rating, 8.1);
        QVERIFY(!recipe.ratingCount.has_value());

        QCOMPARE(recipe.steps.size(), 2);
        QCOMPARE(recipe.steps[0].order, 1);
        QCOMPARE(recipe.steps[0].text, QStringLiteral("准备食材"));
        QVERIFY(recipe.steps[0].imageUrl.startsWith(QStringLiteral("https://i2.chuimg.com/")));
        QCOMPARE(recipe.steps[1].order, 2);
        QVERIFY(recipe.steps[1].text.startsWith(QStringLiteral("虾仁冲洗后")));

        QCOMPARE(recipe.tips.size(), 1);
        QVERIFY(recipe.tips.first().contains(QStringLiteral("冷冻虾仁")));
        for (const RecipeStep &step : recipe.steps) {
            QVERIFY(!step.text.contains(QStringLiteral("冷冻虾仁")));
        }

        QCOMPARE(recipe.ingredients.size(), 3);
        QCOMPARE(recipe.ingredients[0].raw, QStringLiteral("鸡蛋 4个"));
        QCOMPARE(recipe.ingredients[0].name, QStringLiteral("鸡蛋"));
        QCOMPARE(recipe.ingredients[0].amount, QStringLiteral("4个"));
        QCOMPARE(recipe.ingredients[0].required, QStringLiteral("unknown"));
        QCOMPARE(recipe.ingredients[2].raw, QStringLiteral("葱 1束"));
    }

    void secondDetailSampleHasNoTips()
    {
        const ParsedRecipeDocument parsed = parseXiachufangYaml(sample("detail-lexmount-107114615.yaml"));
        QVERIFY(parsed.ok());
        const WebRecipe &recipe = *parsed.document->recipe;
        QCOMPARE(recipe.name, QStringLiteral("清炒虾仁"));
        QCOMPARE(*recipe.rating, 7.2);
        QVERIFY(recipe.tips.isEmpty());
        QCOMPARE(recipe.ingredients[0].raw, QStringLiteral("南美虾 12"));
        QCOMPARE(recipe.steps[1].text, QStringLiteral("剥虾小技巧，放冰箱冷冻20分钟拿出来非常好剥"));
    }

    void partialDetailSetsTheFlag()
    {
        const ParsedRecipeDocument parsed = parseXiachufangYaml(sample("detail-partial-synthetic.yaml"));
        QVERIFY(parsed.ok());
        const WebRecipe &recipe = *parsed.document->recipe;
        QVERIFY(recipe.partial);
        QCOMPARE(recipe.incomplete, QStringList { QStringLiteral("steps") });
        QVERIFY(recipe.steps.isEmpty());
        QCOMPARE(recipe.ingredients.size(), 4);
        QVERIFY(recipe.tips.isEmpty());
    }

    void errorStatusBecomesTypedErrors()
    {
        const ParsedRecipeDocument blocked = parseXiachufangYaml(sample("error-detail-blocked.yaml"));
        QVERIFY(!blocked.ok());
        QCOMPARE(blocked.error.kind, RecipeErrorKind::Unavailable);
        QVERIFY(blocked.error.detail.contains(QStringLiteral("unrecognized detail page")));
        QCOMPARE(blocked.queryId, QStringLiteral("107722218"));

        const ParsedRecipeDocument invalid = parseXiachufangYaml(sample("error-invalid-id.yaml"));
        QVERIFY(!invalid.ok());
        QCOMPARE(invalid.error.kind, RecipeErrorKind::InvalidQuery);
        QCOMPARE(invalid.queryId, QStringLiteral("abc"));
    }

    void schemaVersion()
    {
        const ParsedRecipeDocument minor = parseXiachufangYaml(minimalDocument("1.7", "complete"));
        QVERIFY2(minor.ok(), qPrintable(minor.error.detail));

        const ParsedRecipeDocument unknown = parseXiachufangYaml(minimalDocument("2.0", "complete"));
        QVERIFY(!unknown.ok());
        QCOMPARE(unknown.error.kind, RecipeErrorKind::BadResponse);
        QVERIFY(unknown.error.detail.contains(QStringLiteral("2.0")));

        QVERIFY(!parseXiachufangYaml(minimalDocument("", "complete")).ok());
    }

    void nullResultsAreEmpty()
    {
        QByteArray yaml = minimalDocument("1.0", "complete");
        yaml.replace("results: []", "results: null");
        const ParsedRecipeDocument parsed = parseXiachufangYaml(yaml);
        QVERIFY2(parsed.ok(), qPrintable(parsed.error.detail));
        QVERIFY(parsed.document->search.items.isEmpty());
        QVERIFY(!parsed.document->search.partial);
    }

    void unknownStatusIsRejected()
    {
        QCOMPARE(parseXiachufangYaml(minimalDocument("1.0", "weird")).error.kind, RecipeErrorKind::BadResponse);
    }

    void malformedInputNeverCrashes_data()
    {
        QTest::addColumn<QByteArray>("yaml");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("scalar") << QByteArray("just text");
        QTest::newRow("list") << QByteArray("- a\n- b\n");
        QTest::newRow("unterminated flow") << QByteArray("schema_version: '1.0'\nquery: {a: [1, 2\n");
        QTest::newRow("tab indentation") << QByteArray("a:\n\t- b\n  c: d\n");
        QTest::newRow("binary") << QByteArray("\x00\xff\xfe\x01 :: [[{", 9);
        QTest::newRow("results not a list")
            << QByteArray("schema_version: '1.0'\noperation: search\nstatus: {code: complete}\nresults: 5\n");
        QTest::newRow("step not a mapping")
            << QByteArray("schema_version: '1.0'\noperation: detail\nstatus: {code: complete}\n"
                          "results:\n- id: '1'\n  name: x\n  steps:\n  - just a string\n");
        QTest::newRow("rating not a number")
            << QByteArray("schema_version: '1.0'\noperation: detail\nstatus: {code: complete}\n"
                          "results:\n- id: '1'\n  name: x\n  rating: much\n");
        QTest::newRow("missing operation") << QByteArray("schema_version: '1.0'\nstatus: {code: complete}\n");
        QTest::newRow("detail without result")
            << QByteArray("schema_version: '1.0'\noperation: detail\nstatus: {code: complete}\nresults: []\n");
    }

    void malformedInputNeverCrashes()
    {
        QFETCH(QByteArray, yaml);
        const ParsedRecipeDocument parsed = parseXiachufangYaml(yaml);
        QVERIFY(!parsed.ok());
        QCOMPARE(parsed.error.kind, RecipeErrorKind::BadResponse);
        QVERIFY(!parsed.error.detail.isEmpty());
    }
};

QTEST_APPLESS_MAIN(XiachufangYamlParserTest)
#include "xiachufang-yaml-parser-test.moc"
