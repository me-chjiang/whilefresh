#include "recognition/deepseek-recognizer.h"

#include "recognition/api-key.h"
#include "recognition/recognition-log.h"
#include "recognition/request-builder.h"
#include "recognition/response-parser.h"

#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPromise>
#include <QThreadPool>
#include <QTimer>

namespace whilefresh::recognition {

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

namespace {

constexpr int kMaxRetries = 1;
constexpr int kDetailSnippetChars = 200;

RecognitionError classifyFailure(QNetworkReply *reply, int status, const QByteArray &body)
{
    if (status >= 400) {
        // Error bodies from the API are short JSON messages that do not echo the request.
        const QString snippet = QString::fromUtf8(body.left(kDetailSnippetChars)).simplified();
        return makeError(kindForHttpStatus(status),
                         QStringLiteral("HTTP %1: %2").arg(status).arg(snippet), status);
    }
    return makeError(kindForNetworkError(reply->error()),
                     QStringLiteral("network error %1: %2").arg(int(reply->error())).arg(reply->errorString()),
                     status);
}

// Runs on a worker thread: decoding and encoding tall screenshots takes noticeable time.
PrepareOutcome prepareJob(const RecognitionRequest &request, const BatchLimits &limits,
                          const RequestOptions &options, const std::function<bool()> &isCanceled)
{
    PrepareOutcome outcome;
    const auto addBatch = [&](const QList<PreparedImage> &images, const QString &text) {
        const QJsonObject body = buildChatRequest(request.kind, *request.vocabulary, images, text, options);
        BatchSpan span;
        for (const PreparedImage &image : images) {
            if (span.firstImage < 0)
                span.firstImage = image.imageIndex;
            span.lastImage = image.imageIndex;
        }
        outcome.batches.append({ QJsonDocument(body).toJson(QJsonDocument::Compact), int(images.size()), span });
    };

    if (request.kind == InputKind::OrderText) {
        addBatch({}, request.text);
        return outcome;
    }

    QList<PreparedImage> prepared;
    QString detail;
    if (!prepareImages(request.images, &prepared, &detail, isCanceled)) {
        if (detail.isEmpty())
            return outcome; // canceled: nobody is waiting for the answer
        RecognitionError error = makeError(ErrorKind::InvalidInput, detail);
        error.userMessage = QStringLiteral("有图片无法读取，请重新选择图片。");
        outcome.error = error;
        return outcome;
    }
    outcome.sliceCount = prepared.size();
    for (const QList<PreparedImage> &batch : planBatches(prepared, limits))
        addBatch(batch, {});
    return outcome;
}

} // namespace

QUrl DeepSeekRecognizer::defaultEndpoint()
{
    return QUrl(QStringLiteral("https://api.deepseek.com/chat/completions"));
}

DeepSeekRecognizer::DeepSeekRecognizer(QObject *parent)
    : RecognitionService(parent)
    , m_apiKey(deepSeekApiKey())
    , m_endpoint(defaultEndpoint())
{
}

DeepSeekRecognizer::~DeepSeekRecognizer()
{
    for (const Job &job : std::as_const(m_jobs)) {
        if (job.watcher)
            job.watcher->cancel();
    }
}

quint64 DeepSeekRecognizer::recognize(const RecognitionRequest &request)
{
    const quint64 id = nextRequestId();

    if (m_apiKey.isEmpty()) {
        failLater(id, makeError(ErrorKind::NotConfigured, QStringLiteral("no API key compiled in")));
        return id;
    }
    if (!request.vocabulary || request.vocabulary->isEmpty()) {
        failLater(id, makeError(ErrorKind::InvalidInput, QStringLiteral("request has no vocabulary")));
        return id;
    }
    if (request.kind == InputKind::OrderText) {
        const qsizetype length = request.text.trimmed().size();
        if (length == 0) {
            failLater(id, makeError(ErrorKind::InvalidInput, QStringLiteral("empty order text")));
            return id;
        }
        if (length > kMaxOrderTextChars) {
            RecognitionError error = makeError(ErrorKind::InvalidInput,
                                               QStringLiteral("order text has %1 chars").arg(length));
            error.userMessage = QStringLiteral("文字太长，请分几次粘贴。");
            failLater(id, error);
            return id;
        }
    } else if (request.images.isEmpty()) {
        failLater(id, makeError(ErrorKind::InvalidInput, QStringLiteral("no images")));
        return id;
    }

    if (request.kind != InputKind::OrderText && !m_jpegProbe()) {
        failLater(id, makeError(ErrorKind::Internal,
                                QStringLiteral("JPEG image plugin (qjpeg) is not available; writer formats: %1")
                                    .arg(QString::fromLatin1(QImageWriter::supportedImageFormats().join(',')))));
        return id;
    }

    Job &job = m_jobs[id];
    job.request = request;
    job.total.start();

    auto promise = std::make_shared<QPromise<PrepareOutcome>>();
    job.watcher = new QFutureWatcher<PrepareOutcome>(this);
    connect(job.watcher, &QFutureWatcherBase::finished, this, [this, id] { preparationDone(id); });
    promise->start();
    job.watcher->setFuture(promise->future());

    QThreadPool::globalInstance()->start([promise, request, limits = m_limits, options = m_options] {
        promise->addResult(prepareJob(request, limits, options, [&promise] { return promise->isCanceled(); }));
        promise->finish();
    });
    return id;
}

void DeepSeekRecognizer::cancel(quint64 requestId)
{
    const auto it = m_jobs.find(requestId);
    if (it == m_jobs.end())
        return;
    QPointer<QNetworkReply> reply = it->reply;
    QFutureWatcher<PrepareOutcome> *watcher = it->watcher;
    stopDeadline(*it);
    m_jobs.erase(it);
    if (watcher) {
        watcher->cancel(); // lets the worker stop between images
        watcher->deleteLater();
    }
    if (reply)
        reply->abort(); // its finished handler finds no job and discards the reply
    qCInfo(lcRecognition, "request %llu cancelled", requestId);
}

void DeepSeekRecognizer::preparationDone(quint64 id)
{
    const auto it = m_jobs.find(id);
    if (it == m_jobs.end())
        return;
    Job &job = *it;
    const PrepareOutcome outcome = job.watcher->result();
    job.watcher->deleteLater();
    job.watcher = nullptr;

    if (outcome.error) {
        fail(id, *outcome.error);
        return;
    }
    job.batches = outcome.batches;

    qint64 bytes = 0;
    for (const PreparedBatch &batch : std::as_const(job.batches))
        bytes += batch.body.size();
    qCInfo(lcRecognition, "request %llu prepared: kind=%s images=%d slices=%lld batches=%lld bytes=%lld prepare=%lldms",
           id, qUtf8Printable(enumKey(job.request.kind)), int(job.request.images.size()),
           qint64(outcome.sliceCount), qint64(job.batches.size()), bytes, job.total.elapsed());

    sendBatch(id);
}

void DeepSeekRecognizer::sendBatch(quint64 id)
{
    Job &job = m_jobs[id];
    const PreparedBatch &batch = job.batches.at(job.batchIndex);

    QNetworkRequest request(m_endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    request.setTransferTimeout(m_idleTimeoutMs);

    job.batchTimer.start();
    job.deadlineHit = false;
    QNetworkReply *reply = m_network.post(request, batch.body);
    job.reply = reply;

    auto *deadline = new QTimer(this);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, this, [this, id] {
        const auto it = m_jobs.find(id);
        if (it == m_jobs.end() || !it->reply)
            return;
        it->deadlineHit = true;
        it->reply->abort();
    });
    deadline->start(m_deadlineMs);
    job.deadline = deadline;
    connect(reply, &QNetworkReply::finished, this, [this, id, reply] { replyFinished(id, reply); });
}

void DeepSeekRecognizer::stopDeadline(Job &job)
{
    if (job.deadline) {
        job.deadline->stop();
        job.deadline->deleteLater();
        job.deadline = nullptr;
    }
}

void DeepSeekRecognizer::replyFinished(quint64 id, QNetworkReply *reply)
{
    reply->deleteLater();
    const auto it = m_jobs.find(id);
    if (it == m_jobs.end())
        return;
    Job &job = *it;
    job.reply = nullptr;
    stopDeadline(job);

    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const qint64 elapsed = job.batchTimer.elapsed();
    const int batchNumber = job.batchIndex + 1;
    const qsizetype batchTotal = job.batches.size();

    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        const RecognitionError error = job.deadlineHit
            ? makeError(ErrorKind::Timeout, QStringLiteral("no complete answer within the %1 ms deadline").arg(m_deadlineMs))
            : classifyFailure(reply, status, body);
        qCWarning(lcRecognition, "request %llu batch %d/%lld failed: kind=%s status=%d elapsed=%lldms attempt=%d",
                  id, batchNumber, qint64(batchTotal), qUtf8Printable(enumKey(error.kind)), status, elapsed,
                  job.attempt + 1);
        if (error.kind == ErrorKind::ServerError && job.attempt < kMaxRetries) {
            ++job.attempt;
            QTimer::singleShot(m_retryDelayMs, this, [this, id] {
                if (m_jobs.contains(id))
                    sendBatch(id);
            });
            return;
        }
        fail(id, error);
        return;
    }

    ParseOutcome outcome = parseChatCompletion(body, job.request.vocabulary.get(), job.request.kind);
    if (!outcome.result) {
        qCWarning(lcRecognition, "request %llu batch %d/%lld unparsable: status=%d bytes=%lld elapsed=%lldms: %s",
                  id, batchNumber, qint64(batchTotal), status, qint64(body.size()), elapsed,
                  qUtf8Printable(outcome.error.detail));
        fail(id, outcome.error);
        return;
    }
    qCInfo(lcRecognition, "request %llu batch %d/%lld ok: status=%d bytes=%lld elapsed=%lldms items=%lld tokens=%d",
           id, batchNumber, qint64(batchTotal), status, qint64(body.size()), elapsed,
           qint64(outcome.result->items.size()), outcome.result->usage.totalTokens);

    outcome.result->batchSpans = { job.batches.at(job.batchIndex).span };
    job.parts.append(*outcome.result);
    job.attempt = 0;
    if (++job.batchIndex < job.batches.size())
        sendBatch(id);
    else
        succeed(id);
}

void DeepSeekRecognizer::succeed(quint64 id)
{
    Job job = m_jobs.take(id);
    RecognitionResult result = mergeResults(job.parts);
    result.elapsedMs = job.total.elapsed();
    qCInfo(lcRecognition, "request %llu done: items=%lld prompt=%d completion=%d total=%d elapsed=%lldms", id,
           qint64(result.items.size()), result.usage.promptTokens, result.usage.completionTokens,
           result.usage.totalTokens, result.elapsedMs);
    emit succeeded(id, result);
}

void DeepSeekRecognizer::fail(quint64 id, const RecognitionError &error)
{
    m_jobs.remove(id);
    qCWarning(lcRecognition, "request %llu failed: kind=%s", id, qUtf8Printable(enumKey(error.kind)));
    emit failed(id, error);
}

void DeepSeekRecognizer::failLater(quint64 id, const RecognitionError &error)
{
    QTimer::singleShot(0, this, [this, id, error] {
        qCWarning(lcRecognition, "request %llu rejected: kind=%s", id, qUtf8Printable(enumKey(error.kind)));
        emit failed(id, error);
    });
}

}
