#pragma once

#include "ai/chat-types.h"

#include <QObject>

namespace whilefresh::ai {

// One asynchronous chat-completions call per request. Implementations (DeepSeek, recorded
// replay, recording decorator) are interchangeable; every AI service talks to this interface.
//
// send() returns a request id and never completes synchronously: exactly one of succeeded() /
// failed() is emitted later from the event loop with the same id, unless the request was
// cancelled (then neither is emitted). Several services may share one client; each picks out
// its own ids.
class ChatClient : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    quint64 send(const ChatRequest &request) { return sendEncoded(encodeChatRequest(request)); }
    virtual quint64 sendEncoded(const EncodedChatRequest &request) = 0;
    virtual void cancel(quint64 requestId) = 0;
    // False when a request could not even be attempted (no key); send() would fail with
    // NotConfigured.
    virtual bool isConfigured() const = 0;

signals:
    void succeeded(quint64 requestId, const whilefresh::ai::ChatResult &result);
    void failed(quint64 requestId, const whilefresh::ai::ChatError &error);

protected:
    quint64 nextRequestId() { return ++m_lastRequestId; }

private:
    quint64 m_lastRequestId = 0;
};

}
