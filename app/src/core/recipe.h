#pragma once

#include "core/catalog.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace whilefresh::core {

// A requirement group: alternatives, any one of which satisfies it ("牛奶或淡奶油").
using RequirementGroup = QStringList;

struct RecipeIngredient {
    QString raw;
    QString name;
    bool optional = false;
    QString kind; // "ingredient", "tool" or "unknown"
    // All groups are needed; an empty list means "ignore" (tools, water-like fillers, vague words).
    QList<RequirementGroup> groups;
};

struct RecipeQuantity {
    QString name;
    QString expr;
};

// One HowToCook recipe from app/assets/data/recipes.json; text is kept verbatim.
struct Recipe {
    QString id;
    QString name;
    QString category;
    QString categoryZh;
    std::optional<int> difficulty;
    std::optional<int> caloriesKcal;
    std::optional<int> timeMinutes; // only when the source states it
    QString summary;
    QList<RecipeIngredient> ingredients;
    QStringList ingredientNotes;
    QList<RecipeQuantity> quantities;
    QStringList quantityNotes;
    QStringList steps;
    QString notes;
    QString sourceUrl;
};

struct RecipeSource {
    QString name;
    QString url;
    QString commit;
    QString license;
    QString licenseUrl;
};

class RecipeBook
{
public:
    static std::optional<RecipeBook> fromJson(const QByteArray &json, QString *error = nullptr);

    const QList<Recipe> &recipes() const { return m_recipes; }
    const Recipe *find(const QString &id) const;
    const RecipeSource &source() const { return m_source; }
    int size() const { return m_recipes.size(); }

private:
    QList<Recipe> m_recipes;
    QHash<QString, int> m_index;
    RecipeSource m_source;
};

// Something that can satisfy a requirement: a fridge item in stock, or a pantry name at home.
struct StockEntry {
    QString name;        // standard name
    qint64 itemId = 0;   // 0 for pantry entries
    bool pantry = false;
    bool selected = false;   // chosen on the clear-fridge page
    bool nearExpiry = false; // currently "临期"
};

struct GroupMatch {
    RequirementGroup group;
    bool optional = false;
    QList<StockEntry> satisfiedBy; // empty when missing
    QString ingredientRaw;         // first recipe line asking for this group

    bool satisfied() const { return !satisfiedBy.isEmpty(); }
};

struct RecipeMatch {
    const Recipe *recipe = nullptr;
    QList<GroupMatch> groups; // de-duplicated, in recipe order
    int haveCount = 0;    // satisfied required groups ("有 N")
    int missingCount = 0; // unsatisfied required groups ("缺 M")
    int selectedUsed = 0;    // distinct selected items used
    int nearExpiryUsed = 0;  // distinct near-expiry items used
    int inventoryUsed = 0;   // distinct fridge items (not pantry) used

    QList<RequirementGroup> missingGroups() const;
};

// Matches recipes against the stock with the requirement-group model: a group is met when the
// stock holds one of its names or a descendant of one (五花肉 meets 猪肉; 猪肉 does not meet 五花肉).
// Optional ingredients never count as missing.
class RecipeMatcher
{
public:
    RecipeMatcher(const IngredientCatalog *catalog, QList<StockEntry> stock);

    RecipeMatch match(const Recipe &recipe) const;
    QList<RecipeMatch> matchAll(const QList<Recipe> &recipes) const;

private:
    const IngredientCatalog *m_catalog;
    QList<StockEntry> m_stock;
    QHash<QString, QList<int>> m_byRequirement; // requirement name -> stock indices meeting it
};

// Clear-fridge order around the user's selection: more selected items used, fewer missing,
// more near-expiry items used, then recipe name. Recipes using no selected item are dropped.
QList<RecipeMatch> rankForClearFridge(QList<RecipeMatch> matches);
// Recipe page order: fewer missing, more fridge items used, then recipe name.
void sortForBrowsing(QList<RecipeMatch> &matches);

}
