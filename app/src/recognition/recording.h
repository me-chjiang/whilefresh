#pragma once

#include "recognition/recognition-types.h"

#include <QJsonObject>

namespace whilefresh::recognition {

// A recorded recognition: a summary of the request plus the raw chat-completion bodies.
//
//   { "format": 1,
//     "request": { "inputKind": "screenshot", "imageCount": 3, "textLength": 0,
//                  "promptVersion": "v1" },
//     "responses": [ { <raw /chat/completions body> }, ... ] }
//
// Recordings of synthetic material go to test-data/recordings/; recordings of real material
// must stay under private/recordings/.
struct Recording {
    InputKind inputKind = InputKind::Screenshot;
    int imageCount = 0;
    int textLength = 0;
    QString promptVersion;
    QList<QByteArray> responses; // raw bodies, compact JSON
};

Recording makeRecording(const RecognitionRequest &request, const RecognitionResult &result);

std::optional<Recording> readRecording(const QString &path, QString *error = nullptr);
bool writeRecording(const QString &path, const Recording &recording, QString *error = nullptr);

// Parses every recorded response with the request's vocabulary and merges them like a live
// multi-batch import.
std::optional<RecognitionResult> resultFromRecording(const Recording &recording,
                                                     const RecognitionRequest &request,
                                                     RecognitionError *error);

}
