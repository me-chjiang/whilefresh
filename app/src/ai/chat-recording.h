#pragma once

#include "ai/chat-types.h"

namespace whilefresh::ai {

// Recorded chat-completions calls (R-ADR-2), replayed in order by RecordedChatClient.
//
//   { "format": 2,
//     "exchanges": [ { "request": { "digest": "<hex sha-256 of the body>", "model": "deepseek-flash",
//                                   "messages": 2, "images": 0, "textChars": 812,
//                                   "jsonMode": true, "maxTokens": 4000 },
//                      "elapsedMs": 2310,
//                      "response": { <raw /chat/completions body> } },
//                    { "request": { ... },
//                      "error": { "kind": "server-error", "httpStatus": 503, "detail": "HTTP 503: ..." } } ] }
//
// Only sizes and a digest of the request are kept, never the prompt or images. Format 1 (the
// recognition recordings of recognize-cli, see recognition/recording.h) is read as well: each of
// its "responses" becomes an exchange, with the matching entry of "requestDigests" if present.
//
// Files are written readable by the owner only: answers may describe what the user bought or
// likes to eat.
struct ChatExchange {
    ChatRequestSummary request;
    QByteArray requestDigest; // empty: matches any request
    QByteArray response;      // raw body when the call succeeded
    std::optional<ChatError> error;
    qint64 elapsedMs = 0;
};

struct ChatRecording {
    QList<ChatExchange> exchanges;
};

std::optional<ChatRecording> readChatRecording(const QString &path, QString *error = nullptr);
bool writeChatRecording(const QString &path, const ChatRecording &recording, QString *error = nullptr);

}
