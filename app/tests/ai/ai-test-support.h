#pragma once

#include "ai/chat-types.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace QTest {
template<>
inline char *toString(const whilefresh::ai::ErrorKind &value)
{
    return toString(whilefresh::ai::enumKey(value));
}
}

namespace whilefresh::ai::testing {

// A /chat/completions body whose assistant message is `content`.
inline QByteArray chatCompletion(const QString &content, const QString &finishReason = QStringLiteral("stop"),
                                 int promptTokens = 100, int completionTokens = 50)
{
    const QJsonObject message{ { "role", "assistant" }, { "content", content } };
    const QJsonObject choice{ { "index", 0 }, { "message", message }, { "finish_reason", finishReason } };
    const QJsonObject usage{ { "prompt_tokens", promptTokens },
                             { "completion_tokens", completionTokens },
                             { "total_tokens", promptTokens + completionTokens } };
    return QJsonDocument(QJsonObject{ { "choices", QJsonArray{ choice } }, { "usage", usage } })
        .toJson(QJsonDocument::Compact);
}

inline ChatRequest userRequest(const QString &text)
{
    ChatRequest request;
    request.messages = { ChatMessage::system(QStringLiteral("只输出 JSON。")), ChatMessage::user(text) };
    request.jsonMode = true;
    request.maxTokens = 4000;
    return request;
}

}
