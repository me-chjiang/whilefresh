#include "ui/kitchen-service.h"

#include "core/fridge-groups.h"
#include "core/quantity.h"

namespace whilefresh::ui {
using namespace core;

KitchenService::KitchenService(data::Store &store, std::shared_ptr<const data::BuiltinData> builtin,
                               const Clock &clock, QObject *parent)
    : QObject(parent), m_store(store), m_builtin(std::move(builtin)), m_clock(clock)
{
}

bool KitchenService::finish(bool ok)
{
    if (ok)
        emit changed();
    return ok;
}

QList<FridgeItem> KitchenService::fridge() const { return sortForFridge(m_store.items(), m_clock.today()); }

QList<FridgeItem> KitchenService::attention() const
{
    QList<FridgeItem> list;
    const QDate today = m_clock.today();
    for (const auto &item : fridge()) {
        const auto freshness = item.freshness(today).freshness;
        if ((freshness == Freshness::NearExpiry || freshness == Freshness::BeyondEstimate) && !item.isSnoozed(today))
            list.append(item);
    }
    return list;
}

void KitchenService::classify(FridgeItem &item) const
{
    item.kind = classifyItemKind(item.canonicalName, item.kind == ItemKind::StapleSeasoning, item.storage,
                                 m_builtin->catalog.get(), m_builtin->shelfLife.get());
}
FreshnessState KitchenService::freshness(const FridgeItem &item) const { return item.freshness(m_clock.today()); }

QSet<qint64> KitchenService::defaultSelection() const
{
    QSet<qint64> ids;
    for (const auto &item : fridge()) {
        if (item.useFirst || freshness(item).freshness == Freshness::NearExpiry)
            ids.insert(item.id);
    }
    return ids;
}

ShelfLifeResolution KitchenService::resolve(FridgeItem &item) const
{
    const auto rules = m_store.ruleBook();
    const auto cache = m_store.aiEstimateCache();
    const auto result = resolveShelfLife(item.shelfLifeInput(),
                                        { m_builtin->catalog.get(), m_builtin->shelfLife.get(), &rules, &cache });
    item.applyShelfLife(result);
    return result;
}

bool KitchenService::saveItem(FridgeItem &item, QString *error)
{
    if (item.displayName.trimmed().isEmpty() || item.count <= 0
        || (!m_builtin->catalog->contains(item.canonicalName) && item.canonicalName != QStringLiteral("未归类"))
        || (item.manualDays && *item.manualDays <= 0) || (item.labelledDays && *item.labelledDays <= 0)) {
        if (error)
            *error = QStringLiteral("请填写名称、有效数量和保质天数，并确认标准食材名");
        return false;
    }
    if (item.id) {
        const auto before = m_store.item(item.id);
        if (!before)
            return false;
        item.importId = before->importId;
        item.entryDate = before->entryDate;
        item.createdAt = before->createdAt;
        if (before->canonicalName != item.canonicalName) {
            item.aiDays = {};
            item.referenceConditionsConfirmed = false;
        }
        if (before->storage != item.storage) {
            item.referenceConditionsConfirmed = false;
            if (before->manualDays == item.manualDays)
                item.manualDays.reset();
        }
        if (before->productionDate.date != item.productionDate.date)
            item.productionDate.origin = item.productionDate.isSet() ? DateOrigin::User : DateOrigin::None;
        if (before->purchaseDate.date != item.purchaseDate.date)
            item.purchaseDate.origin = item.purchaseDate.isSet() ? DateOrigin::User : DateOrigin::None;
        if (before->packageExpiry.date != item.packageExpiry.date) {
            item.packageExpiry.origin = item.packageExpiry.isSet() ? DateOrigin::User : DateOrigin::None;
            item.manualDays.reset();
        }
        if (before->quantityText != item.quantityText || before->count != item.count) {
            const Amount amount = totalAmount(item.quantityText, item.count);
            item.amount = amount.value;
            item.unit = amount.unit;
        }
    } else {
        item.importId = 0;
        item.entryDate = m_clock.today();
        item.productionDate.origin = item.productionDate.isSet() ? DateOrigin::User : DateOrigin::None;
        item.purchaseDate.origin = item.purchaseDate.isSet() ? DateOrigin::User : DateOrigin::None;
        item.packageExpiry.origin = item.packageExpiry.isSet() ? DateOrigin::User : DateOrigin::None;
        const Amount amount = totalAmount(item.quantityText, item.count);
        item.amount = amount.value;
        item.unit = amount.unit;
    }
    item.category = m_builtin->catalog->categoryOf(item.canonicalName);
    classify(item);
    resolve(item);
    return finish(item.id ? m_store.updateItem(item) : m_store.insertItem(item) > 0);
}

bool KitchenService::useItem(qint64 id)
{
    auto item = m_store.item(id);
    if (!item || item->status != ItemStatus::InStock)
        return false;
    item->status = statusAfterUse(pantryRules().isBulk(item->canonicalName));
    item->statusChangedAt = m_clock.now();
    return finish(m_store.updateItem(*item));
}

bool KitchenService::markAbsent(qint64 id)
{
    auto item = m_store.item(id);
    if (!item || !m_store.transaction())
        return false;
    item->status = ItemStatus::Absent;
    item->statusChangedAt = m_clock.now();
    bool ok = m_store.updateItem(*item);
    const auto rules = pantryRules();
    // A pantry name may still have another physical packet in stock.
    bool another = false;
    for (const auto &other : fridge())
        another = another || other.canonicalName == item->canonicalName;
    if (!another) {
        if (rules.isPantry(item->canonicalName)) {
            auto override = rules.overrideFor(item->canonicalName);
            override.out = true;
            ok = ok && m_store.savePantryOverride(override);
        }
        if (auto suggestion = suggestionForAbsent(item->canonicalName, rules))
            ok = ok && m_store.addSuggestion(*suggestion) > 0;
    }
    if (!ok || !m_store.commit()) {
        m_store.rollback();
        return false;
    }
    return finish(true);
}

bool KitchenService::setUseFirst(qint64 id, bool enabled)
{
    auto item = m_store.item(id);
    if (!item)
        return false;
    item->useFirst = enabled;
    return finish(m_store.updateItem(*item));
}

bool KitchenService::changeStorage(qint64 id, Storage storage)
{
    auto item = m_store.item(id);
    if (!item)
        return false;
    item->storage = storage;
    // A single-item manual day count is specific to its old storage.
    item->manualDays.reset();
    item->referenceConditionsConfirmed = false;
    classify(*item);
    resolve(*item);
    return finish(m_store.updateItem(*item));
}

bool KitchenService::discardItem(qint64 id)
{
    auto item = m_store.item(id);
    if (!item || item->status != ItemStatus::InStock)
        return false;
    item->status = ItemStatus::Discarded;
    item->statusChangedAt = m_clock.now();
    return finish(m_store.updateItem(*item));
}

bool KitchenService::snoozeItem(qint64 id)
{
    auto item = m_store.item(id);
    if (!item || item->status != ItemStatus::InStock || freshness(*item).freshness != Freshness::NearExpiry)
        return false;
    item->snoozedUntil = m_clock.today().addDays(1);
    return finish(m_store.updateItem(*item));
}

QList<RuleProposal> KitchenService::proposalsForEdit(const FridgeItem &before, const FridgeItem &after) const
{
    QList<RuleProposal> result;
    if (!m_builtin->catalog->contains(after.canonicalName))
        return result;
    if (before.storage != after.storage) {
        if (auto proposal = proposeStorageRule(after.canonicalName, after.storage))
            result.append(*proposal);
    }
    if (before.manualDays != after.manualDays || before.expiryDate != after.expiryDate
        || before.purchaseDate.date != after.purchaseDate.date || before.productionDate.date != after.productionDate.date) {
        if (after.shelfLifeDays) {
            if (auto proposal = proposeShelfLifeRule(after.canonicalName, after.storage, *after.shelfLifeDays))
                result.append(*proposal);
        }
    }
    return result;
}

bool KitchenService::confirmRule(const RuleProposal &proposal)
{
    if (!m_builtin->catalog->contains(proposal.name))
        return false;
    return finish(proposal.kind == RuleProposal::Kind::ShelfLife
                      ? m_store.saveShelfLifeRule({ proposal.name, proposal.storage, proposal.days })
                      : m_store.saveStorageRule({ proposal.name, proposal.storage }));
}

bool KitchenService::deleteShelfLifeRule(const QString &name, Storage storage)
{ return finish(m_store.deleteShelfLifeRule(name, storage)); }
bool KitchenService::deleteStorageRule(const QString &name) { return finish(m_store.deleteStorageRule(name)); }
QList<ShelfLifeRule> KitchenService::shelfLifeRules() const { return m_store.shelfLifeRules(); }
QList<StorageRule> KitchenService::storageRules() const { return m_store.storageRules(); }
PantryRules KitchenService::pantryRules() const { return m_store.pantryRules(m_builtin->catalog.get()); }
QList<Suggestion> KitchenService::suggestions() const { return m_store.pendingSuggestions(); }

bool KitchenService::setPantryOverride(const PantryOverride &override)
{
    return m_builtin->catalog->contains(override.name) && finish(m_store.savePantryOverride(override));
}

bool KitchenService::markPantryAbsent(const QString &name)
{
    const auto rules = pantryRules();
    if (!rules.isPantry(name) || !m_store.transaction())
        return false;
    auto override = rules.overrideFor(name);
    override.out = true;
    bool ok = m_store.savePantryOverride(override);
    for (auto item : fridge()) {
        if (item.canonicalName != name)
            continue;
        item.status = ItemStatus::Absent;
        item.statusChangedAt = m_clock.now();
        ok = ok && m_store.updateItem(item);
    }
    if (auto suggestion = suggestionForAbsent(name, rules))
        ok = ok && m_store.addSuggestion(*suggestion) > 0;
    if (!ok || !m_store.commit()) {
        m_store.rollback();
        return false;
    }
    return finish(true);
}

bool KitchenService::decideSuggestion(qint64 id, bool accept)
{
    const auto suggestion = m_store.suggestion(id);
    if (!suggestion || !m_store.transaction())
        return false;
    bool ok = true;
    if (accept) {
        if (suggestion->kind == SuggestionKind::AddToShopping) {
            auto entry = makeShoppingEntry({ suggestion->name }, ShoppingOrigin::Pantry, {}, m_builtin->catalog.get());
            m_store.addShoppingEntry(entry);
            ok = false;
            for (const auto &existing : shopping())
                ok = ok || existing.dedupeKey() == entry.dedupeKey();
        } else {
            auto override = pantryRules().overrideFor(suggestion->name);
            override.pantry = true;
            override.out = false;
            ok = m_store.savePantryOverride(override);
        }
    }
    ok = ok && m_store.resolveSuggestion(id, accept);
    if (!ok || !m_store.commit()) {
        m_store.rollback();
        return false;
    }
    return finish(true);
}

QList<StockEntry> KitchenService::stock(const QSet<qint64> &selected) const
{
    QList<StockEntry> result;
    for (const auto &item : fridge())
        result.append({ item.canonicalName, item.id, false, selected.contains(item.id),
                        freshness(item).freshness == Freshness::NearExpiry });
    for (const auto &name : pantryRules().namesAtHome())
        result.append({ name, 0, true, false, false });
    return result;
}

QList<RecipeMatch> KitchenService::recipes(const QString &search) const
{
    auto matches = RecipeMatcher(m_builtin->catalog.get(), stock()).matchAll(m_builtin->recipes->recipes());
    if (!search.trimmed().isEmpty())
        matches.removeIf([&](const RecipeMatch &m) { return !m.recipe->name.contains(search.trimmed(), Qt::CaseInsensitive); });
    sortForBrowsing(matches);
    return matches;
}

QList<RecipeMatch> KitchenService::clearFridge(const QSet<qint64> &selected) const
{
    return rankForClearFridge(RecipeMatcher(m_builtin->catalog.get(), stock(selected)).matchAll(m_builtin->recipes->recipes()));
}

std::optional<RecipeMatch> KitchenService::recipe(const QString &id) const
{
    const auto *recipe = m_builtin->recipes->find(id);
    if (!recipe)
        return std::nullopt;
    return RecipeMatcher(m_builtin->catalog.get(), stock()).match(*recipe);
}

const RecipeSource &KitchenService::recipeSource() const { return m_builtin->recipes->source(); }

int KitchenService::addMissingToShopping(const QString &recipeId)
{
    const auto match = recipe(recipeId);
    if (!match)
        return 0;
    int count = 0;
    for (auto entry : shoppingEntriesForMissing(*match, m_builtin->catalog.get()))
        count += m_store.addShoppingEntry(entry) > 0;
    if (count)
        emit changed();
    return count;
}

qint64 KitchenService::addShopping(const QStringList &names)
{
    for (const auto &name : names) {
        if (!m_builtin->catalog->contains(name))
            return 0;
    }
    auto entry = makeShoppingEntry(names, ShoppingOrigin::Manual, {}, m_builtin->catalog.get());
    const auto id = m_store.addShoppingEntry(entry);
    finish(id > 0);
    return id;
}

QList<ShoppingEntry> KitchenService::shopping() const { return m_store.shoppingEntries(); }
QList<ShoppingSection> KitchenService::shoppingSections() const
{ return groupShoppingEntries(shopping(), m_builtin->catalog.get()); }
QString KitchenService::shoppingText() const { return shoppingListText(shopping(), m_builtin->catalog.get()); }
bool KitchenService::removeShopping(qint64 id) { return finish(m_store.removeShoppingEntry(id)); }
QList<data::ImportRecord> KitchenService::imports() const { return m_store.imports(); }
bool KitchenService::deleteImport(qint64 id)
{
    const bool ok = m_store.deleteImport(id);
    // SQL may have committed even when filesystem cleanup needs a retry.
    if (ok || !m_store.importRecord(id))
        emit changed();
    return ok;
}

bool KitchenService::clearImportHistory()
{
    const bool ok = m_store.clearImportHistory();
    emit changed(); // records are hidden even when an image still waits for cleanup
    return ok;
}

bool KitchenService::deleteAllData()
{
    emit aboutToDeleteAllData();
    const bool ok = m_store.deleteAllData();
    emit changed();
    return ok;
}
bool KitchenService::demoMode() const { return m_store.setting(QStringLiteral("demo_mode")) == QStringLiteral("1"); }
bool KitchenService::setDemoMode(bool enabled)
{ return finish(m_store.setSetting(QStringLiteral("demo_mode"), enabled ? QStringLiteral("1") : QStringLiteral("0"))); }

}
