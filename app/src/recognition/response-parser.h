#pragma once

#include "recognition/recognition-types.h"
#include "core/catalog.h"

namespace whilefresh::recognition {

struct ParseOutcome {
    std::optional<RecognitionResult> result;
    RecognitionError error; // meaningful when result is empty
};

// Parses a chat-completions HTTP body. The model text may be wrapped in a ```json fence;
// missing or mistyped item fields fall back to defaults; canonical names outside the
// catalog (aliases are mapped to their standard name) become uncategorizedName(); refining by
// the raw name is left to core::VocabularyResolver. With a null catalog names are kept as the
// model wrote them.
// Fails with BadResponse when the body, the model text or the "items" array is unusable,
// or when the reply was cut off (finish_reason "length").
ParseOutcome parseChatCompletion(const QByteArray &body, const core::IngredientCatalog *catalog, InputKind kind);

// Same for the bare model text, with usage left at zero.
ParseOutcome parseModelContent(const QString &content, const core::IngredientCatalog *catalog, InputKind kind);

// Merges the results of consecutive requests of one import; usage, raw responses and batch
// spans are summed / concatenated. Two identical-looking items are two real purchases, so
// the only duplicates dropped are those caused by the overlap between slices: when one
// original image was split across two consecutive requests (known from the batch spans), an
// item of the later request that repeats (same rawName, quantityText and isGift) an item of
// the earlier request, both attributed to that image or to none, is dropped. Without span
// information nothing is dropped.
RecognitionResult mergeResults(const QList<RecognitionResult> &results);

}
