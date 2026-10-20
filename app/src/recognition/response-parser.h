#pragma once

#include "recognition/recognition-types.h"
#include "recognition/vocabulary.h"

namespace whilefresh::recognition {

struct ParseOutcome {
    std::optional<RecognitionResult> result;
    RecognitionError error; // meaningful when result is empty
};

// Parses a chat-completions HTTP body. The model text may be wrapped in a ```json fence;
// missing or mistyped item fields fall back to defaults; canonical names outside the
// vocabulary (aliases are mapped to their standard name) become uncategorizedName().
// With a null vocabulary names are kept as the model wrote them.
// Fails with BadResponse when the body, the model text or the "items" array is unusable,
// or when the reply was cut off (finish_reason "length").
ParseOutcome parseChatCompletion(const QByteArray &body, const Vocabulary *vocabulary, InputKind kind);

// Same for the bare model text, with usage left at zero.
ParseOutcome parseModelContent(const QString &content, const Vocabulary *vocabulary, InputKind kind);

// Merges the results of consecutive requests of one import. Items of later results that
// repeat an earlier item (same rawName, quantityText and isGift) are dropped; usage and raw
// responses are summed / concatenated.
RecognitionResult mergeResults(const QList<RecognitionResult> &results);

}
