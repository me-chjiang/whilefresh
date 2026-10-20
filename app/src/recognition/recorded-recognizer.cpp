#include "recognition/recorded-recognizer.h"

#include "recognition/recording.h"

#include <QTimer>

namespace whilefresh::recognition {

quint64 RecordedRecognizer::recognize(const RecognitionRequest &request)
{
    const quint64 id = nextRequestId();
    m_pending.insert(id);
    QTimer::singleShot(0, this, [this, id, request] { complete(id, request); });
    return id;
}

void RecordedRecognizer::cancel(quint64 requestId)
{
    m_pending.remove(requestId);
}

void RecordedRecognizer::complete(quint64 requestId, const RecognitionRequest &request)
{
    if (!m_pending.remove(requestId))
        return;

    QString loadError;
    const auto recording = readRecording(recordingPathFor(request), &loadError);
    if (!recording) {
        emit failed(requestId, makeError(ErrorKind::BadResponse, loadError));
        return;
    }

    RecognitionError parseError;
    auto result = resultFromRecording(*recording, request, &parseError);
    if (!result) {
        emit failed(requestId, parseError);
        return;
    }
    result->isDemo = isDemo();
    emit succeeded(requestId, *result);
}

}
