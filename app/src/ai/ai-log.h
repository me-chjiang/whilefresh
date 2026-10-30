#pragma once

#include <QLoggingCategory>

namespace whilefresh::ai {

// AI logs carry sizes, counts, timings, token usage and status codes only; never keys, image
// data, prompts or model output.
Q_DECLARE_LOGGING_CATEGORY(lcAi)

}
