#include "ai/deepseek-chat-client.h"

#include "ai/ai-log.h"

#include <QNetworkRequest>
#include <QTimer>

namespace whilefresh::ai {

ErrorKind kindForHttpStatus(int status)
{
    if (status == 401 || status == 403)
        return ErrorKind::AuthFailed;
    if (status == 402 || status == 429)
        return ErrorKind::QuotaOrRateLimit;
    if (status >= 500)
        return ErrorKind::ServerError;
    return ErrorKind::RequestRejected;
}

ErrorKind kindForNetworkError(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::SslHandshakeFailedError:
        return ErrorKind::SecureConnectionFailed;
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError: // what a transfer timeout reports on some Qt versions
        return ErrorKind::Timeout;
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::ProtocolFailure:
    case QNetworkReply::ProtocolInvalidOperationError:
    case QNetworkReply::ProtocolUnknownError:
        return ErrorKind::ServerError;
    default:
        return ErrorKind::NoNetwork;
    }
}

bool isRetryableFailure(QNetworkReply::NetworkError error, int status)
{
    if (status >= 500)
        return true;
    // The server hung up without (finishing) an answer. Protocol errors are not retried: a
    // second attempt would most likely be garbled the same way.
    return status < 400 && error == QNetworkReply::RemoteHostClosedError;
}

namespace {

constexpr int kMaxRetries = 1;
constexpr int kDetailSnippetChars = 200;

ChatError classifyFailure(QNetworkReply *reply, int status, const QByteArray &body)
{
    if (status >= 400) {
        // Error bodies from the API are short JSON messages that do not echo the request.
        const QString snippet = QString::fromUtf8(body.left(kDetailSnippetChars)).simplified();
        return { kindForHttpStatus(status), QStringLiteral("HTTP %1: %2").arg(status).arg(snippet), status };
    }
    return { kindForNetworkError(reply->error()),
             QStringLiteral("network error %1: %2").arg(int(reply->error())).arg(reply->errorString()), status };
}

}

QUrl DeepSeekChatClient::defaultEndpoint()
{
    return QUrl(QStringLiteral("https://api.deepseek.com/chat/completions"));
}

DeepSeekChatClient::DeepSeekChatClient(std::shared_ptr<const CredentialProvider> credentials, QObject *parent)
    : ChatClient(parent)
    , m_credentials(std::move(credentials))
    , m_endpoint(defaultEndpoint())
{
}

DeepSeekChatClient::~DeepSeekChatClient()
{
    // Aborting emits finished(); nothing may reach a half-destroyed client.
    for (Call &call : m_calls) {
        if (call.reply) {
            call.reply->disconnect(this);
            call.reply->abort();
        }
    }
}

bool DeepSeekChatClient::isConfigured() const
{
    return m_credentials && m_credentials->isConfigured(Service::DeepSeek);
}

quint64 DeepSeekChatClient::sendEncoded(const EncodedChatRequest &request)
{
    const quint64 id = nextRequestId();
    const QString key = m_credentials ? m_credentials->key(Service::DeepSeek) : QString();
    if (key.isEmpty()) {
        failLater(id, { ErrorKind::NotConfigured, QStringLiteral("no DeepSeek API key configured"), 0 });
        return id;
    }
    Call &call = m_calls[id];
    call.request = request;
    call.key = key;
    call.total.start();

    // One deadline for the whole call, retry included: the user is waiting for all of it.
    const int deadlineMs = request.deadlineMs.value_or(m_deadlineMs);
    auto *deadline = new QTimer(this);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, this, [this, id, deadlineMs] {
        const auto it = m_calls.find(id);
        if (it == m_calls.end())
            return;
        it->deadlineHit = true;
        if (it->reply) {
            it->reply->abort(); // replyFinished() reports the timeout
            return;
        }
        qCWarning(lcAi, "chat %llu failed: kind=timeout while waiting to retry", id);
        fail(id, { ErrorKind::Timeout, QStringLiteral("no complete answer within the %1 ms deadline").arg(deadlineMs), 0 });
    });
    deadline->start(deadlineMs);
    call.deadline = deadline;

    post(id);
    return id;
}

void DeepSeekChatClient::cancel(quint64 requestId)
{
    const auto it = m_calls.find(requestId);
    if (it == m_calls.end())
        return;
    QPointer<QNetworkReply> reply = it->reply;
    remove(requestId);
    if (reply)
        reply->abort(); // its finished handler finds no call and discards the reply
    qCInfo(lcAi, "chat %llu cancelled", requestId);
}

void DeepSeekChatClient::post(quint64 id)
{
    Call &call = m_calls[id];

    QNetworkRequest request(m_endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + call.key.toUtf8());
    request.setTransferTimeout(call.request.idleTimeoutMs.value_or(m_idleTimeoutMs));

    const ChatRequestSummary &summary = call.request.summary;
    qCInfo(lcAi, "chat %llu sending: model=%s messages=%d images=%d text=%d bytes=%lld json=%d max_tokens=%d attempt=%d",
           id, qUtf8Printable(summary.model), summary.messageCount, summary.imageCount, summary.textChars,
           qint64(call.request.body.size()), summary.jsonMode, summary.maxTokens, call.attempt + 1);

    call.attemptTimer.start();
    QNetworkReply *reply = m_network.post(request, call.request.body);
    call.reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, id, reply] { replyFinished(id, reply); });
}

void DeepSeekChatClient::remove(quint64 id)
{
    const auto it = m_calls.find(id);
    if (it == m_calls.end())
        return;
    if (it->deadline) {
        it->deadline->stop();
        it->deadline->deleteLater();
    }
    m_calls.erase(it);
}

void DeepSeekChatClient::replyFinished(quint64 id, QNetworkReply *reply)
{
    reply->deleteLater();
    const auto it = m_calls.find(id);
    if (it == m_calls.end())
        return;
    Call &call = *it;
    call.reply = nullptr;

    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const qint64 elapsed = call.attemptTimer.elapsed();

    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        const ChatError error = call.deadlineHit
            ? ChatError{ ErrorKind::Timeout,
                         QStringLiteral("no complete answer within the %1 ms deadline")
                             .arg(call.request.deadlineMs.value_or(m_deadlineMs)),
                         status }
            : classifyFailure(reply, status, body);
        qCWarning(lcAi, "chat %llu failed: kind=%s status=%d elapsed=%lldms attempt=%d", id,
                  qUtf8Printable(enumKey(error.kind)), status, elapsed, call.attempt + 1);
        if (!call.deadlineHit && isRetryableFailure(reply->error(), status) && call.attempt < kMaxRetries) {
            ++call.attempt;
            QTimer::singleShot(m_retryDelayMs, this, [this, id] {
                if (m_calls.contains(id))
                    post(id);
            });
            return;
        }
        fail(id, error);
        return;
    }

    ChatError parseError;
    std::optional<ChatResult> result = parseChatResponse(body, &parseError);
    if (!result) {
        qCWarning(lcAi, "chat %llu unparsable: status=%d bytes=%lld elapsed=%lldms", id, status,
                  qint64(body.size()), elapsed);
        fail(id, parseError);
        return;
    }
    result->elapsedMs = call.total.elapsed();
    qCInfo(lcAi, "chat %llu ok: status=%d bytes=%lld elapsed=%lldms finish=%s prompt=%d completion=%d total=%d", id,
           status, qint64(body.size()), elapsed, qUtf8Printable(result->finishReason), result->usage.promptTokens,
           result->usage.completionTokens, result->usage.totalTokens);
    remove(id);
    emit succeeded(id, *result);
}

void DeepSeekChatClient::fail(quint64 id, const ChatError &error)
{
    remove(id);
    emit failed(id, error);
}

void DeepSeekChatClient::failLater(quint64 id, const ChatError &error)
{
    m_calls.insert(id, {}); // so that cancel() can still silence it
    QTimer::singleShot(0, this, [this, id, error] {
        if (!m_calls.remove(id))
            return;
        qCWarning(lcAi, "chat %llu rejected: kind=%s", id, qUtf8Printable(enumKey(error.kind)));
        emit failed(id, error);
    });
}

}
