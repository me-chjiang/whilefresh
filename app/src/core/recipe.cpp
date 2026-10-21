#include "core/recipe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace whilefresh::core {

namespace {

std::optional<int> optionalInt(const QJsonValue &value)
{
    return value.isDouble() ? std::optional<int>(value.toInt()) : std::nullopt;
}

QStringList stringList(const QJsonValue &value)
{
    QStringList list;
    for (const QJsonValue &item : value.toArray())
        list.append(item.toString());
    return list;
}

// Accepts the canonical form [["a","b"],["c"]] and the legacy flat form ["a","c"].
QList<RequirementGroup> groupsFromJson(const QJsonValue &value)
{
    QList<RequirementGroup> groups;
    for (const QJsonValue &group : value.toArray()) {
        RequirementGroup names = group.isArray() ? stringList(group) : RequirementGroup{ group.toString() };
        names.removeAll(QString());
        if (!names.isEmpty())
            groups.append(names);
    }
    return groups;
}

Recipe recipeFromJson(const QJsonObject &object)
{
    Recipe recipe;
    recipe.id = object.value(QStringLiteral("id")).toString();
    recipe.name = object.value(QStringLiteral("name")).toString();
    recipe.category = object.value(QStringLiteral("category")).toString();
    recipe.categoryZh = object.value(QStringLiteral("category_zh")).toString();
    recipe.difficulty = optionalInt(object.value(QStringLiteral("difficulty")));
    recipe.caloriesKcal = optionalInt(object.value(QStringLiteral("calories_kcal")));
    recipe.timeMinutes = optionalInt(object.value(QStringLiteral("time_minutes")));
    recipe.summary = object.value(QStringLiteral("summary")).toString();
    for (const QJsonValue &value : object.value(QStringLiteral("ingredients")).toArray()) {
        const QJsonObject ingredient = value.toObject();
        recipe.ingredients.append({ ingredient.value(QStringLiteral("raw")).toString(),
                                    ingredient.value(QStringLiteral("name")).toString(),
                                    ingredient.value(QStringLiteral("optional")).toBool(),
                                    ingredient.value(QStringLiteral("kind")).toString(),
                                    groupsFromJson(ingredient.value(QStringLiteral("canonical"))) });
    }
    recipe.ingredientNotes = stringList(object.value(QStringLiteral("ingredient_notes")));
    for (const QJsonValue &value : object.value(QStringLiteral("quantities")).toArray()) {
        const QJsonObject quantity = value.toObject();
        recipe.quantities.append({ quantity.value(QStringLiteral("name")).toString(),
                                   quantity.value(QStringLiteral("expr")).toString() });
    }
    recipe.quantityNotes = stringList(object.value(QStringLiteral("quantity_notes")));
    recipe.steps = stringList(object.value(QStringLiteral("steps")));
    recipe.notes = object.value(QStringLiteral("notes")).toString();
    recipe.sourceUrl = object.value(QStringLiteral("source_url")).toString();
    return recipe;
}

QString groupKey(const RequirementGroup &group)
{
    RequirementGroup sorted = group;
    sorted.sort();
    return sorted.join(QChar(0x1f));
}

}

std::optional<RecipeBook> RecipeBook::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull()) {
        if (error)
            *error = QStringLiteral("recipes: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    RecipeBook book;
    const QJsonObject source = root.value(QStringLiteral("source")).toObject();
    book.m_source = { source.value(QStringLiteral("name")).toString(), source.value(QStringLiteral("url")).toString(),
                      source.value(QStringLiteral("commit")).toString(),
                      source.value(QStringLiteral("license")).toString(),
                      source.value(QStringLiteral("license_url")).toString() };
    const QJsonArray recipes = doc.isArray() ? doc.array() : root.value(QStringLiteral("recipes")).toArray();
    for (const QJsonValue &value : recipes) {
        Recipe recipe = recipeFromJson(value.toObject());
        if (recipe.id.isEmpty() || book.m_index.contains(recipe.id))
            continue;
        book.m_index.insert(recipe.id, book.m_recipes.size());
        book.m_recipes.append(std::move(recipe));
    }
    if (book.m_recipes.isEmpty()) {
        if (error)
            *error = QStringLiteral("recipes: no recipes");
        return std::nullopt;
    }
    return book;
}

const Recipe *RecipeBook::find(const QString &id) const
{
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? nullptr : &m_recipes.at(*it);
}

QList<RequirementGroup> RecipeMatch::missingGroups() const
{
    QList<RequirementGroup> missing;
    for (const GroupMatch &group : groups) {
        if (!group.optional && !group.satisfied())
            missing.append(group.group);
    }
    return missing;
}

RecipeMatcher::RecipeMatcher(const IngredientCatalog *catalog, QList<StockEntry> stock)
    : m_catalog(catalog), m_stock(std::move(stock))
{
    for (int i = 0; i < m_stock.size(); ++i) {
        const QString &name = m_stock.at(i).name;
        QStringList meets{ name };
        if (m_catalog)
            meets += m_catalog->ancestors(name);
        for (const QString &requirement : meets)
            m_byRequirement[requirement].append(i);
    }
}

RecipeMatch RecipeMatcher::match(const Recipe &recipe) const
{
    RecipeMatch result;
    result.recipe = &recipe;
    QHash<QString, int> seen; // group key -> index in result.groups
    for (const RecipeIngredient &ingredient : recipe.ingredients) {
        if (ingredient.kind == QStringLiteral("tool"))
            continue;
        for (const RequirementGroup &group : ingredient.groups) {
            const QString key = groupKey(group);
            if (const auto it = seen.constFind(key); it != seen.constEnd()) {
                // The same need listed twice is one need; required wins over optional.
                result.groups[*it].optional = result.groups[*it].optional && ingredient.optional;
                continue;
            }
            GroupMatch groupMatch;
            groupMatch.group = group;
            groupMatch.optional = ingredient.optional;
            groupMatch.ingredientRaw = ingredient.raw;
            QSet<int> used;
            for (const QString &name : group) {
                for (int index : m_byRequirement.value(name)) {
                    if (!used.contains(index)) {
                        used.insert(index);
                        groupMatch.satisfiedBy.append(m_stock.at(index));
                    }
                }
            }
            seen.insert(key, result.groups.size());
            result.groups.append(groupMatch);
        }
    }

    QSet<qint64> selected, nearExpiry, inventory;
    for (const GroupMatch &group : std::as_const(result.groups)) {
        if (group.satisfied())
            ++result.haveCount;
        else if (!group.optional)
            ++result.missingCount;
        for (const StockEntry &entry : group.satisfiedBy) {
            if (entry.pantry)
                continue;
            inventory.insert(entry.itemId);
            if (entry.selected)
                selected.insert(entry.itemId);
            if (entry.nearExpiry)
                nearExpiry.insert(entry.itemId);
        }
    }
    result.selectedUsed = selected.size();
    result.nearExpiryUsed = nearExpiry.size();
    result.inventoryUsed = inventory.size();
    return result;
}

QList<RecipeMatch> RecipeMatcher::matchAll(const QList<Recipe> &recipes) const
{
    QList<RecipeMatch> matches;
    matches.reserve(recipes.size());
    for (const Recipe &recipe : recipes)
        matches.append(match(recipe));
    return matches;
}

QList<RecipeMatch> rankForClearFridge(QList<RecipeMatch> matches)
{
    matches.removeIf([](const RecipeMatch &match) { return match.selectedUsed == 0; });
    std::stable_sort(matches.begin(), matches.end(), [](const RecipeMatch &a, const RecipeMatch &b) {
        if (a.selectedUsed != b.selectedUsed)
            return a.selectedUsed > b.selectedUsed;
        if (a.missingCount != b.missingCount)
            return a.missingCount < b.missingCount;
        if (a.nearExpiryUsed != b.nearExpiryUsed)
            return a.nearExpiryUsed > b.nearExpiryUsed;
        return a.recipe->name < b.recipe->name;
    });
    return matches;
}

void sortForBrowsing(QList<RecipeMatch> &matches)
{
    std::stable_sort(matches.begin(), matches.end(), [](const RecipeMatch &a, const RecipeMatch &b) {
        if (a.missingCount != b.missingCount)
            return a.missingCount < b.missingCount;
        if (a.inventoryUsed != b.inventoryUsed)
            return a.inventoryUsed > b.inventoryUsed;
        return a.recipe->name < b.recipe->name;
    });
}

}
