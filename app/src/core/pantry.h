#pragma once

#include "core/catalog.h"
#include "core/fridge-item.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

namespace whilefresh::core {

// The user's changes to the vocabulary defaults. Unset fields keep the default.
struct PantryOverride {
    QString name;
    std::optional<bool> pantry; // counts as "家里有" without being in the fridge
    std::optional<bool> bulk;   // "用掉" keeps it; only "家里没有" removes it
    bool out = false;           // a pantry item the user marked "家里没有"
};

// Pantry (常备) and bulk (存量多) rules: vocabulary defaults (pantry_default / bulk_default)
// overridden by the user. Never changed by the system on its own; see Suggestion.
class PantryRules
{
public:
    explicit PantryRules(const IngredientCatalog *catalog = nullptr) : m_catalog(catalog) {}

    void setOverride(const PantryOverride &override) { m_overrides.insert(override.name, override); }
    PantryOverride overrideFor(const QString &name) const;

    bool isPantry(const QString &name) const;
    bool isBulk(const QString &name) const;
    bool isOut(const QString &name) const;
    bool isAtHome(const QString &name) const { return isPantry(name) && !isOut(name); }

    // Effective pantry members in vocabulary order, then user-added names.
    QStringList pantryNames() const;
    QStringList namesAtHome() const;

private:
    const IngredientCatalog *m_catalog = nullptr;
    QHash<QString, PantryOverride> m_overrides;
};

// "用掉": bulk items stay in stock, everything else is used up.
ItemStatus statusAfterUse(bool bulk);

// A change the system proposes; applied only when the user accepts it.
enum class SuggestionKind {
    MarkPantryHave, // bought a pantry item the user had marked "家里没有"
    AddToPantry,    // bought a seasoning that is not in the pantry list
    AddToShopping,  // a pantry or bulk item is "家里没有"
};
QString suggestionKindKey(SuggestionKind kind);
std::optional<SuggestionKind> suggestionKindFromKey(const QString &key);

struct Suggestion {
    qint64 id = 0;
    SuggestionKind kind = SuggestionKind::MarkPantryHave;
    QString name;
    QString message;
    qint64 importId = 0;
    QDateTime createdAt;
};

// For a seasoning that was just bought and confirmed in an import.
std::optional<Suggestion> suggestionForPurchase(const QString &name, const PantryRules &rules);
// After the user marks something "家里没有".
std::optional<Suggestion> suggestionForAbsent(const QString &name, const PantryRules &rules);

}
