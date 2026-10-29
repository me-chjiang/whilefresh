#include "ai/recorded-chat-client.h"

#include <QTimer>

namespace whilefresh::ai {

quint64 RecordedChatClient::sendEncoded(const EncodedChatRequest &request)
{
    const quint64 id = nextRequestId();
    const qsizetype index = m_requests.size();
    m_requests.append(request);
    m_pending.insert(id);
    QTimer::singleShot(0, this, [this, id, index] { answer(id, index); });
    return id;
}

void RecordedChatClient::answer(quint64 id, qsizetype index)
{
    if (!m_pending.remove(id))
        return;

    if (index >= m_recording.exchanges.size()) {
        emit failed(id, { ErrorKind::BadResponse,
                          QStringLiteral("the recording has no answer for request %1").arg(index + 1), 0 });
        return;
    }
    const ChatExchange &exchange = m_recording.exchanges.at(index);
    if (!exchange.requestDigest.isEmpty() && exchange.requestDigest != requestDigest(m_requests.at(index).body)) {
        emit failed(id, { ErrorKind::BadResponse,
                          QStringLiteral("request %1 differs from the recorded one; re-record").arg(index + 1), 0 });
        return;
    }
    if (exchange.error) {
        emit failed(id, *exchange.error);
        return;
    }
    ChatError error;
    std::optional<ChatResult> result = parseChatResponse(exchange.response, &error);
    if (!result) {
        emit failed(id, error);
        return;
    }
    result->elapsedMs = exchange.elapsedMs;
    emit succeeded(id, *result);
}

RecordingChatClient::RecordingChatClient(ChatClient *inner, QObject *parent)
    : ChatClient(parent), m_inner(inner)
{
    connect(m_inner, &ChatClient::succeeded, this, [this](quint64 id, const ChatResult &result) {
        Entry *open = entry(id);
        if (!open)
            return;
        open->exchange.response = result.rawBody;
        open->exchange.elapsedMs = result.elapsedMs;
        open->done = true;
        emit succeeded(id, result);
    });
    connect(m_inner, &ChatClient::failed, this, [this](quint64 id, const ChatError &error) {
        Entry *open = entry(id);
        if (!open)
            return;
        open->exchange.error = error;
        open->exchange.elapsedMs = open->timer.elapsed();
        open->done = true;
        emit failed(id, error);
    });
}

quint64 RecordingChatClient::sendEncoded(const EncodedChatRequest &request)
{
    // The inner client's ids are passed through: they are unique and never answered synchronously.
    Entry added;
    added.id = m_inner->sendEncoded(request);
    added.exchange.request = request.summary;
    added.exchange.requestDigest = requestDigest(request.body);
    added.timer.start();
    m_entries.append(added);
    return added.id;
}

void RecordingChatClient::cancel(quint64 requestId)
{
    m_entries.removeIf([requestId](const Entry &e) { return e.id == requestId; });
    m_inner->cancel(requestId);
}

ChatRecording RecordingChatClient::recording() const
{
    ChatRecording recording;
    for (const Entry &e : m_entries) {
        if (e.done)
            recording.exchanges.append(e.exchange);
    }
    return recording;
}

RecordingChatClient::Entry *RecordingChatClient::entry(quint64 id)
{
    for (Entry &e : m_entries) {
        if (e.id == id && !e.done)
            return &e;
    }
    return nullptr;
}

}
