#include "recognition/recording.h"

#include "recognition/request-builder.h"
#include "recognition/response-parser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    if (result.batchSpans.size() == result.rawResponses.size())
        recording.batches = result.batchSpans;
    return recording;
}

bool isRecordingPathAllowed(const QString &path, const QStringList &repositoryRoots)
{
    // The file itself may not exist yet, so resolve its directory.
    const QFileInfo info(path);
    QDir directory = info.absoluteDir();
    if (!directory.exists())
        return false;
    const QString target = QDir::cleanPath(directory.canonicalPath() + QLatin1Char('/') + info.fileName());

    const auto isInside = [](const QString &candidate, const QString &root) {
        return candidate == root || candidate.startsWith(root + QLatin1Char('/'));
    };
    for (const QString &rootPath : repositoryRoots) {
        const QString root = QDir(rootPath).canonicalPath();
        if (root.isEmpty() || !isInside(target, root))
            continue;
        if (!isInside(target, root + QStringLiteral("/private")))
            return false;
    }
    return true;
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
    for (const QJsonValue &span : doc.object().value(QStringLiteral("batches")).toArray()) {
        recording.batches.append({ span.toObject().value(QStringLiteral("firstImage")).toInt(-1),
                                   span.toObject().value(QStringLiteral("lastImage")).toInt(-1) });
    }
    for (const QJsonValue &digest : doc.object().value(QStringLiteral("requestDigests")).toArray())
        recording.requestDigests.append(digest.toString().toLatin1());
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
    QJsonObject root{
        { QStringLiteral("format"), kFormat },
        { QStringLiteral("request"), summary },
        { QStringLiteral("responses"), responses },
    };
    if (recording.batches.size() == recording.responses.size() && !recording.batches.isEmpty()) {
        QJsonArray spans;
        for (const BatchSpan &span : recording.batches) {
            spans.append(QJsonObject{ { QStringLiteral("firstImage"), span.firstImage },
                                      { QStringLiteral("lastImage"), span.lastImage } });
        }
        root.insert(QStringLiteral("batches"), spans);
    }
    if (recording.requestDigests.size() == recording.responses.size() && !recording.requestDigests.isEmpty()) {
        QJsonArray digests;
        for (const QByteArray &digest : recording.requestDigests)
            digests.append(QString::fromLatin1(digest));
        root.insert(QStringLiteral("requestDigests"), digests);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, QStringLiteral("cannot write recording %1: %2").arg(path, file.errorString()));
        return false;
    }
    // Restrict before any content is written; an existing file is tightened as well.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
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
        if (recording.batches.size() == recording.responses.size())
            outcome.result->batchSpans = { recording.batches.at(parts.size()) };
        parts.append(*outcome.result);
    }
    return mergeResults(parts);
}

}
