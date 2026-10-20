#pragma once

#include "recognition/image-preprocessor.h"
#include "recognition/recognition-service.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

class QNetworkReply;
class QTimer;

namespace whilefresh::recognition {

struct PreparedBatch {
    QByteArray body;
    int imageCount = 0;
};

struct PrepareOutcome {
    QList<PreparedBatch> batches;
    qsizetype sliceCount = 0;
    std::optional<RecognitionError> error;
};

// Recognition through the DeepSeek chat-completions API (ADR-4).
//
// Images are decoded, scaled, sliced and encoded on a worker thread, then sent in as few
// requests as the limits allow (normally one per import). Each request has an idle timeout
// and an overall deadline. Only 5xx answers (and a connection the server drops) are retried,
// once; timeouts are not, because the server may still finish and bill the first attempt.
// 4xx answers are never retried. Logs carry sizes,
// counts, timings, token usage and status codes only - never the key, image data or prompts.
class DeepSeekRecognizer : public RecognitionService
{
    Q_OBJECT

public:
    static QUrl defaultEndpoint();

    // The key defaults to the one compiled in by the build script (ADR-5).
    explicit DeepSeekRecognizer(QObject *parent = nullptr);

    quint64 recognize(const RecognitionRequest &request) override;
    void cancel(quint64 requestId) override;

    // Settings below exist for tests and tools; the defaults are what the app uses.
    void setApiKey(const QString &key) { m_apiKey = key; }
    void setEndpoint(const QUrl &endpoint) { m_endpoint = endpoint; }
    void setIdleTimeoutMs(int ms) { m_idleTimeoutMs = ms; }
    void setDeadlineMs(int ms) { m_deadlineMs = ms; }
    void setRetryDelayMs(int ms) { m_retryDelayMs = ms; }
    void setBatchLimits(const BatchLimits &limits) { m_limits = limits; }

private:
    struct Job {
        RecognitionRequest request;
        QList<PreparedBatch> batches;
        int batchIndex = 0;
        int attempt = 0;
        QList<RecognitionResult> parts;
        QElapsedTimer total;
        QElapsedTimer batchTimer;
        QPointer<QNetworkReply> reply;
        QPointer<QTimer> deadline;
        bool deadlineHit = false;
        QFutureWatcher<PrepareOutcome> *watcher = nullptr;
    };

    void preparationDone(quint64 id);
    void sendBatch(quint64 id);
    void replyFinished(quint64 id, QNetworkReply *reply);
    void succeed(quint64 id);
    void fail(quint64 id, const RecognitionError &error);
    void stopDeadline(Job &job);
    void failLater(quint64 id, const RecognitionError &error);

    QNetworkAccessManager m_network;
    QHash<quint64, Job> m_jobs;
    QString m_apiKey;
    QUrl m_endpoint;
    int m_idleTimeoutMs = 150000; // no bytes in either direction; a non-streaming reply is silent while the model works
    int m_deadlineMs = 240000;    // per request, whatever the traffic
    int m_retryDelayMs = 1000;
    BatchLimits m_limits;
};

}
