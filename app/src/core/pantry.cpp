#include "core/pantry.h"

#include "core/texts.h"

namespace whilefresh::core {

PantryOverride PantryRules::overrideFor(const QString &name) const
{
    const auto it = m_overrides.constFind(name);
    if (it != m_overrides.constEnd())
        return *it;
    PantryOverride none;
    none.name = name;
    return none;
}

bool PantryRules::isPantry(const QString &name) const
{
    const PantryOverride override = overrideFor(name);
    if (override.pantry)
        return *override.pantry;
    const CatalogEntry *entry = m_catalog ? m_catalog->entry(name) : nullptr;
    return entry && entry->pantryDefault;
}

bool PantryRules::isBulk(const QString &name) const
{
    const PantryOverride override = overrideFor(name);
    if (override.bulk)
        return *override.bulk;
    const CatalogEntry *entry = m_catalog ? m_catalog->entry(name) : nullptr;
    return entry && entry->bulkDefault;
}

bool PantryRules::isOut(const QString &name) const
{
    return overrideFor(name).out;
}

QStringList PantryRules::pantryNames() const
{
    QStringList names;
    if (m_catalog) {
        for (const CatalogEntry &entry : m_catalog->entries()) {
            if (isPantry(entry.name))
                names.append(entry.name);
        }
    }
    QStringList extra;
    for (const PantryOverride &override : m_overrides) {
        if (override.pantry.value_or(false) && !(m_catalog && m_catalog->contains(override.name)))
            extra.append(override.name);
    }
    extra.sort();
    return names + extra;
}

QStringList PantryRules::namesAtHome() const
{
    QStringList names;
    for (const QString &name : pantryNames()) {
        if (!isOut(name))
            names.append(name);
    }
    return names;
}

ItemStatus statusAfterUse(bool bulk)
{
    return bulk ? ItemStatus::InStock : ItemStatus::Used;
}

QString suggestionKindKey(SuggestionKind kind)
{
    switch (kind) {
    case SuggestionKind::MarkPantryHave:
        return QStringLiteral("mark_pantry_have");
    case SuggestionKind::AddToPantry:
        return QStringLiteral("add_to_pantry");
    case SuggestionKind::AddToShopping:
        return QStringLiteral("add_to_shopping");
    }
    return {};
}

std::optional<SuggestionKind> suggestionKindFromKey(const QString &key)
{
    for (SuggestionKind kind : { SuggestionKind::MarkPantryHave, SuggestionKind::AddToPantry,
                                 SuggestionKind::AddToShopping }) {
        if (suggestionKindKey(kind) == key)
            return kind;
    }
    return std::nullopt;
}

std::optional<Suggestion> suggestionForPurchase(const QString &name, const PantryRules &rules)
{
    if (!isRuleEligibleName(name))
        return std::nullopt;
    Suggestion suggestion;
    suggestion.name = name;
    if (rules.isPantry(name)) {
        if (!rules.isOut(name))
            return std::nullopt;
        suggestion.kind = SuggestionKind::MarkPantryHave;
        suggestion.message = texts::suggestionMarkPantryHave(name);
    } else {
        suggestion.kind = SuggestionKind::AddToPantry;
        suggestion.message = texts::suggestionAddToPantry(name);
    }
    return suggestion;
}

std::optional<Suggestion> suggestionForAbsent(const QString &name, const PantryRules &rules)
{
    if (!isRuleEligibleName(name) || !(rules.isPantry(name) || rules.isBulk(name)))
        return std::nullopt;
    Suggestion suggestion;
    suggestion.kind = SuggestionKind::AddToShopping;
    suggestion.name = name;
    suggestion.message = texts::suggestionAddToShopping(name);
    return suggestion;
}

}
