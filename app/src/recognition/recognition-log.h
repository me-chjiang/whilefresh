#pragma once

#include <QLoggingCategory>

namespace whilefresh::recognition {

// Recognition logs carry sizes, counts, timings, status codes and vocabulary words only;
// never keys, image data, prompts or user text.
Q_DECLARE_LOGGING_CATEGORY(lcRecognition)

}
