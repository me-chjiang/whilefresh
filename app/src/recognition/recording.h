#pragma once

#include "recognition/recognition-types.h"

#include <QJsonObject>

namespace whilefresh::recognition {

// A recorded recognition: a summary of the request plus the raw chat-completion bodies.
//
//   { "format": 1,
//     "request": { "inputKind": "screenshot", "imageCount": 3, "textLength": 0,
//                  "promptVersion": "v2" },
//     "batches": [ { "firstImage": 0, "lastImage": 3 }, ... ],      // optional, one per response
//     "responses": [ { <raw /chat/completions body> }, ... ] }
//
// "batches" tells which original images each response covered; without it replay cannot tell
// overlap duplicates from real ones and drops nothing.
//
// Files are written readable by the owner only: they hold what the user bought.
//
// Recordings of synthetic material go to test-data/recordings/; recordings of real material
// must stay under private/recordings/.
struct Recording {
    InputKind inputKind = InputKind::Screenshot;
    int imageCount = 0;
    int textLength = 0;
    QString promptVersion;
    QList<QByteArray> responses; // raw bodies, compact JSON
    QList<BatchSpan> batches;    // parallel to responses, or empty
};

// Whether recording to `path` is acceptable: anything outside the repository roots, or inside
// `<root>/private/`. Real-sample recordings must never land in a tracked directory.
bool isRecordingPathAllowed(const QString &path, const QStringList &repositoryRoots);

Recording makeRecording(const RecognitionRequest &request, const RecognitionResult &result);

std::optional<Recording> readRecording(const QString &path, QString *error = nullptr);
bool writeRecording(const QString &path, const Recording &recording, QString *error = nullptr);

// Parses every recorded response with the request's vocabulary and merges them like a live
// multi-batch import.
std::optional<RecognitionResult> resultFromRecording(const Recording &recording,
                                                     const RecognitionRequest &request,
                                                     RecognitionError *error);

}
