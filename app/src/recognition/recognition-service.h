#pragma once

#include "recognition/recognition-types.h"

#include <QObject>

namespace whilefresh::recognition {

// Asynchronous recognition of purchase material. Implementations (DeepSeek, replay, demo)
// are interchangeable; callers only use this interface.
//
// recognize() returns a request id and never completes synchronously: exactly one of
// succeeded() / failed() is emitted later from the event loop with the same id, unless
// the request was cancelled (then neither is emitted).
class RecognitionService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual quint64 recognize(const RecognitionRequest &request) = 0;
    virtual void cancel(quint64 requestId) = 0;

signals:
    void succeeded(quint64 requestId, const whilefresh::recognition::RecognitionResult &result);
    void failed(quint64 requestId, const whilefresh::recognition::RecognitionError &error);

protected:
    quint64 nextRequestId() { return ++m_lastRequestId; }

private:
    quint64 m_lastRequestId = 0;
};

}
