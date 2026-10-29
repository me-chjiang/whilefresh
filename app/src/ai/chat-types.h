#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

#include <optional>

namespace whilefresh::ai {

inline constexpr char kDefaultChatModel[] = "deepseek-flash";

enum class ErrorKind {
    NotConfigured,
    NoNetwork,
    Timeout,
    AuthFailed,
    QuotaOrRateLimit,
    ServerError,
    RequestRejected, // any other 4xx
    BadResponse,
    InvalidInput,
    SecureConnectionFailed, // TLS handshake or certificate trouble (often a wrong system clock)
    Internal, // a part of the app is broken (e.g. a missing image plugin), not the user or the network
};

QString enumKey(ErrorKind kind);
std::optional<ErrorKind> errorKindFromString(const QString &text);

struct ChatError {
    ErrorKind kind = ErrorKind::BadResponse;
    QString detail; // for logs only; never holds the key, prompts or image data
    int httpStatus = 0;
};

struct TokenUsage {
    int promptTokens = 0;
    int completionTokens = 0;
    int totalTokens = 0;
};

enum class ChatRole { System, User, Assistant };

struct ChatPart {
    enum class Type { Text, ImageUrl };

    static ChatPart text(const QString &text) { return { Type::Text, text }; }
    // A data: URL; encoding is the expensive part of large requests.
    static ChatPart jpegImage(const QByteArray &jpeg);

    Type type = Type::Text;
    QString value; // the text, or the image URL
};

struct ChatMessage {
    static ChatMessage system(const QString &text) { return { ChatRole::System, { ChatPart::text(text) } }; }
    static ChatMessage user(const QString &text) { return { ChatRole::User, { ChatPart::text(text) } }; }

    ChatRole role = ChatRole::User;
    // A single text part is sent as a plain string, anything else as an array of blocks.
    QList<ChatPart> parts;
};

// One chat-completions call. DeepSeek documents max_tokens as 1..393216; with thinking on
// (the deepseek-flash default) thinking tokens count against it.
struct ChatRequest {
    QString model = QString::fromLatin1(kDefaultChatModel);
    QList<ChatMessage> messages;
    bool jsonMode = false; // response_format json_object
    std::optional<int> maxTokens;
    std::optional<double> temperature;
    bool thinking = true; // false sends "thinking": {"type": "disabled"}

    QJsonObject toJson() const;
};

// What logs and recordings may say about a request.
struct ChatRequestSummary {
    QString model;
    int messageCount = 0;
    int imageCount = 0;
    int textChars = 0;
    bool jsonMode = false;
    int maxTokens = 0; // 0: not sent
};

// The wire form of a request, built once so large image requests can be encoded off the GUI
// thread.
struct EncodedChatRequest {
    QByteArray body;
    ChatRequestSummary summary;
};

EncodedChatRequest encodeChatRequest(const ChatRequest &request);

// Hex SHA-256 of an encoded body; recordings use it to tell whether a replayed request is the
// one that was recorded.
QByteArray requestDigest(const QByteArray &body);

struct ChatResult {
    QString content;
    QString finishReason;
    // finish_reason "length": the output budget ran out and `content` is what arrived before
    // the cut.
    bool truncated = false;
    TokenUsage usage;
    qint64 elapsedMs = 0;
    QByteArray rawBody; // as received; for parsers that need more than the content, and recordings
};

// Reads a 2xx chat-completions body. Fails with BadResponse when it is not JSON or has no
// message content.
std::optional<ChatResult> parseChatResponse(const QByteArray &body, ChatError *error);

}

Q_DECLARE_METATYPE(whilefresh::ai::ChatError)
Q_DECLARE_METATYPE(whilefresh::ai::ChatResult)
