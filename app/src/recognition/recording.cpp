#include "recognition/recording.h"

#include "recognition/request-builder.h"
#include "recognition/response-parser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace whilefresh::recognition {

namespace {
constexpr int kFormat = 1;

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}
}

Recording makeRecording(const RecognitionRequest &request, const RecognitionResult &result)
{
    Recording recording;
    recording.inputKind = request.kind;
    recording.imageCount = int(request.images.size());
    recording.textLength = int(request.text.size());
    recording.promptVersion = promptVersion();
    recording.responses = result.rawResponses;
    return recording;
}

std::optional<Recording> readRecording(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot open recording %1: %2").arg(path, file.errorString()));
        return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || doc.object().value(QStringLiteral("format")).toInt() != kFormat) {
        setError(error, QStringLiteral("%1 is not a format-%2 recording").arg(path).arg(kFormat));
        return std::nullopt;
    }

    const QJsonObject summary = doc.object().value(QStringLiteral("request")).toObject();
    Recording recording;
    recording.inputKind = inputKindFromString(summary.value(QStringLiteral("inputKind")).toString())
                              .value_or(InputKind::Screenshot);
    recording.imageCount = summary.value(QStringLiteral("imageCount")).toInt();
    recording.textLength = summary.value(QStringLiteral("textLength")).toInt();
    recording.promptVersion = summary.value(QStringLiteral("promptVersion")).toString();
    for (const QJsonValue &response : doc.object().value(QStringLiteral("responses")).toArray())
        recording.responses.append(QJsonDocument(response.toObject()).toJson(QJsonDocument::Compact));
    if (recording.responses.isEmpty()) {
        setError(error, QStringLiteral("%1 has no responses").arg(path));
        return std::nullopt;
    }
    return recording;
}

bool writeRecording(const QString &path, const Recording &recording, QString *error)
{
    QJsonArray responses;
    for (const QByteArray &body : recording.responses)
        responses.append(QJsonDocument::fromJson(body).object());

    const QJsonObject summary{
        { QStringLiteral("inputKind"), enumKey(recording.inputKind) },
        { QStringLiteral("imageCount"), recording.imageCount },
        { QStringLiteral("textLength"), recording.textLength },
        { QStringLiteral("promptVersion"), recording.promptVersion },
    };
    const QJsonObject root{
        { QStringLiteral("format"), kFormat },
        { QStringLiteral("request"), summary },
        { QStringLiteral("responses"), responses },
    };

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, QStringLiteral("cannot write recording %1: %2").arg(path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

std::optional<RecognitionResult> resultFromRecording(const Recording &recording,
                                                     const RecognitionRequest &request,
                                                     RecognitionError *error)
{
    QList<RecognitionResult> parts;
    for (const QByteArray &body : recording.responses) {
        ParseOutcome outcome = parseChatCompletion(body, request.vocabulary.get(), recording.inputKind);
        if (!outcome.result) {
            if (error)
                *error = outcome.error;
            return std::nullopt;
        }
        parts.append(*outcome.result);
    }
    return mergeResults(parts);
}

}
