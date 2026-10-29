#pragma once

#include "ai/chat-client.h"
#include "ai/chat-recording.h"

#include <QElapsedTimer>
#include <QSet>

namespace whilefresh::ai {

// Answers requests from a recording, in order: the n-th request gets the n-th exchange (its
// response or its error). An exchange with a digest only answers the identical request body;
// anything else fails with BadResponse so stale recordings show up instead of passing
// silently. For tests and offline replay of every AI service.
class RecordedChatClient : public ChatClient
{
    Q_OBJECT

public:
    explicit RecordedChatClient(ChatRecording recording, QObject *parent = nullptr)
        : ChatClient(parent), m_recording(std::move(recording))
    {
    }

    quint64 sendEncoded(const EncodedChatRequest &request) override;
    void cancel(quint64 requestId) override { m_pending.remove(requestId); }
    bool isConfigured() const override { return true; }

    // Every request received so far, in order.
    const QList<EncodedChatRequest> &requests() const { return m_requests; }
    qsizetype remaining() const { return m_recording.exchanges.size() - m_requests.size(); }

private:
    void answer(quint64 id, qsizetype index);

    ChatRecording m_recording;
    QList<EncodedChatRequest> m_requests;
    QSet<quint64> m_pending;
};

// Passes requests on to another client and keeps what came back, in the order the requests
// were sent; cancelled requests are left out. The wrapped client must outlive this one.
class RecordingChatClient : public ChatClient
{
    Q_OBJECT

public:
    explicit RecordingChatClient(ChatClient *inner, QObject *parent = nullptr);

    quint64 sendEncoded(const EncodedChatRequest &request) override;
    void cancel(quint64 requestId) override;
    bool isConfigured() const override { return m_inner->isConfigured(); }

    // Completed exchanges only.
    ChatRecording recording() const;

private:
    struct Entry {
        quint64 id = 0;
        ChatExchange exchange;
        QElapsedTimer timer;
        bool done = false;
    };

    Entry *entry(quint64 id);

    ChatClient *m_inner;
    QList<Entry> m_entries;
};

}
