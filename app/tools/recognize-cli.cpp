// Desktop-only command-line front end for the real recognition service.
//
//   recognize-cli --vocab <vocabulary.json> [--kind screenshot|receipt] [--date YYYY-MM-DD]
//                 [--no-thinking] [--record <file>] <image>...
//   recognize-cli --vocab <vocabulary.json> [--record <file>] --text <order-text-file>
//
// Calls the real DeepSeek API (billed) with the key compiled in by
// `scripts/build-desktop.sh --with-key`, prints a summary of the parsed result, and can save a
// recording. Recordings of real material belong in private/recordings/, never in the repository.

#include "recognition/api-key.h"
#include "recognition/deepseek-recognizer.h"
#include "recognition/recording.h"
#include "recognition/request-builder.h"
#include "recognition/vocabulary.h"

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
    parser.process(app);

    if (!parser.isSet(QStringLiteral("vocab"))) {
        err() << "error: --vocab is required\n";
        return 2;
    }
    QString error;
    const auto vocabulary = Vocabulary::fromFile(parser.value(QStringLiteral("vocab")), &error);
    if (!vocabulary) {
        err() << "error: " << error << "\n";
        return 2;
    }

    RecognitionRequest request;
    request.vocabulary = std::make_shared<const Vocabulary>(*vocabulary);
    if (parser.isSet(QStringLiteral("text"))) {
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

    if (!isDeepSeekApiKeyConfigured()) {
        err() << "error: no API key compiled in; build with scripts/build-desktop.sh --with-key\n";
        return 2;
    }

    DeepSeekRecognizer recognizer;
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
                             if (writeRecording(path, makeRecording(request, result), &recordError))
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
