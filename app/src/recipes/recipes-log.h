#pragma once

#include <QLoggingCategory>

namespace whilefresh::recipes {

// Recipe logs carry ids, counts, status codes and timings only; never credentials, recipe text
// or the source's raw documents.
Q_DECLARE_LOGGING_CATEGORY(lcRecipes)

}
