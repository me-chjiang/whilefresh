#pragma once

#include "ai/chat-client.h"

#include <QHash>
#include <QObject>
#include <QStringList>

namespace whilefresh::ai {

inline constexpr int kSummaryMaxChars = 200;
inline constexpr int kSummarizerIdleTimeoutMs = 20000;
inline constexpr int kSummarizerDeadlineMs = 40000;

struct PreferenceSummary {
    QString text;
    TokenUsage usage;
    qint64 elapsedMs = 0;
};

// Condenses the user's remembered preferences into the short summary that is appended to the
// system prompt of cooking requests (PRD v6: 偏好记忆). One text-only JSON-mode call through the
// shared ChatClient, started only on request. The answer is a single line of at most
// kSummaryMaxChars characters, cut if the model ran over.
class PreferenceSummarizer : public QObject
{
    Q_OBJECT

public:
    // The client must outlive this object.
    explicit PreferenceSummarizer(ChatClient *client, QObject *parent = nullptr);

    bool isConfigured() const { return m_client->isConfigured(); }
    // Returns a request id; exactly one of summarized() / failed() follows unless cancelled.
    // `preferences` must not be empty.
    quint64 summarize(const QStringList &preferences);
    void cancel(quint64 requestId);

    static ChatRequest buildRequest(const QStringList &preferences);
    // Empty when the answer has no usable "summary" string.
    static QString parseAnswer(const QJsonObject &answer);

signals:
    void summarized(quint64 requestId, const whilefresh::ai::PreferenceSummary &summary);
    void failed(quint64 requestId, const whilefresh::ai::ChatError &error);

private:
    void chatSucceeded(quint64 id, const ChatResult &result);
    void chatFailed(quint64 id, const ChatError &error);

    ChatClient *m_client;
    QHash<quint64, bool> m_pending;
};

}

Q_DECLARE_METATYPE(whilefresh::ai::PreferenceSummary)
