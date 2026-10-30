#include "ai/chat-recording.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace whilefresh::ai {

namespace {

constexpr int kRecognitionFormat = 1;
constexpr int kChatFormat = 2;

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QByteArray compact(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

ChatExchange exchangeFromJson(const QJsonObject &json)
{
    ChatExchange exchange;
    const QJsonObject request = json.value(QStringLiteral("request")).toObject();
    exchange.requestDigest = request.value(QStringLiteral("digest")).toString().toLatin1();
    exchange.request.model = request.value(QStringLiteral("model")).toString();
    exchange.request.messageCount = request.value(QStringLiteral("messages")).toInt();
    exchange.request.imageCount = request.value(QStringLiteral("images")).toInt();
    exchange.request.textChars = request.value(QStringLiteral("textChars")).toInt();
    exchange.request.jsonMode = request.value(QStringLiteral("jsonMode")).toBool();
    exchange.request.maxTokens = request.value(QStringLiteral("maxTokens")).toInt();
    exchange.elapsedMs = json.value(QStringLiteral("elapsedMs")).toInteger();
    if (json.contains(QStringLiteral("error"))) {
        const QJsonObject error = json.value(QStringLiteral("error")).toObject();
        exchange.error = ChatError{
            errorKindFromString(error.value(QStringLiteral("kind")).toString()).value_or(ErrorKind::BadResponse),
            error.value(QStringLiteral("detail")).toString(), error.value(QStringLiteral("httpStatus")).toInt() };
    } else {
        exchange.response = compact(json.value(QStringLiteral("response")).toObject());
    }
    return exchange;
}

QJsonObject exchangeToJson(const ChatExchange &exchange)
{
    const ChatRequestSummary &summary = exchange.request;
    QJsonObject json{
        { QStringLiteral("request"),
          QJsonObject{ { QStringLiteral("digest"), QString::fromLatin1(exchange.requestDigest) },
                       { QStringLiteral("model"), summary.model },
                       { QStringLiteral("messages"), summary.messageCount },
                       { QStringLiteral("images"), summary.imageCount },
                       { QStringLiteral("textChars"), summary.textChars },
                       { QStringLiteral("jsonMode"), summary.jsonMode },
                       { QStringLiteral("maxTokens"), summary.maxTokens } } },
        { QStringLiteral("elapsedMs"), exchange.elapsedMs },
    };
    if (exchange.error) {
        json.insert(QStringLiteral("error"),
                    QJsonObject{ { QStringLiteral("kind"), enumKey(exchange.error->kind) },
                                 { QStringLiteral("httpStatus"), exchange.error->httpStatus },
                                 { QStringLiteral("detail"), exchange.error->detail } });
    } else {
        json.insert(QStringLiteral("response"), QJsonDocument::fromJson(exchange.response).object());
    }
    return json;
}

}

std::optional<ChatRecording> readChatRecording(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot open recording %1: %2").arg(path, file.errorString()));
        return std::nullopt;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const int format = root.value(QStringLiteral("format")).toInt();

    ChatRecording recording;
    if (format == kRecognitionFormat) {
        const QJsonArray responses = root.value(QStringLiteral("responses")).toArray();
        const QJsonArray digests = root.value(QStringLiteral("requestDigests")).toArray();
        for (qsizetype i = 0; i < responses.size(); ++i) {
            ChatExchange exchange;
            exchange.response = compact(responses.at(i).toObject());
            if (digests.size() == responses.size())
                exchange.requestDigest = digests.at(i).toString().toLatin1();
            recording.exchanges.append(exchange);
        }
    } else if (format == kChatFormat) {
        for (const QJsonValue &exchange : root.value(QStringLiteral("exchanges")).toArray())
            recording.exchanges.append(exchangeFromJson(exchange.toObject()));
    } else {
        setError(error, QStringLiteral("%1 is not a format-1 or format-2 recording").arg(path));
        return std::nullopt;
    }
    if (recording.exchanges.isEmpty()) {
        setError(error, QStringLiteral("%1 has no recorded answers").arg(path));
        return std::nullopt;
    }
    return recording;
}

bool writeChatRecording(const QString &path, const ChatRecording &recording, QString *error)
{
    QJsonArray exchanges;
    for (const ChatExchange &exchange : recording.exchanges)
        exchanges.append(exchangeToJson(exchange));
    const QJsonObject root{ { QStringLiteral("format"), kChatFormat }, { QStringLiteral("exchanges"), exchanges } };

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

}
