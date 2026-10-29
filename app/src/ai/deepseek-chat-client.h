#pragma once

#include "ai/chat-client.h"
#include "ai/api-keys.h"

#include <QElapsedTimer>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>

#include <memory>

class QTimer;

namespace whilefresh::ai {

// How HTTP statuses and Qt network errors are classified (exposed for tests).
ErrorKind kindForHttpStatus(int status);
ErrorKind kindForNetworkError(QNetworkReply::NetworkError error);

// The DeepSeek chat-completions API (ADR-4, R-ADR-1).
//
// Each request has an idle timeout and an overall deadline. Only 5xx answers (and a connection
// the server drops) are retried, once, after a short delay; timeouts are not, because the
// server may still finish and bill the first attempt. 4xx answers are never retried. A reply cut
// off by the token limit is a success flagged `truncated`. The key is read from the credential
// provider when a request is sent; without one the request fails with NotConfigured and
// nothing goes out. Logs carry sizes, counts, timings, token usage and status codes only.
class DeepSeekChatClient : public ChatClient
{
    Q_OBJECT

public:
    static QUrl defaultEndpoint();

    explicit DeepSeekChatClient(std::shared_ptr<const CredentialProvider> credentials, QObject *parent = nullptr);
    ~DeepSeekChatClient() override;

    quint64 sendEncoded(const EncodedChatRequest &request) override;
    void cancel(quint64 requestId) override;
    bool isConfigured() const override;

    // Requests already under way keep the key they started with.
    void setCredentials(std::shared_ptr<const CredentialProvider> credentials) { m_credentials = std::move(credentials); }

    // Settings below exist for tests and tools; the defaults are what the app uses.
    void setEndpoint(const QUrl &endpoint) { m_endpoint = endpoint; }
    void setIdleTimeoutMs(int ms) { m_idleTimeoutMs = ms; }
    void setDeadlineMs(int ms) { m_deadlineMs = ms; }
    void setRetryDelayMs(int ms) { m_retryDelayMs = ms; }

private:
    struct Call {
        EncodedChatRequest request;
        QString key;
        int attempt = 0;
        QElapsedTimer total;
        QElapsedTimer attemptTimer;
        QPointer<QNetworkReply> reply;
        QPointer<QTimer> deadline;
        bool deadlineHit = false;
    };

    void post(quint64 id);
    void replyFinished(quint64 id, QNetworkReply *reply);
    void stopDeadline(Call &call);
    void fail(quint64 id, const ChatError &error);
    void failLater(quint64 id, const ChatError &error);

    QNetworkAccessManager m_network;
    QHash<quint64, Call> m_calls;
    std::shared_ptr<const CredentialProvider> m_credentials;
    QUrl m_endpoint;
    int m_idleTimeoutMs = 150000; // no bytes in either direction; a non-streaming reply is silent while the model works
    int m_deadlineMs = 240000;    // per request, whatever the traffic
    int m_retryDelayMs = 1000;
};

}
