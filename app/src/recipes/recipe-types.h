#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <optional>

namespace whilefresh::recipes {

enum class RecipeErrorKind {
    NotConfigured, // the source cannot even be tried (adapter not built, no program)
    InvalidQuery,  // the source rejected our parameters
    NotFound,
    Unavailable,   // blocked, rate limited, unreachable or upstream markup changed
    Timeout,
    BadResponse,   // not a YAML document of the contract, or an unsupported schema major
    Internal,
};

QString enumKey(RecipeErrorKind kind);

struct RecipeError {
    RecipeErrorKind kind = RecipeErrorKind::BadResponse;
    QString detail; // for logs only; never holds credentials
};

struct RecipeIngredient {
    QString raw;
    QString name;
    QString amount; // empty when the source gives none
    QString category;
    QString required; // "unknown" for this source
    QString evidence;
    QString notes;
    QStringList substitutes;
};

struct RecipeStep {
    int order = 0;
    QString text;
    QString imageUrl;
};

// Where a recipe comes from; the UI shows it and links back (PRD: no cached or bundled recipe
// bodies, the source link and attribution stay available).
struct RecipeAttribution {
    QString sourceName; // "xiachufang"
    QString provider;   // "direct", "lexmount", ...
    QString sourceUrl;
    QString author;
};

// One search hit. Carries no steps or ingredients.
struct RecipeSummary {
    QString id;
    QString name;
    QString description;
    QString imageUrl;
    RecipeAttribution attribution;
};

struct WebRecipe {
    QString id;
    QString name;
    QString description;
    QString imageUrl;
    QString category;
    QString region;
    std::optional<double> rating;
    std::optional<int> ratingCount;
    std::optional<int> madeCount;
    QList<RecipeIngredient> ingredients;
    QList<RecipeStep> steps;
    QStringList tips;
    QString notes;
    RecipeAttribution attribution;
    // The source flagged missing parts (e.g. no steps); show what is there, do not guess.
    bool partial = false;
    QStringList incomplete;
};

struct RecipeSearchResult {
    QString keyword;
    int page = 1;
    int pageSize = 0;
    int totalCandidates = 0;
    QList<RecipeSummary> items;
    bool partial = false;
    QStringList incomplete;
    QStringList warnings;
};

}

Q_DECLARE_METATYPE(whilefresh::recipes::RecipeError)
Q_DECLARE_METATYPE(whilefresh::recipes::RecipeSearchResult)
Q_DECLARE_METATYPE(whilefresh::recipes::WebRecipe)
