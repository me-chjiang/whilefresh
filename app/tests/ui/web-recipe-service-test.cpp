#include "ui/web-recipe-service.h"

#include <QSignalSpy>
#include <QTest>

using namespace whilefresh;
using namespace whilefresh::recipes;

namespace {

// Answers only when the test says so, so replies can arrive late or out of order.
class ScriptedSource : public RecipeSource
{
public:
    quint64 search(const QString &keyword, int) override
    {
        searches.append(keyword);
        return nextRequestId();
    }
    quint64 detail(const QString &id) override
    {
        details.append(id);
        return nextRequestId();
    }
    void cancel(quint64 id) override { cancelled.append(id); }
    bool isConfigured() const override { return true; }

    void reply(quint64 id, const RecipeSearchResult &result) { emit searchSucceeded(id, result); }
    void reply(quint64 id, const WebRecipe &recipe) { emit detailSucceeded(id, recipe); }
    void fail(quint64 id, RecipeErrorKind kind) { emit failed(id, { kind, QStringLiteral("detail never shown") }); }

    QStringList searches;
    QStringList details;
    QList<quint64> cancelled;
};

RecipeSearchResult results(const QString &name)
{
    RecipeSearchResult result;
    RecipeSummary summary;
    summary.id = QStringLiteral("1");
    summary.name = name;
    summary.attribution.sourceUrl = QStringLiteral("https://www.xiachufang.com/recipe/1/");
    result.items.append(summary);
    return result;
}

WebRecipe recipe(const QString &id)
{
    WebRecipe r;
    r.id = id;
    r.name = QStringLiteral("清炒虾仁");
    r.ingredients.append({ QStringLiteral("虾仁 200克"), QStringLiteral("虾仁"), QStringLiteral("200克"), {}, {}, {}, {}, {} });
    r.steps.append({ 1, QStringLiteral("下锅"), {} });
    r.tips.append(QStringLiteral("火要大"));
    r.attribution = { QStringLiteral("xiachufang"), QStringLiteral("lexmount"), QStringLiteral("https://www.xiachufang.com/recipe/") + id + QLatin1Char('/'), {} };
    return r;
}

}

class WebRecipeServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void searchShowsResultsForTheCurrentRequestOnly()
    {
        auto owned = std::make_unique<ScriptedSource>();
        ScriptedSource *source = owned.get();
        ui::WebRecipeService service(std::move(owned));

        service.search(QStringLiteral(" 虾仁 "));
        QCOMPARE(source->searches, QStringList { QStringLiteral("虾仁") });
        QCOMPARE(service.searchState(), QStringLiteral("busy"));

        service.search(QStringLiteral("番茄"));
        QCOMPARE(source->cancelled, QList<quint64> { 1 });
        source->reply(1, results(QStringLiteral("晚到的虾仁"))); // stale: request 1 was replaced
        QCOMPARE(service.searchState(), QStringLiteral("busy"));
        QVERIFY(service.results().isEmpty());

        source->reply(2, results(QStringLiteral("番茄炒蛋")));
        QCOMPARE(service.searchState(), QStringLiteral("done"));
        QCOMPARE(service.results().size(), 1);
        QCOMPARE(service.results().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("番茄炒蛋"));
    }

    void detailCarriesAttributionAndIsCachedInMemory()
    {
        auto owned = std::make_unique<ScriptedSource>();
        ScriptedSource *source = owned.get();
        ui::WebRecipeService service(std::move(owned));

        service.requestDetail(QStringLiteral("42"));
        QCOMPARE(service.detailState(), QStringLiteral("busy"));
        source->reply(1, recipe(QStringLiteral("42")));
        QCOMPARE(service.detailState(), QStringLiteral("done"));
        const QVariantMap detail = service.detail();
        QCOMPARE(detail.value(QStringLiteral("name")).toString(), QStringLiteral("清炒虾仁"));
        QCOMPARE(detail.value(QStringLiteral("ingredients")).toList().first().toMap().value(QStringLiteral("raw")).toString(),
            QStringLiteral("虾仁 200克"));
        QCOMPARE(detail.value(QStringLiteral("tips")).toStringList(), QStringList { QStringLiteral("火要大") });
        const QVariantMap attribution = detail.value(QStringLiteral("attribution")).toMap();
        QCOMPARE(attribution.value(QStringLiteral("source")).toString(), QStringLiteral("下厨房"));
        QCOMPARE(attribution.value(QStringLiteral("url")).toString(), QStringLiteral("https://www.xiachufang.com/recipe/42/"));

        service.requestDetail(QStringLiteral("42")); // served from memory, no second request
        QCOMPARE(source->details.size(), 1);
        QCOMPARE(service.detailState(), QStringLiteral("done"));
    }

    void errorsBecomeShortChineseLabels()
    {
        auto owned = std::make_unique<ScriptedSource>();
        ScriptedSource *source = owned.get();
        ui::WebRecipeService service(std::move(owned));

        service.requestDetail(QStringLiteral("1"));
        source->fail(1, RecipeErrorKind::Blocked);
        QCOMPARE(service.detailState(), QStringLiteral("error"));
        QCOMPARE(service.detailError(), QStringLiteral("下厨房拦截"));

        service.search(QStringLiteral("x"));
        source->fail(2, RecipeErrorKind::NotConfigured);
        QCOMPARE(service.searchError(), QStringLiteral("未配置 Lexmount"));

        QCOMPARE(ui::WebRecipeService::errorLabel(RecipeErrorKind::NetworkError), QStringLiteral("网络错误"));
        QCOMPARE(ui::WebRecipeService::errorLabel(RecipeErrorKind::Timeout), QStringLiteral("超时"));
        QCOMPARE(ui::WebRecipeService::errorLabel(RecipeErrorKind::BadResponse), QStringLiteral("菜谱源不可用"));
    }

    void cancelStopsBothRequests()
    {
        auto owned = std::make_unique<ScriptedSource>();
        ScriptedSource *source = owned.get();
        ui::WebRecipeService service(std::move(owned));
        service.search(QStringLiteral("x"));
        service.requestDetail(QStringLiteral("2"));
        service.cancel();
        QCOMPARE(source->cancelled.size(), 2);
        QCOMPARE(service.searchState(), QStringLiteral("idle"));
        QCOMPARE(service.detailState(), QStringLiteral("idle"));
        source->reply(2, recipe(QStringLiteral("2"))); // a late reply after cancel is dropped
        QCOMPARE(service.detailState(), QStringLiteral("idle"));
    }
};

QTEST_GUILESS_MAIN(WebRecipeServiceTest)
#include "web-recipe-service-test.moc"
