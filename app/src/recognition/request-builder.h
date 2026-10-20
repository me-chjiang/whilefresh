#pragma once

#include "recognition/image-preprocessor.h"
#include "recognition/vocabulary.h"

#include <QJsonObject>

namespace whilefresh::recognition {

inline constexpr char kDeepSeekModel[] = "deepseek-flash";
inline constexpr int kMaxOrderTextChars = 30000;

QString promptVersion();

// Full instruction text for one request: the input-kind paragraph, schema, rules and the
// vocabulary grouped by category.
QString buildPrompt(InputKind kind, const Vocabulary &vocabulary);

// Chat-completions body: a single user message holding the prompt, then for each image a
// text label and an image_url block (or the delimited order text for OrderText), with
// response_format json_object.
QJsonObject buildChatRequest(InputKind kind, const Vocabulary &vocabulary,
                             const QList<PreparedImage> &images, const QString &text);

}
