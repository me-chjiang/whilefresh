#include "recipes/recorded-recipe-source.h"

#include <QSignalSpy>
#include <QTest>

using namespace whilefresh::recipes;

class RecordedRecipeSourceTest : public QObject
{
    Q_OBJECT

private slots:
    void directoryIsLoaded()
    {
        RecordedRecipeSource source;
        QCOMPARE(source.addDirectory(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR)), 6);
        QVERIFY(!source.addFile(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR "/SOURCE.md")));
        QVERIFY(!source.addFile(QStringLiteral("/nonexistent.yaml")));
    }

    void searchAnswersAsynchronously()
    {
        RecordedRecipeSource source;
        source.addDirectory(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR));
        QSignalSpy ok(&source, &RecipeSource::searchSucceeded);
        QSignalSpy bad(&source, &RecipeSource::failed);

        const quint64 id = source.search(QStringLiteral("虾仁"));
        QCOMPARE(ok.size(), 0);
        QTRY_COMPARE(ok.size(), 1);
        QCOMPARE(bad.size(), 0);
        QCOMPARE(ok.first().at(0).toULongLong(), id);
        const auto result = ok.first().at(1).value<RecipeSearchResult>();
        QCOMPARE(result.items.size(), 14);
    }

    void detailAnswers()
    {
        RecordedRecipeSource source;
        source.addDirectory(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR));
        QSignalSpy ok(&source, &RecipeSource::detailSucceeded);

        source.detail(QStringLiteral("900000002"));
        QTRY_COMPARE(ok.size(), 1);
        const auto recipe = ok.first().at(1).value<WebRecipe>();
        QCOMPARE(recipe.id, QStringLiteral("900000002"));
        QVERIFY(recipe.partial);
    }

    void recordedErrorsAreTyped()
    {
        RecordedRecipeSource source;
        source.addDirectory(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR));
        QSignalSpy bad(&source, &RecipeSource::failed);

        source.detail(QStringLiteral("107722218"));
        source.detail(QStringLiteral("abc"));
        QTRY_COMPARE(bad.size(), 2);
        QCOMPARE(bad.at(0).at(1).value<RecipeError>().kind, RecipeErrorKind::Unavailable);
        QCOMPARE(bad.at(1).at(1).value<RecipeError>().kind, RecipeErrorKind::InvalidQuery);
    }

    void unrecordedRequestsAreNotFound()
    {
        RecordedRecipeSource source;
        source.addDirectory(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR));
        QSignalSpy bad(&source, &RecipeSource::failed);

        source.search(QStringLiteral("没录过"));
        source.search(QStringLiteral("虾仁"), 2);
        QTRY_COMPARE(bad.size(), 2);
        QCOMPARE(bad.at(0).at(1).value<RecipeError>().kind, RecipeErrorKind::NotFound);
        QCOMPARE(bad.at(1).at(1).value<RecipeError>().kind, RecipeErrorKind::NotFound);
    }

    void cancelledRequestsStaySilent()
    {
        RecordedRecipeSource source;
        source.addDirectory(QStringLiteral(WHILEFRESH_RECIPE_SAMPLES_DIR));
        QSignalSpy ok(&source, &RecipeSource::searchSucceeded);
        QSignalSpy bad(&source, &RecipeSource::failed);

        const quint64 id = source.search(QStringLiteral("虾仁"));
        source.cancel(id);
        const quint64 other = source.search(QStringLiteral("番茄炒蛋"));
        QTRY_COMPARE(ok.size(), 1);
        QCOMPARE(ok.first().at(0).toULongLong(), other);
        QCOMPARE(bad.size(), 0);
    }
};

QTEST_GUILESS_MAIN(RecordedRecipeSourceTest)
#include "recorded-recipe-source-test.moc"
