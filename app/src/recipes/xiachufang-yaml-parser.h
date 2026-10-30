#pragma once

#include "recipes/recipe-types.h"

#include <QByteArray>

#include <optional>

namespace whilefresh::recipes {

// One document of the recipe-source YAML contract (schema_version 1.x).
struct RecipeDocument {
    enum class Operation { Search, Detail };

    QString schemaVersion;
    Operation operation = Operation::Search;
    RecipeSearchResult search; // Operation::Search
    std::optional<WebRecipe> recipe; // Operation::Detail
    double elapsedMs = 0;
};

struct ParsedRecipeDocument {
    std::optional<RecipeDocument> document;
    RecipeError error; // meaningful when document is empty
    // Echo of the query, filled as far as the document got, error documents included. The source
    // strips credentials before echoing.
    std::optional<RecipeDocument::Operation> operation;
    QString queryKeyword;
    QString queryId;
    int queryPage = 1;

    bool ok() const { return document.has_value(); }
};

// status.code error becomes a typed error, partial sets the partial flag, complete is plain.
// Unknown schema majors and malformed input yield BadResponse; nothing here throws.
ParsedRecipeDocument parseXiachufangYaml(const QByteArray &yaml);

}
