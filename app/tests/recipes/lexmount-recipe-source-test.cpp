#include "fake-lexmount-server.h"

#include "recipes/contract-document.h"
#include "recipes/lexmount-recipe-source.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTest>

using namespace whilefresh::recipes;

namespace {

QJsonObject detailScrape()
{
    return {
        { QStringLiteral("Content"),
            QJsonArray { QJsonObject {
                { QStringLiteral("name"), QStringLiteral("测试番茄炒蛋") },
                { QStringLiteral("cover"), QStringLiteral("https://i2.chuimg.com/cover.jpg") },
                { QStringLiteral("grade"), QStringLiteral("8.4") },
                { QStringLiteral("materials"),
                    QJsonArray { QJsonObject { { QStringLiteral("name"), QStringLiteral("番茄") }, { QStringLiteral("unit"), QStringLiteral("2个") } },
                        QJsonObject { { QStringLiteral("name"), QStringLiteral("鸡蛋") }, { QStringLiteral("unit"), QString() } } } },
                { QStringLiteral("steps"),
                    QJsonArray { QJsonObject { { QStringLiteral("desc"), QStringLiteral("切块。") }, { QStringLiteral("img"), QStringLiteral("/photo/1.jpg") } },
                        QJsonObject { { QStringLiteral("desc"), QStringLiteral("下锅。") }, { QStringLiteral("img"), QJsonValue::Null } } } },
                { QStringLiteral("tip"), QStringLiteral("别炒老。") } } },
        },
        { QStringLiteral("diag"), QJsonObject { { QStringLiteral("title"), QStringLiteral("下厨房") } } },
    };
}

QJsonObject searchScrape()
{
    return {
        { QStringLiteral("Recipe"),
            QJsonArray { QJsonObject { { QStringLiteral("name"), QStringLiteral("虾仁滑蛋") }, { QStringLiteral("url"), QStringLiteral("/recipe/111/") }, { QStringLiteral("cover"), QStringLiteral("//i2.chuimg.com/a.jpg") } },
                QJsonObject { { QStringLiteral("name"), QString() }, { QStringLiteral("url"), QStringLiteral("/recipe/222/") } },
                QJsonObject { { QStringLiteral("name"), QStringLiteral("椒盐虾仁") }, { QStringLiteral("url"), QStringLiteral("/recipe/333/") } } } },
        { QStringLiteral("diag"), QJsonObject { { QStringLiteral("list"), true }, { QStringLiteral("path"), QStringLiteral("/search/") } } },
    };
}

LexmountRecipeSource::Config fastConfig(const FakeLexmountServer &server)
{
    LexmountRecipeSource::Config config;
    config.baseUrl = server.baseUrl();
    config.pollIntervalMs = 10;
    config.stepTimeoutMs = 2000;
    config.httpTimeoutMs = 2000;
    config.pageWaitTimeoutMs = 3000;
    config.totalTimeoutMs = 8000;
    config.cleanupTimeoutMs = 2000;
    return config;
}

LexmountRecipeSource::CredentialsFn credentials()
{
    return [] { return LexmountCredentials { QStringLiteral("test-key"), QStringLiteral("test-project") }; };
}

}

class LexmountRecipeSourceTest : public QObject
{
    Q_OBJECT

private slots:
    void detailWaitsForTheNavigatedPageBeforeScraping()
    {
        FakeLexmountServer server;
        server.detailScrape = detailScrape();
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy ok(&source, &RecipeSource::detailSucceeded);
        QSignalSpy bad(&source, &RecipeSource::failed);
        QSignalSpy document(&source, &LexmountRecipeSource::documentReady);

        const quint64 id = source.detail(QStringLiteral("123456"));
        QCOMPARE(ok.size(), 0);
        QTRY_COMPARE(ok.size(), 1);
        QCOMPARE(bad.size(), 0);
        QCOMPARE(ok.first().at(0).toULongLong(), id);

        const auto recipe = ok.first().at(1).value<WebRecipe>();
        QCOMPARE(recipe.id, QStringLiteral("123456"));
        QCOMPARE(recipe.name, QStringLiteral("测试番茄炒蛋"));
        QCOMPARE(recipe.attribution.sourceUrl, QStringLiteral("https://www.xiachufang.com/recipe/123456/"));
        QCOMPARE(recipe.attribution.provider, QStringLiteral("lexmount"));
        QCOMPARE(recipe.ingredients.size(), 2);
        QCOMPARE(recipe.ingredients[0].raw, QStringLiteral("番茄 2个"));
        QCOMPARE(recipe.ingredients[1].raw, QStringLiteral("鸡蛋"));
        QVERIFY(recipe.ingredients[1].amount.isEmpty());
        QCOMPARE(recipe.steps.size(), 2);
        QCOMPARE(recipe.steps[0].imageUrl, QStringLiteral("https://www.xiachufang.com/photo/1.jpg"));
        QCOMPARE(recipe.tips, QStringList { QStringLiteral("别炒老。") });
        QCOMPARE(*recipe.rating, 8.4);
        QVERIFY(!recipe.partial);
        QCOMPARE(document.size(), 1);

        // The state polls after Page.navigate saw about:blank twice; the scrape came only after the
        // target page was reported.
        const QStringList &c = server.commands;
        const qsizetype navigate = c.indexOf(QStringLiteral("Page.navigate"));
        const qsizetype scrape = c.indexOf(QStringLiteral("Runtime.evaluate:scrape"));
        QVERIFY(navigate >= 0 && scrape > navigate);
        QVERIFY(c.mid(navigate, scrape - navigate).count(QStringLiteral("Runtime.evaluate:state")) >= 3);
        QCOMPARE(c.count(QStringLiteral("Runtime.evaluate:scrape")), 1);

        QTRY_COMPARE(server.deletes(), 1);
        QTRY_COMPARE(source.pendingCleanups(), 0);
    }

    void sessionIsDeletedWithTheSubmodulesRequest()
    {
        FakeLexmountServer server;
        server.detailScrape = detailScrape();
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy ok(&source, &RecipeSource::detailSucceeded);
        source.detail(QStringLiteral("1"));
        QTRY_COMPARE(server.deletes(), 1);
        for (const auto &record : server.http) {
            QCOMPARE(record.headers.value("x-api-key"), QByteArray("test-key"));
            QCOMPARE(record.headers.value("x-project-id"), QByteArray("test-project"));
        }
        const auto &del = server.http.last();
        QCOMPARE(del.method, QByteArray("DELETE"));
        QCOMPARE(del.path, QByteArray("/instance"));
        QCOMPARE(del.headers.value("content-type"), QByteArray("application/json"));
        QCOMPARE(QJsonDocument::fromJson(del.body).object().value(QLatin1String("session_id")).toString(), QStringLiteral("s1"));
    }

    void searchSkipsNamelessRows()
    {
        FakeLexmountServer server;
        server.searchScrape = searchScrape();
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy ok(&source, &RecipeSource::searchSucceeded);
        source.search(QStringLiteral("虾仁"));
        QTRY_COMPARE(ok.size(), 1);
        const auto result = ok.first().at(1).value<RecipeSearchResult>();
        QCOMPARE(result.keyword, QStringLiteral("虾仁"));
        QCOMPARE(result.items.size(), 2);
        QCOMPARE(result.items[0].id, QStringLiteral("111"));
        QCOMPARE(result.items[0].imageUrl, QStringLiteral("https://i2.chuimg.com/a.jpg"));
        QCOMPARE(result.items[1].id, QStringLiteral("333"));
        QVERIFY(result.warnings.contains(QStringLiteral("skipped_unusable_rows")));
        QVERIFY(result.partial);
        QTRY_COMPARE(server.deletes(), 1);
        QVERIFY(server.http.at(0).body.contains("browser_mode"));
    }

    void pingsDoNotDisturbTheFlow()
    {
        FakeLexmountServer server;
        server.pingBeforeReplies = true;
        server.detailScrape = detailScrape();
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy ok(&source, &RecipeSource::detailSucceeded);
        source.detail(QStringLiteral("1"));
        QTRY_COMPARE(ok.size(), 1);
    }

    void aNonRecipePageIsReportedAsBlockedWithItsTitle()
    {
        FakeLexmountServer server;
        server.landedPath = QStringLiteral("/auth/verify");
        server.detailScrape = { { QStringLiteral("diag"),
            QJsonObject { { QStringLiteral("title"), QStringLiteral("安全验证") }, { QStringLiteral("host"), QStringLiteral("www.xiachufang.com") },
                { QStringLiteral("path"), QStringLiteral("/auth/verify") } } } };
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy bad(&source, &RecipeSource::failed);
        source.detail(QStringLiteral("1"));
        QTRY_COMPARE(bad.size(), 1);
        const auto error = bad.first().at(1).value<RecipeError>();
        QCOMPARE(error.kind, RecipeErrorKind::Blocked);
        QVERIFY(error.detail.contains(QStringLiteral("安全验证")));
        QVERIFY(error.detail.contains(QStringLiteral("www.xiachufang.com/auth/verify")));
        QVERIFY(!error.detail.contains(QStringLiteral("test-key")));
        QTRY_COMPARE(server.deletes(), 1);
    }

    void searchWithoutResultListOnASearchPageIsJustEmpty()
    {
        FakeLexmountServer server;
        server.searchScrape = { { QStringLiteral("Recipe"), QJsonArray() },
            { QStringLiteral("diag"), QJsonObject { { QStringLiteral("list"), false }, { QStringLiteral("path"), QStringLiteral("/search/") }, { QStringLiteral("title"), QStringLiteral("没找到 - 下厨房") } } } };
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy ok(&source, &RecipeSource::searchSucceeded);
        source.search(QStringLiteral("zzz"));
        QTRY_COMPARE(ok.size(), 1);
        QVERIFY(ok.first().at(1).value<RecipeSearchResult>().items.isEmpty());
    }

    void errorsStillDeleteTheSession()
    {
        FakeLexmountServer server;
        server.versionStatus = 500;
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy bad(&source, &RecipeSource::failed);
        source.detail(QStringLiteral("1"));
        QTRY_COMPARE(bad.size(), 1);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::Unavailable);
        QTRY_COMPARE(server.deletes(), 1);
    }

    void rejectedCredentialsAreAuthFailures()
    {
        FakeLexmountServer server;
        server.createStatus = 401;
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy bad(&source, &RecipeSource::failed);
        source.detail(QStringLiteral("1"));
        QTRY_COMPARE(bad.size(), 1);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::AuthFailed);
        QCOMPARE(server.deletes(), 0); // there was no session to delete
    }

    void timeoutDeletesTheSession()
    {
        FakeLexmountServer server;
        server.stallNavigate = true;
        auto config = fastConfig(server);
        config.stepTimeoutMs = 300;
        LexmountRecipeSource source(credentials(), config);
        QSignalSpy bad(&source, &RecipeSource::failed);
        source.detail(QStringLiteral("1"));
        QTRY_COMPARE_WITH_TIMEOUT(bad.size(), 1, 5000);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::Timeout);
        QTRY_COMPARE(server.deletes(), 1);
    }

    void cancelIsSilentAndDeletesTheSession()
    {
        FakeLexmountServer server;
        server.stallNavigate = true;
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy ok(&source, &RecipeSource::detailSucceeded);
        QSignalSpy bad(&source, &RecipeSource::failed);
        const quint64 id = source.detail(QStringLiteral("1"));
        QTRY_VERIFY(server.commands.contains(QStringLiteral("Page.navigate")));
        source.cancel(id);
        QTRY_COMPARE(server.deletes(), 1);
        QTest::qWait(200);
        QCOMPARE(ok.size(), 0);
        QCOMPARE(bad.size(), 0);
        QTRY_COMPARE(source.pendingCleanups(), 0);
    }

    void cancelBeforeAnySessionExistsLeavesNothingBehind()
    {
        FakeLexmountServer server;
        server.detailScrape = detailScrape();
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy ok(&source, &RecipeSource::detailSucceeded);
        const quint64 id = source.detail(QStringLiteral("1"));
        source.cancel(id);
        QTest::qWait(300);
        QCOMPARE(ok.size(), 0);
        QCOMPARE(server.deletes(), server.created); // a session created in flight is still deleted
    }

    void aCancelledRequestsLateRepliesDoNotLeakIntoTheNextOne()
    {
        FakeLexmountServer server;
        server.detailScrape = detailScrape();
        server.searchScrape = searchScrape();
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy detailOk(&source, &RecipeSource::detailSucceeded);
        QSignalSpy searchOk(&source, &RecipeSource::searchSucceeded);
        QSignalSpy bad(&source, &RecipeSource::failed);

        const quint64 first = source.detail(QStringLiteral("1"));
        QTRY_VERIFY(server.created >= 1);
        source.cancel(first);
        const quint64 second = source.search(QStringLiteral("虾仁"));
        QTRY_COMPARE(searchOk.size(), 1);
        QCOMPARE(searchOk.first().at(0).toULongLong(), second);
        QTest::qWait(300);
        QCOMPARE(detailOk.size(), 0);
        QCOMPARE(bad.size(), 0);
        QTRY_COMPARE(server.deletes(), 2);
    }

    void searchAndDetailRunIndependently()
    {
        FakeLexmountServer server;
        server.detailScrape = detailScrape();
        server.searchScrape = searchScrape();
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy detailOk(&source, &RecipeSource::detailSucceeded);
        QSignalSpy searchOk(&source, &RecipeSource::searchSucceeded);
        const quint64 a = source.search(QStringLiteral("虾仁"));
        const quint64 b = source.detail(QStringLiteral("42"));
        QTRY_COMPARE(searchOk.size(), 1);
        QTRY_COMPARE(detailOk.size(), 1);
        QCOMPARE(searchOk.first().at(0).toULongLong(), a);
        QCOMPARE(detailOk.first().at(0).toULongLong(), b);
        QCOMPARE(detailOk.first().at(1).value<WebRecipe>().id, QStringLiteral("42"));
        QTRY_COMPARE(server.deletes(), 2);
    }

    void missingCredentialsAndBadInputFailAsynchronously()
    {
        LexmountRecipeSource none([] { return LexmountCredentials(); });
        QVERIFY(!none.isConfigured());
        QSignalSpy bad(&none, &RecipeSource::failed);
        none.search(QStringLiteral("x"));
        QCOMPARE(bad.size(), 0);
        QTRY_COMPARE(bad.size(), 1);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::NotConfigured);

        FakeLexmountServer server;
        LexmountRecipeSource source(credentials(), fastConfig(server));
        QSignalSpy bad2(&source, &RecipeSource::failed);
        source.detail(QStringLiteral("abc"));
        source.search(QStringLiteral("  "));
        QTRY_COMPARE(bad2.size(), 2);
        QCOMPARE(bad2.at(0).at(1).value<RecipeError>().kind, RecipeErrorKind::InvalidQuery);
        QCOMPARE(server.created, 0);
    }

    void unreachableServerIsANetworkError()
    {
        LexmountRecipeSource::Config config;
        config.baseUrl = QUrl(QStringLiteral("http://127.0.0.1:1"));
        config.httpTimeoutMs = 2000;
        LexmountRecipeSource source(credentials(), config);
        QSignalSpy bad(&source, &RecipeSource::failed);
        source.detail(QStringLiteral("1"));
        QTRY_COMPARE_WITH_TIMEOUT(bad.size(), 1, 5000);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::NetworkError);
    }
};

QTEST_GUILESS_MAIN(LexmountRecipeSourceTest)
#include "lexmount-recipe-source-test.moc"
