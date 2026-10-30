// Desktop-only command-line front end for the real recognition service.
//
//   recognize-cli --vocab <vocabulary.json> [--kind screenshot|receipt] [--date YYYY-MM-DD]
//                 [--no-thinking] [--record <file>] <image>...
//   recognize-cli --vocab <vocabulary.json> [--record <file>] --text <order-text-file>
//   recognize-cli --vocab <vocabulary.json> --replay <recording> (same input as when recorded)
//   recognize-cli --vocab <vocabulary.json> --classify <names-file> [--record <file> | --replay <file>]
//
// --summarize <preferences-file> sends the preferences of a text file (one per line) to the
// preference summarizer instead (one text-only call; no --vocab needed) and prints the summary;
// it takes --record / --replay like --classify.
//
// --classify sends the product names of a text file (one per line) to the ingredient
// classifier instead (one text-only call) and prints each name with its standard name; its
// recordings are ai chat recordings (format 2).
//
// Calls the real DeepSeek API (billed) with the key compiled in by
// `scripts/build-desktop.sh --with-key`, prints a summary of the parsed result, and can save a
// recording. Recordings of real material belong in private/recordings/, never in the repository;
// --record refuses any other path inside the repository. --replay answers from a recording
// through the same request path instead, offline and without a key; requests that differ from
// the recorded ones fail when the recording holds their digests.

#include "ai/api-keys.h"
#include "ai/deepseek-chat-client.h"
#include "ai/ingredient-classifier.h"
#include "ai/preference-summarizer.h"
#include "ai/recorded-chat-client.h"
#include "recognition/deepseek-recognizer.h"
#include "recognition/recording.h"
#include "recognition/request-builder.h"
#include "core/catalog.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

using namespace whilefresh::recognition;

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

QString shelfLife(const ShelfLifeDays &days)
{
    const auto part = [](const std::optional<int> &value) {
        return value ? QString::number(*value) : QStringLiteral("-");
    };
    return QStringLiteral("%1/%2/%3").arg(part(days.fridge), part(days.freezer), part(days.room));
}

void printSummary(const RecognitionResult &result)
{
    int selected = 0, gifts = 0, irrelevant = 0, uncategorized = 0;
    for (const RecognizedItem &item : result.items) {
        selected += item.selected;
        gifts += item.isGift;
        irrelevant += item.category == ItemCategory::Irrelevant;
        uncategorized += item.canonicalName == uncategorizedName();
    }
    out() << "source: " << enumKey(result.sourceType) << "  elapsed: " << result.elapsedMs << " ms  tokens: "
          << result.usage.totalTokens << " (prompt " << result.usage.promptTokens << ", completion "
          << result.usage.completionTokens << ")  requests: " << result.rawResponses.size() << "\n";
    out() << "items: " << result.items.size() << "  selected: " << selected << "  gifts: " << gifts
          << "  irrelevant: " << irrelevant << "  uncategorized: " << uncategorized << "\n";
    if (result.truncated)
        out() << "WARNING: reply was cut off by the token limit; the list may be incomplete\n";
    out() << "ignored: " << result.ignoredSummary << "\n";
    for (const RecognizedItem &item : result.items) {
        out() << (item.selected ? "[x]" : "[ ]") << (item.isGift ? "[gift] " : " ") << item.canonicalName
              << " | " << item.rawName << " | " << item.quantityText << " x" << item.count << " | "
              << enumKey(item.category) << " | " << enumKey(item.storage) << " | shelf(f/z/r) "
              << shelfLife(item.shelfLifeDays);
        if (!item.productionDateText.isEmpty())
            out() << " | prod: " << item.productionDateText;
        if (!item.expiryDateText.isEmpty())
            out() << " | exp: " << item.expiryDateText;
        if (!item.purchaseDateText.isEmpty())
            out() << " | bought: " << item.purchaseDateText;
        out() << "\n";
    }
    out().flush();
}

int classifyNames(QCoreApplication &app, const QCommandLineParser &parser,
                  whilefresh::ai::RecordingChatClient &recorder,
                  std::shared_ptr<const whilefresh::core::IngredientCatalog> catalog, const QStringList &names)
{
    using whilefresh::ai::Classification;
    whilefresh::ai::IngredientClassifier classifier(&recorder, std::move(catalog));
    int exitCode = 1;
    QObject::connect(&classifier, &whilefresh::ai::IngredientClassifier::classified, &app,
                     [&](quint64, const Classification &result) {
                         out() << "elapsed: " << result.elapsedMs << " ms  tokens: " << result.usage.totalTokens
                               << " (prompt " << result.usage.promptTokens << ", completion "
                               << result.usage.completionTokens << ")  classified: " << result.canonical.size()
                               << "/" << names.size() << "  rejected: " << result.rejected << "\n";
                         for (const QString &name : names)
                             out() << name << " -> " << result.canonical.value(name, QStringLiteral("none")) << "\n";
                         exitCode = 0;
                         if (parser.isSet(QStringLiteral("record"))) {
                             QString recordError;
                             const QString path = parser.value(QStringLiteral("record"));
                             if (whilefresh::ai::writeChatRecording(path, recorder.recording(), &recordError))
                                 out() << "recorded: " << path << "\n";
                             else {
                                 err() << "error: " << recordError << "\n";
                                 exitCode = 1;
                             }
                         }
                         out().flush();
                         app.quit();
                     });
    QObject::connect(&classifier, &whilefresh::ai::IngredientClassifier::failed, &app,
                     [&](quint64, const whilefresh::ai::ChatError &failure) {
                         err() << "failed (" << whilefresh::ai::enumKey(failure.kind) << "): " << failure.detail << "\n";
                         app.quit();
                     });
    classifier.classify(names);
    app.exec();
    return exitCode;
}

int summarizePreferences(QCoreApplication &app, const QCommandLineParser &parser,
                         whilefresh::ai::RecordingChatClient &recorder, const QStringList &preferences)
{
    using whilefresh::ai::PreferenceSummary;
    whilefresh::ai::PreferenceSummarizer summarizer(&recorder);
    int exitCode = 1;
    QObject::connect(&summarizer, &whilefresh::ai::PreferenceSummarizer::summarized, &app,
                     [&](quint64, const PreferenceSummary &result) {
                         out() << "elapsed: " << result.elapsedMs << " ms  tokens: " << result.usage.totalTokens
                               << " (prompt " << result.usage.promptTokens << ", completion "
                               << result.usage.completionTokens << ")  chars: " << result.text.size() << "\n"
                               << result.text << "\n";
                         exitCode = 0;
                         if (parser.isSet(QStringLiteral("record"))) {
                             QString recordError;
                             const QString path = parser.value(QStringLiteral("record"));
                             if (whilefresh::ai::writeChatRecording(path, recorder.recording(), &recordError))
                                 out() << "recorded: " << path << "\n";
                             else {
                                 err() << "error: " << recordError << "\n";
                                 exitCode = 1;
                             }
                         }
                         out().flush();
                         app.quit();
                     });
    QObject::connect(&summarizer, &whilefresh::ai::PreferenceSummarizer::failed, &app,
                     [&](quint64, const whilefresh::ai::ChatError &failure) {
                         err() << "failed (" << whilefresh::ai::enumKey(failure.kind) << "): " << failure.detail << "\n";
                         app.quit();
                     });
    summarizer.summarize(preferences);
    app.exec();
    return exitCode;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Run one real DeepSeek recognition and print the result."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("image"), QStringLiteral("Image files, in order."), QStringLiteral("[image...]"));
    parser.addOption({ QStringLiteral("vocab"), QStringLiteral("Vocabulary JSON file (required)."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("text"), QStringLiteral("Order text file; selects order-text input."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("kind"), QStringLiteral("Image input kind: screenshot (default) or receipt."), QStringLiteral("kind") });
    parser.addOption({ QStringLiteral("date"), QStringLiteral("Capture date of the images, YYYY-MM-DD."), QStringLiteral("date") });
    parser.addOption({ QStringLiteral("no-thinking"), QStringLiteral("Send thinking disabled (default: thinking on, as validated).") });
    parser.addOption({ QStringLiteral("record"), QStringLiteral("Save the raw responses as a recording."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("replay"), QStringLiteral("Answer from a recording instead of the network."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("classify"), QStringLiteral("Classify the product names in a file (one per line)."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("summarize"), QStringLiteral("Summarize the preferences in a file (one per line)."), QStringLiteral("file") });
    parser.process(app);

    const bool summarize = parser.isSet(QStringLiteral("summarize"));
    if (!summarize && !parser.isSet(QStringLiteral("vocab"))) {
        err() << "error: --vocab is required\n";
        return 2;
    }
    QString error;
    std::shared_ptr<const whilefresh::core::IngredientCatalog> catalog;
    if (!summarize) {
        const auto vocabulary = whilefresh::core::IngredientCatalog::fromFile(parser.value(QStringLiteral("vocab")), &error);
        if (!vocabulary) {
            err() << "error: " << error << "\n";
            return 2;
        }
        catalog = std::make_shared<const whilefresh::core::IngredientCatalog>(*vocabulary);
    }
    QStringList preferencesToSummarize;
    QStringList namesToClassify;
    RecognitionRequest request;
    request.catalog = catalog;
    if (summarize) {
        QFile file(parser.value(QStringLiteral("summarize")));
        if (!file.open(QIODevice::ReadOnly)) {
            err() << "error: cannot read preferences file: " << file.errorString() << "\n";
            return 2;
        }
        preferencesToSummarize = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (preferencesToSummarize.isEmpty()) {
            err() << "error: no preferences to summarize\n";
            return 2;
        }
    } else if (parser.isSet(QStringLiteral("classify"))) {
        QFile file(parser.value(QStringLiteral("classify")));
        if (!file.open(QIODevice::ReadOnly)) {
            err() << "error: cannot read names file: " << file.errorString() << "\n";
            return 2;
        }
        namesToClassify = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        namesToClassify = whilefresh::ai::IngredientClassifier::namesToSend(namesToClassify);
        if (namesToClassify.isEmpty()) {
            err() << "error: no names to classify\n";
            return 2;
        }
    } else if (parser.isSet(QStringLiteral("text"))) {
        QFile file(parser.value(QStringLiteral("text")));
        if (!file.open(QIODevice::ReadOnly)) {
            err() << "error: cannot read text file: " << file.errorString() << "\n";
            return 2;
        }
        request.kind = InputKind::OrderText;
        request.text = QString::fromUtf8(file.readAll());
    } else {
        const QString kindName = parser.isSet(QStringLiteral("kind")) ? parser.value(QStringLiteral("kind"))
                                                                      : QStringLiteral("screenshot");
        const auto kind = inputKindFromString(kindName);
        if (!kind || *kind == InputKind::OrderText) {
            err() << "error: --kind must be screenshot or receipt\n";
            return 2;
        }
        request.kind = *kind;
        QDate date;
        if (parser.isSet(QStringLiteral("date"))) {
            date = QDate::fromString(parser.value(QStringLiteral("date")), Qt::ISODate);
            if (!date.isValid()) {
                err() << "error: --date must be YYYY-MM-DD\n";
                return 2;
            }
        }
        for (const QString &path : parser.positionalArguments())
            request.images.append({ path, date });
        if (request.images.isEmpty()) {
            err() << "error: give image files or --text\n";
            return 2;
        }
    }

    if (parser.isSet(QStringLiteral("record"))
        && !isRecordingPathAllowed(parser.value(QStringLiteral("record")),
                                   QString::fromLatin1(WHILEFRESH_REPO_ROOTS).split(QLatin1Char('|')))) {
        err() << "error: refusing to record inside the repository; use a path outside it or under private/ "
                 "(and its directory must exist)\n";
        return 2;
    }

    const bool replay = parser.isSet(QStringLiteral("replay"));
    if (replay && parser.isSet(QStringLiteral("record"))) {
        err() << "error: --record and --replay cannot be combined\n";
        return 2;
    }

    std::unique_ptr<whilefresh::ai::ChatClient> transport;
    if (replay) {
        QString loadError;
        auto recorded = whilefresh::ai::readChatRecording(parser.value(QStringLiteral("replay")), &loadError);
        if (!recorded) {
            err() << "error: " << loadError << "\n";
            return 2;
        }
        transport = std::make_unique<whilefresh::ai::RecordedChatClient>(*recorded);
    } else {
        transport = std::make_unique<whilefresh::ai::DeepSeekChatClient>(
            std::make_shared<whilefresh::ai::BuiltinCredentialProvider>());
        if (!transport->isConfigured()) {
            err() << "error: no API key compiled in; build with scripts/build-desktop.sh --with-key\n";
            return 2;
        }
    }
    whilefresh::ai::RecordingChatClient recorder(transport.get());

    if (summarize)
        return summarizePreferences(app, parser, recorder, preferencesToSummarize);
    if (!namesToClassify.isEmpty())
        return classifyNames(app, parser, recorder, catalog, namesToClassify);

    DeepSeekRecognizer recognizer(&recorder);
    RequestOptions options;
    options.thinking = !parser.isSet(QStringLiteral("no-thinking"));
    recognizer.setRequestOptions(options);
    int exitCode = 1;
    QObject::connect(&recognizer, &RecognitionService::succeeded, &app,
                     [&](quint64, const RecognitionResult &result) {
                         printSummary(result);
                         exitCode = 0;
                         if (parser.isSet(QStringLiteral("record"))) {
                             QString recordError;
                             const QString path = parser.value(QStringLiteral("record"));
                             Recording recording = makeRecording(request, result);
                             for (const auto &exchange : recorder.recording().exchanges)
                                 recording.requestDigests.append(exchange.requestDigest);
                             if (writeRecording(path, recording, &recordError))
                                 out() << "recorded: " << path << "\n";
                             else {
                                 err() << "error: " << recordError << "\n";
                                 exitCode = 1;
                             }
                             out().flush();
                         }
                         app.quit();
                     });
    QObject::connect(&recognizer, &RecognitionService::failed, &app,
                     [&](quint64, const RecognitionError &failure) {
                         err() << "failed (" << enumKey(failure.kind) << "): " << failure.userMessage << "\n"
                               << "detail: " << failure.detail << "\n";
                         app.quit();
                     });

    recognizer.recognize(request);
    app.exec();
    return exitCode;
}
