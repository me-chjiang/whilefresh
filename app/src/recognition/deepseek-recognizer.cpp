#include "recognition/deepseek-recognizer.h"

#include "recognition/recognition-log.h"
#include "recognition/response-parser.h"

#include <QImageWriter>
#include <QPromise>
#include <QThreadPool>
#include <QTimer>

namespace whilefresh::recognition {

namespace {

// Runs on a worker thread: decoding and encoding tall screenshots takes noticeable time, and
// so does serialising megabytes of base64 into the request body.
PrepareOutcome prepareJob(const RecognitionRequest &request, const BatchLimits &limits,
                          const RequestOptions &options, const std::function<bool()> &isCanceled)
{
    PrepareOutcome outcome;
    const auto addBatch = [&](const QList<PreparedImage> &images, const QString &text) {
        BatchSpan span;
        for (const PreparedImage &image : images) {
            if (span.firstImage < 0)
                span.firstImage = image.imageIndex;
            span.lastImage = image.imageIndex;
        }
        outcome.batches.append(
            { ai::encodeChatRequest(buildChatRequest(request.kind, *request.vocabulary, images, text, options)),
              int(images.size()), span });
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

DeepSeekRecognizer::DeepSeekRecognizer(ai::ChatClient *client, QObject *parent)
    : RecognitionService(parent)
    , m_client(client)
{
    connect(m_client, &ai::ChatClient::succeeded, this, &DeepSeekRecognizer::chatSucceeded);
    connect(m_client, &ai::ChatClient::failed, this, &DeepSeekRecognizer::chatFailed);
}

DeepSeekRecognizer::~DeepSeekRecognizer()
{
    for (const Job &job : std::as_const(m_jobs)) {
        if (job.watcher)
            job.watcher->cancel();
        if (job.chatId)
            m_client->cancel(job.chatId);
    }
}

quint64 DeepSeekRecognizer::recognize(const RecognitionRequest &request)
{
    const quint64 id = nextRequestId();

    if (!m_client->isConfigured()) {
        failLater(id, makeError(ErrorKind::NotConfigured, QStringLiteral("no API key configured")));
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
    const quint64 chatId = it->chatId;
    QFutureWatcher<PrepareOutcome> *watcher = it->watcher;
    m_jobs.erase(it);
    if (watcher) {
        watcher->cancel(); // lets the worker stop between images
        watcher->deleteLater();
    }
    if (chatId) {
        m_jobByChat.remove(chatId);
        m_client->cancel(chatId);
    }
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
        bytes += batch.request.body.size();
    qCInfo(lcRecognition, "request %llu prepared: kind=%s images=%d slices=%lld batches=%lld bytes=%lld prepare=%lldms",
           id, qUtf8Printable(enumKey(job.request.kind)), int(job.request.images.size()),
           qint64(outcome.sliceCount), qint64(job.batches.size()), bytes, job.total.elapsed());

    sendBatch(id);
}

void DeepSeekRecognizer::sendBatch(quint64 id)
{
    Job &job = m_jobs[id];
    job.batchTimer.start();
    job.chatId = m_client->sendEncoded(job.batches.at(job.batchIndex).request);
    m_jobByChat.insert(job.chatId, id);
}

void DeepSeekRecognizer::chatSucceeded(quint64 chatId, const ai::ChatResult &chat)
{
    const auto owner = m_jobByChat.constFind(chatId);
    if (owner == m_jobByChat.cend())
        return;
    const quint64 id = owner.value();
    m_jobByChat.erase(owner);
    Job &job = m_jobs[id];
    job.chatId = 0;

    const qint64 elapsed = job.batchTimer.elapsed();
    const int batchNumber = job.batchIndex + 1;
    const qsizetype batchTotal = job.batches.size();
    ParseOutcome outcome = parseChatCompletion(chat.rawBody, job.request.vocabulary.get(), job.request.kind);
    if (!outcome.result) {
        qCWarning(lcRecognition, "request %llu batch %d/%lld unparsable: bytes=%lld elapsed=%lldms: %s",
                  id, batchNumber, qint64(batchTotal), qint64(chat.rawBody.size()), elapsed,
                  qUtf8Printable(outcome.error.detail));
        fail(id, outcome.error);
        return;
    }
    qCInfo(lcRecognition, "request %llu batch %d/%lld ok: bytes=%lld elapsed=%lldms items=%lld tokens=%d",
           id, batchNumber, qint64(batchTotal), qint64(chat.rawBody.size()), elapsed,
           qint64(outcome.result->items.size()), outcome.result->usage.totalTokens);

    outcome.result->batchSpans = { job.batches.at(job.batchIndex).span };
    job.parts.append(*outcome.result);
    if (++job.batchIndex < job.batches.size())
        sendBatch(id);
    else
        succeed(id);
}

void DeepSeekRecognizer::chatFailed(quint64 chatId, const ai::ChatError &error)
{
    const auto owner = m_jobByChat.constFind(chatId);
    if (owner == m_jobByChat.cend())
        return;
    const quint64 id = owner.value();
    m_jobByChat.erase(owner);
    fail(id, makeError(error.kind, error.detail, error.httpStatus));
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
