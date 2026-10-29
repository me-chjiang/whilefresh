#pragma once

#include "ai/chat-client.h"
#include "recognition/image-preprocessor.h"
#include "recognition/recognition-service.h"
#include "recognition/request-builder.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>

namespace whilefresh::recognition {

struct PreparedBatch {
    ai::EncodedChatRequest request;
    int imageCount = 0;
    BatchSpan span;
};

struct PrepareOutcome {
    QList<PreparedBatch> batches;
    qsizetype sliceCount = 0;
    std::optional<RecognitionError> error;
};

// Recognition through DeepSeek chat completions (ADR-4).
//
// Images are decoded, scaled, sliced, encoded and turned into request bodies on a worker
// thread, then sent through the chat client in as few requests as the limits allow (normally
// one per import), one after another. Timeouts, retries and the key are the client's business
// (see ai::DeepSeekChatClient); its errors keep their kind. Logs carry sizes, counts, timings,
// token usage and status codes only - never the key, image data or prompts.
class DeepSeekRecognizer : public RecognitionService
{
    Q_OBJECT

public:
    // `client` must outlive the recognizer; it may be shared with other AI services.
    explicit DeepSeekRecognizer(ai::ChatClient *client, QObject *parent = nullptr);
    ~DeepSeekRecognizer() override;

    quint64 recognize(const RecognitionRequest &request) override;
    void cancel(quint64 requestId) override;

    // Settings below exist for tests and tools; the defaults are what the app uses.
    void setBatchLimits(const BatchLimits &limits) { m_limits = limits; }
    void setRequestOptions(const RequestOptions &options) { m_options = options; }
    // Replaces the check for the JPEG image plugin; for tests.
    void setJpegProbe(std::function<bool()> probe) { m_jpegProbe = std::move(probe); }

private:
    struct Job {
        RecognitionRequest request;
        QList<PreparedBatch> batches;
        int batchIndex = 0;
        QList<RecognitionResult> parts;
        QElapsedTimer total;
        QElapsedTimer batchTimer;
        quint64 chatId = 0; // the request in flight, 0 while preparing
        QFutureWatcher<PrepareOutcome> *watcher = nullptr;
    };

    void preparationDone(quint64 id);
    void sendBatch(quint64 id);
    void chatSucceeded(quint64 chatId, const ai::ChatResult &chat);
    void chatFailed(quint64 chatId, const ai::ChatError &error);
    void succeed(quint64 id);
    void fail(quint64 id, const RecognitionError &error);
    void failLater(quint64 id, const RecognitionError &error);

    ai::ChatClient *m_client;
    QHash<quint64, Job> m_jobs;
    QHash<quint64, quint64> m_jobByChat;
    BatchLimits m_limits;
    RequestOptions m_options;
    std::function<bool()> m_jpegProbe = jpegEncodingAvailable;
};

}
