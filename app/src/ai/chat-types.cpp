#include "ai/chat-types.h"

#include "ai/ai-log.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>

namespace whilefresh::ai {

Q_LOGGING_CATEGORY(lcAi, "whilefresh.ai")

namespace {

struct ErrorKindName {
    ErrorKind kind;
    const char *text;
};

constexpr ErrorKindName kErrorKinds[] = {
    { ErrorKind::NotConfigured, "not-configured" },
    { ErrorKind::NoNetwork, "no-network" },
    { ErrorKind::Timeout, "timeout" },
    { ErrorKind::AuthFailed, "auth-failed" },
    { ErrorKind::QuotaOrRateLimit, "quota-or-rate-limit" },
    { ErrorKind::ServerError, "server-error" },
    { ErrorKind::RequestRejected, "request-rejected" },
    { ErrorKind::BadResponse, "bad-response" },
    { ErrorKind::InvalidInput, "invalid-input" },
    { ErrorKind::SecureConnectionFailed, "secure-connection-failed" },
    { ErrorKind::Internal, "internal" },
};

QString roleName(ChatRole role)
{
    switch (role) {
    case ChatRole::System:
        return QStringLiteral("system");
    case ChatRole::User:
        return QStringLiteral("user");
    case ChatRole::Assistant:
        return QStringLiteral("assistant");
    }
    return {};
}

QJsonObject partJson(const ChatPart &part)
{
    if (part.type == ChatPart::Type::ImageUrl) {
        return { { QStringLiteral("type"), QStringLiteral("image_url") },
                 { QStringLiteral("image_url"), QJsonObject{ { QStringLiteral("url"), part.value } } } };
    }
    return { { QStringLiteral("type"), QStringLiteral("text") }, { QStringLiteral("text"), part.value } };
}

QJsonValue contentJson(const QList<ChatPart> &parts)
{
    if (parts.size() == 1 && parts.first().type == ChatPart::Type::Text)
        return parts.first().value;
    QJsonArray blocks;
    for (const ChatPart &part : parts)
        blocks.append(partJson(part));
    return blocks;
}

}

QString enumKey(ErrorKind kind)
{
    for (const auto &entry : kErrorKinds) {
        if (entry.kind == kind)
            return QString::fromLatin1(entry.text);
    }
    return {};
}

std::optional<ErrorKind> errorKindFromString(const QString &text)
{
    for (const auto &entry : kErrorKinds) {
        if (text == QLatin1String(entry.text))
            return entry.kind;
    }
    return std::nullopt;
}

ChatPart ChatPart::jpegImage(const QByteArray &jpeg)
{
    return { Type::ImageUrl, QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(jpeg.toBase64()) };
}

QJsonObject ChatRequest::toJson() const
{
    QJsonArray wireMessages;
    for (const ChatMessage &message : messages) {
        wireMessages.append(QJsonObject{ { QStringLiteral("role"), roleName(message.role) },
                                         { QStringLiteral("content"), contentJson(message.parts) } });
    }
    QJsonObject body{ { QStringLiteral("model"), model }, { QStringLiteral("messages"), wireMessages } };
    if (jsonMode) {
        body.insert(QStringLiteral("response_format"),
                    QJsonObject{ { QStringLiteral("type"), QStringLiteral("json_object") } });
    }
    if (maxTokens)
        body.insert(QStringLiteral("max_tokens"), *maxTokens);
    if (temperature)
        body.insert(QStringLiteral("temperature"), *temperature);
    if (!thinking)
        body.insert(QStringLiteral("thinking"), QJsonObject{ { QStringLiteral("type"), QStringLiteral("disabled") } });
    return body;
}

EncodedChatRequest encodeChatRequest(const ChatRequest &request)
{
    EncodedChatRequest encoded;
    encoded.body = QJsonDocument(request.toJson()).toJson(QJsonDocument::Compact);
    ChatRequestSummary &summary = encoded.summary;
    summary.model = request.model;
    summary.messageCount = int(request.messages.size());
    summary.jsonMode = request.jsonMode;
    summary.maxTokens = request.maxTokens.value_or(0);
    encoded.idleTimeoutMs = request.idleTimeoutMs;
    encoded.deadlineMs = request.deadlineMs;
    for (const ChatMessage &message : request.messages) {
        for (const ChatPart &part : message.parts) {
            if (part.type == ChatPart::Type::ImageUrl)
                ++summary.imageCount;
            else
                summary.textChars += int(part.value.size());
        }
    }
    return encoded;
}

QByteArray requestDigest(const QByteArray &body)
{
    return QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex();
}

std::optional<ChatResult> parseChatResponse(const QByteArray &body, ChatError *error)
{
    const auto fail = [error](const QString &detail) {
        if (error)
            *error = { ErrorKind::BadResponse, detail, 0 };
        return std::nullopt;
    };

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (!doc.isObject())
        return fail(QStringLiteral("HTTP body is not a JSON object: %1").arg(parseError.errorString()));

    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    const QJsonObject choice = choices.isEmpty() ? QJsonObject() : choices.at(0).toObject();
    const QJsonValue content = choice.value(QStringLiteral("message")).toObject().value(QStringLiteral("content"));
    ChatResult result;
    result.finishReason = choice.value(QStringLiteral("finish_reason")).toString();
    if (!content.isString())
        return fail(QStringLiteral("response has no message content (finish_reason=%1)").arg(result.finishReason));

    result.content = content.toString();
    result.truncated = result.finishReason == QLatin1String("length");
    const QJsonObject usage = doc.object().value(QStringLiteral("usage")).toObject();
    result.usage.promptTokens = usage.value(QStringLiteral("prompt_tokens")).toInt();
    result.usage.completionTokens = usage.value(QStringLiteral("completion_tokens")).toInt();
    result.usage.totalTokens = usage.value(QStringLiteral("total_tokens")).toInt();
    result.rawBody = body;
    return result;
}

std::optional<QJsonObject> parseJsonContent(const ChatResult &result, ChatError *error)
{
    QString text = result.content.trimmed();
    if (text.startsWith(QLatin1String("```"))) {
        const qsizetype firstLine = text.indexOf(QLatin1Char('\n'));
        text = firstLine < 0 ? QString() : text.mid(firstLine + 1);
        if (text.trimmed().endsWith(QLatin1String("```")))
            text = text.trimmed().chopped(3);
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (doc.isObject())
        return doc.object();
    if (error) {
        const QString reason = doc.isNull() ? parseError.errorString() : QStringLiteral("not an object");
        *error = { ErrorKind::BadResponse,
                   QStringLiteral("model text is not a JSON object (%1, finish_reason=%2, %3 chars)")
                       .arg(reason, result.finishReason)
                       .arg(result.content.size()),
                   0 };
    }
    return std::nullopt;
}

}
