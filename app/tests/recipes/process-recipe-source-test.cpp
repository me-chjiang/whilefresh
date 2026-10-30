#include "recipes/process-recipe-source.h"

#include <QSignalSpy>
#include <QTest>

using namespace whilefresh::recipes;

// The "program" is /bin/sh printing a recorded document, so no Python, network or credentials
// are involved; the query arguments land in $@ and are ignored.
class ProcessRecipeSourceTest : public QObject
{
    Q_OBJECT

private:
    static ProcessRecipeSource::Config cat(const char *sample, int timeoutMs = 10000)
    {
        return { QStringLiteral("/bin/sh"),
            { QStringLiteral("-c"), QStringLiteral("cat \"$1\""), QStringLiteral("sh"),
                QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR "/") + QLatin1String(sample) },
            timeoutMs };
    }

private slots:
    void searchOutputIsParsed()
    {
        ProcessRecipeSource source(cat("search-shrimp.yaml"));
        QSignalSpy ok(&source, &RecipeSource::searchSucceeded);
        source.search(QStringLiteral("虾仁"));
        QTRY_COMPARE_WITH_TIMEOUT(ok.size(), 1, 10000);
        QCOMPARE(ok.first().at(1).value<RecipeSearchResult>().items.size(), 14);
    }

    void errorDocumentWithFailingExitCode()
    {
        ProcessRecipeSource source({ QStringLiteral("/bin/sh"),
            { QStringLiteral("-c"), QStringLiteral("cat \"$1\"; exit 1"), QStringLiteral("sh"),
                QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR "/error-detail-blocked.yaml") } });
        QSignalSpy bad(&source, &RecipeSource::failed);
        source.detail(QStringLiteral("107722218"));
        QTRY_COMPARE_WITH_TIMEOUT(bad.size(), 1, 10000);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::Unavailable);
    }

    void credentialsAreRequestedPerCallAndPassedAsArguments()
    {
        int asked = 0;
        const QString script = QStringLiteral("case \"$*\" in *\"--api-key k1 --project-id p1\"*) cat ")
            + QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR "/detail-synthetic-full.yaml") + QStringLiteral(";; esac");
        ProcessRecipeSource source({ QStringLiteral("/bin/sh"), { QStringLiteral("-c"), script, QStringLiteral("sh") } },
            [&asked] {
                ++asked;
                return LexmountCredentials { QStringLiteral("k1"), QStringLiteral("p1") };
            });
        QSignalSpy ok(&source, &RecipeSource::detailSucceeded);
        source.detail(QStringLiteral("900000001"));
        source.detail(QStringLiteral("900000001"));
        QCOMPARE(asked, 2);
        QTRY_COMPARE_WITH_TIMEOUT(ok.size(), 2, 10000);
    }

    void missingProgram()
    {
        ProcessRecipeSource unconfigured({});
        QVERIFY(!unconfigured.isConfigured());
        QSignalSpy bad(&unconfigured, &RecipeSource::failed);
        unconfigured.search(QStringLiteral("x"));
        QTRY_COMPARE(bad.size(), 1);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::NotConfigured);

        ProcessRecipeSource missing({ QStringLiteral("/nonexistent/program"), {} });
        QSignalSpy bad2(&missing, &RecipeSource::failed);
        missing.search(QStringLiteral("x"));
        QTRY_COMPARE_WITH_TIMEOUT(bad2.size(), 1, 10000);
        QCOMPARE(bad2.first().at(1).value<RecipeError>().kind, RecipeErrorKind::NotConfigured);
    }

    void slowProgramTimesOut()
    {
        ProcessRecipeSource source({ QStringLiteral("/bin/sh"), { QStringLiteral("-c"), QStringLiteral("sleep 30"), QStringLiteral("sh") }, 200 });
        QSignalSpy bad(&source, &RecipeSource::failed);
        source.search(QStringLiteral("x"));
        QTRY_COMPARE_WITH_TIMEOUT(bad.size(), 1, 10000);
        QCOMPARE(bad.first().at(1).value<RecipeError>().kind, RecipeErrorKind::Timeout);
    }
};

QTEST_GUILESS_MAIN(ProcessRecipeSourceTest)
#include "process-recipe-source-test.moc"
