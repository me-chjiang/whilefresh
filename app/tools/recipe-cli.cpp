// Desktop-only command-line front end for the live Lexmount recipe source.
//
//   recipe-cli search <keyword> [--page N] [--record <file>]
//   recipe-cli detail <id> [--record <file>]
//
// Reads Xiachufang through a Lexmount cloud browser (billed per session). The credentials come
// from the environment, never from arguments: WHILEFRESH_LEXMOUNT_API_KEY and
// WHILEFRESH_LEXMOUNT_PROJECT_ID. --record writes the contract YAML the app's parser reads, the
// format of test-data/recipes/xiachufang/. Prints a summary only; credentials are never printed.
// Timings and the number of session deletions still pending at exit go to stderr.

#include "recipes/lexmount-recipe-source.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QTimer>

using namespace whilefresh::recipes;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addPositionalArgument(QStringLiteral("operation"), QStringLiteral("search or detail"));
    parser.addPositionalArgument(QStringLiteral("query"), QStringLiteral("keyword, or recipe id"));
    parser.addOption({ QStringLiteral("page"), QStringLiteral("Result page for search."), QStringLiteral("N"), QStringLiteral("1") });
    parser.addOption({ QStringLiteral("record"), QStringLiteral("Write the contract YAML to this file."), QStringLiteral("file") });
    parser.addHelpOption();
    parser.process(app);
    const QStringList args = parser.positionalArguments();
    if (args.size() != 2 || (args[0] != QLatin1String("search") && args[0] != QLatin1String("detail"))) {
        err() << "usage: recipe-cli search <keyword> | detail <id> [--page N] [--record <file>]\n";
        return 2;
    }

    LexmountRecipeSource source(&lexmountCredentialsFromEnvironment);
    QElapsedTimer timer;
    timer.start();
    int exitCode = 1;
    QByteArray yaml;

    QObject::connect(&source, &LexmountRecipeSource::documentReady, &app, [&](quint64, const QByteArray &document) { yaml = document; });
    // Wait for the browser session to be deleted before exiting.
    auto done = [&] {
        auto *poll = new QTimer(&app);
        QObject::connect(poll, &QTimer::timeout, &app, [&, poll] {
            if (source.pendingCleanups() == 0) {
                poll->stop();
                app.exit(exitCode);
            }
        });
        poll->start(100);
        QTimer::singleShot(15000, &app, [&] { app.exit(exitCode); });
    };
    QObject::connect(&source, &LexmountRecipeSource::cleanupFinished, &app,
        [](quint64, bool ok) { err() << "browser session " << (ok ? "deleted" : "NOT deleted") << "\n"; });
    QObject::connect(&source, &RecipeSource::searchSucceeded, &app, [&](quint64, const RecipeSearchResult &result) {
        out() << "search ok: " << result.items.size() << " results\n";
        for (const RecipeSummary &item : result.items) {
            out() << "  " << item.id << "  " << item.name << (item.imageUrl.isEmpty() ? "" : "  [image]") << "\n";
        }
        exitCode = 0;
        done();
    });
    QObject::connect(&source, &RecipeSource::detailSucceeded, &app, [&](quint64, const WebRecipe &recipe) {
        out() << "detail ok: " << recipe.name << "\n  " << recipe.ingredients.size() << " ingredients, " << recipe.steps.size()
              << " steps, " << recipe.tips.size() << " tips" << (recipe.partial ? ", partial" : "") << "\n  " << recipe.attribution.sourceUrl
              << "\n";
        exitCode = 0;
        done();
    });
    QObject::connect(&source, &RecipeSource::failed, &app, [&](quint64, const RecipeError &error) {
        err() << "failed: " << enumKey(error.kind) << ": " << error.detail << "\n";
        exitCode = 1;
        done();
    });
    if (args[0] == QLatin1String("search")) {
        source.search(args[1], parser.value(QStringLiteral("page")).toInt());
    } else {
        source.detail(args[1]);
    }
    const int result = app.exec();
    err() << "elapsed " << timer.elapsed() << " ms, deletions pending " << source.pendingCleanups() << "\n";
    if (result == 0 && parser.isSet(QStringLiteral("record"))) {
        QFile file(parser.value(QStringLiteral("record")));
        if (!file.open(QIODevice::WriteOnly) || file.write(yaml) != yaml.size()) {
            err() << "could not write the recording\n";
            return 1;
        }
        out() << "recorded " << file.fileName() << "\n";
    }
    return result;
}
