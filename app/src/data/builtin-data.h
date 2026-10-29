#pragma once

#include "core/catalog.h"
#include "core/recipe.h"
#include "core/shelf-life.h"

#include <QByteArray>
#include <QString>

#include <memory>

namespace whilefresh::data {

// The read-only data shipped with the app (app/assets/data/), loaded once into memory.
struct BuiltinData {
    std::shared_ptr<const core::IngredientCatalog> catalog;
    std::shared_ptr<const core::RecipeBook> recipes;
    std::shared_ptr<const core::ShelfLifeTable> shelfLife; // empty table when the file is absent

    // Where the files are embedded (qrc).
    static QString builtInDirectory() { return QStringLiteral(":/whilefresh/data"); }

    // Reads recipes.json, vocabulary.json and (optionally) shelf-life.json from `directory`.
    static std::shared_ptr<const BuiltinData> load(const QString &directory, QString *error = nullptr);
    // The embedded data, loaded on first use and kept for the life of the process.
    static std::shared_ptr<const BuiltinData> builtIn(QString *error = nullptr);
};

}
