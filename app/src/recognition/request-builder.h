#pragma once

#include "ai/chat-types.h"
#include "recognition/image-preprocessor.h"
#include "recognition/vocabulary.h"

namespace whilefresh::recognition {

inline constexpr int kMaxOrderTextChars = 30000;

// Knobs of the chat request. DeepSeek documents max_tokens as 1..393216 and deepseek-flash
// thinks by default, with the thinking tokens counted against max_tokens; a small budget can
// be eaten entirely by thinking and leave an empty answer. temperature is documented as
// 0..2 and without effect while thinking, so it is sent as 0 for the non-thinking case.
struct RequestOptions {
    int maxTokens = 131072;
    bool thinking = true; // false sends "thinking": {"type": "disabled"}
};

QString promptVersion();

// Full instruction text for one request: the input-kind paragraph, schema, rules and the
// vocabulary grouped by category.
QString buildPrompt(InputKind kind, const Vocabulary &vocabulary);

// Chat request: a single user message holding the prompt, then for each image a text label and
// an image_url block (or the delimited order text for OrderText), in JSON mode.
ai::ChatRequest buildChatRequest(InputKind kind, const Vocabulary &vocabulary,
                                 const QList<PreparedImage> &images, const QString &text,
                                 const RequestOptions &options = {});

}
