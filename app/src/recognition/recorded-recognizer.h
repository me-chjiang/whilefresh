#pragma once

#include "recognition/recognition-service.h"

#include <QSet>

namespace whilefresh::recognition {

// Shared behaviour of the recognizers that answer from a recording instead of the network.
class RecordedRecognizer : public RecognitionService
{
    Q_OBJECT

public:
    using RecognitionService::RecognitionService;

    quint64 recognize(const RecognitionRequest &request) override;
    void cancel(quint64 requestId) override;

protected:
    virtual QString recordingPathFor(const RecognitionRequest &request) const = 0;
    virtual bool isDemo() const = 0;

private:
    void complete(quint64 requestId, const RecognitionRequest &request);

    QSet<quint64> m_pending;
};

}
