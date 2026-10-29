#pragma once

#include "core/clock.h"
#include "core/fridge-item.h"
#include "core/pantry.h"
#include "core/recipe.h"
#include "core/shopping-list.h"
#include "data/builtin-data.h"
#include "data/store.h"

#include <QObject>
#include <QSet>

namespace whilefresh::ui {

// Dependencies must outlive the service. No QML types or GUI work here.
struct ClearFridgePreview {
    int selectedCount = 0;
    int candidateCount = 0;
    int completeCount = 0;
    QStringList uncovered;
};

class KitchenService : public QObject
{
    Q_OBJECT
public:
    KitchenService(data::Store &store, std::shared_ptr<const data::BuiltinData> builtin,
                   const core::Clock &clock, QObject *parent = nullptr);

    QList<core::FridgeItem> fridge() const;
    core::FreshnessState freshness(const core::FridgeItem &item) const;
    QSet<qint64> defaultSelection() const;
    bool saveItem(core::FridgeItem &item, QString *error = nullptr);
    bool useItem(qint64 id);
    bool markAbsent(qint64 id);
    bool setUseFirst(qint64 id, bool enabled);
    bool changeStorage(qint64 id, core::Storage storage);
    // Recompute locally; it never calls the recognizer or writes a rule.
    core::ShelfLifeResolution resolve(core::FridgeItem &item) const;
    QList<core::RuleProposal> proposalsForEdit(const core::FridgeItem &before,
                                              const core::FridgeItem &after) const;
    bool confirmRule(const core::RuleProposal &proposal);
    bool replaceRule(const core::RuleProposal &proposal, std::optional<core::Storage> oldStorage);
    bool deleteShelfLifeRule(const QString &name, core::Storage storage);
    bool deleteStorageRule(const QString &name);
    QList<core::ShelfLifeRule> shelfLifeRules() const;
    QList<core::StorageRule> storageRules() const;

    core::PantryRules pantryRules() const;
    bool setPantryOverride(const core::PantryOverride &override);
    QList<core::Suggestion> suggestions() const;
    bool decideSuggestion(qint64 id, bool accept);
    // Explicit user action, not a system inference.
    bool markPantryAbsent(const QString &name);

    QList<core::RecipeMatch> recipes(const QString &search = {}) const;
    QList<core::RecipeMatch> clearFridge(const QSet<qint64> &selected) const;
    ClearFridgePreview clearFridgePreview(const QSet<qint64> &selected) const;
    std::optional<core::RecipeMatch> recipe(const QString &id) const;
    const core::RecipeSource &recipeSource() const;
    int addMissingToShopping(const QString &recipeId);
    qint64 addShopping(const QStringList &names);
    QList<core::ShoppingEntry> shopping() const;
    QList<core::ShoppingSection> shoppingSections() const;
    QString shoppingText() const;
    bool removeShopping(qint64 id);

    QList<data::ImportRecord> imports() const;
    bool deleteImport(qint64 id);
    bool deleteAllData();
    bool demoMode() const;
    bool setDemoMode(bool enabled);

signals:
    void aboutToDeleteAllData();
    void changed();
    void shoppingChanged();

private:
    QList<core::StockEntry> stock(const QSet<qint64> &selected = {}) const;
    bool finish(bool ok);
    data::Store &m_store;
    std::shared_ptr<const data::BuiltinData> m_builtin;
    const core::Clock &m_clock;
};

}
