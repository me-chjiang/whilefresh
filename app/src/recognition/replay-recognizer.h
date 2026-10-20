#pragma once

#include "recognition/recorded-recognizer.h"

namespace whilefresh::recognition {

// Answers every request with the same recording file (see recording.h). For tests and offline
// development; the request's vocabulary is still applied to the recorded model output.
class ReplayRecognizer : public RecordedRecognizer
{
    Q_OBJECT

public:
    explicit ReplayRecognizer(QString recordingPath, QObject *parent = nullptr)
        : RecordedRecognizer(parent), m_path(std::move(recordingPath))
    {
    }

protected:
    QString recordingPathFor(const RecognitionRequest &) const override { return m_path; }
    bool isDemo() const override { return false; }

private:
    QString m_path;
};

}
