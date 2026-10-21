#pragma once

#include "core/catalog.h"
#include "core/recipe.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace whilefresh::core {

enum class ShoppingOrigin { Recipe, Pantry, Manual };
QString shoppingOriginKey(ShoppingOrigin origin);
std::optional<ShoppingOrigin> shoppingOriginFromKey(const QString &key);

struct ShoppingEntry {
    qint64 id = 0;
    QStringList names;  // alternatives; any one will do
    QString category;   // category code of the first name
    ShoppingOrigin origin = ShoppingOrigin::Manual;
    QString note;       // e.g. the recipe name
    QDateTime createdAt;

    QString displayName() const; // "牛奶 或 淡奶油"
    QString dedupeKey() const;   // order-insensitive
};

struct ShoppingSection {
    QString category;
    QString title; // category display name
    QList<ShoppingEntry> entries;
};

ShoppingEntry makeShoppingEntry(const QStringList &names, ShoppingOrigin origin, const QString &note,
                                const IngredientCatalog *catalog);
// One entry per missing required group of the recipe.
QList<ShoppingEntry> shoppingEntriesForMissing(const RecipeMatch &match, const IngredientCatalog *catalog);

// Entries grouped by category in vocabulary order, keeping insertion order inside a category.
QList<ShoppingSection> groupShoppingEntries(const QList<ShoppingEntry> &entries, const IngredientCatalog *catalog);

// Plain text to copy or share: a title, then one block per category. No prices.
QString shoppingListText(const QList<ShoppingEntry> &entries, const IngredientCatalog *catalog);

}
