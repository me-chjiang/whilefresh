#include "recipes/recipe-types.h"

#include "recipes/recipes-log.h"

namespace whilefresh::recipes {

Q_LOGGING_CATEGORY(lcRecipes, "whilefresh.recipes")

QString enumKey(RecipeErrorKind kind)
{
    switch (kind) {
    case RecipeErrorKind::NotConfigured: return QStringLiteral("not_configured");
    case RecipeErrorKind::InvalidQuery: return QStringLiteral("invalid_query");
    case RecipeErrorKind::NotFound: return QStringLiteral("not_found");
    case RecipeErrorKind::Unavailable: return QStringLiteral("unavailable");
    case RecipeErrorKind::Timeout: return QStringLiteral("timeout");
    case RecipeErrorKind::BadResponse: return QStringLiteral("bad_response");
    case RecipeErrorKind::Internal: return QStringLiteral("internal");
    }
    return QStringLiteral("internal");
}

}
