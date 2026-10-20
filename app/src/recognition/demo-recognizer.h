#pragma once

#include "recognition/recorded-recognizer.h"

#include <QDir>

namespace whilefresh::recognition {

// Demo mode: answers from preset recordings, one per input kind, named
// <directory>/<inputKind>.json (screenshot.json, receipt.json, order-text.json).
// Results are flagged isDemo so the UI can label them; they are never real recognitions.
class DemoRecognizer : public RecordedRecognizer
{
    Q_OBJECT

public:
    // Directory holding the presets; the built-in ones are embedded under this resource path.
    static QString builtInDirectory() { return QStringLiteral(":/whilefresh/demo"); }

    explicit DemoRecognizer(QString directory = builtInDirectory(), QObject *parent = nullptr)
        : RecordedRecognizer(parent), m_directory(std::move(directory))
    {
    }

protected:
    QString recordingPathFor(const RecognitionRequest &request) const override
    {
        return QDir(m_directory).filePath(enumKey(request.kind) + QStringLiteral(".json"));
    }
    bool isDemo() const override { return true; }

private:
    QString m_directory;
};

}
