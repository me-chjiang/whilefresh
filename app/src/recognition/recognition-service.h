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
//
// Implementations that send several requests per import also emit batchSucceeded() for each
// batch as it arrives, before the final succeeded(); its batchSpans and rawResponses describe
// that batch alone. Those that answer in one piece (replay, demo) never emit it and ignore
// RecognitionRequest::firstBatch.
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
    void batchSucceeded(quint64 requestId, int batchIndex, int batchCount,
                        const whilefresh::recognition::RecognitionResult &part);

protected:
    quint64 nextRequestId() { return ++m_lastRequestId; }

private:
    quint64 m_lastRequestId = 0;
};

}
