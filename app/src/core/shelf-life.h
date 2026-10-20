#pragma once

#include "core/catalog.h"
#include "core/storage.h"

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace whilefresh::core {

// Where the expiry date / shelf life of an item came from, highest priority first (ADR-7).
enum class ShelfLifeSource {
    None,       // nothing known; freshness is unknown
    Package,    // expiry date or shelf life visible on the package / screenshot, or typed in from it
    Manual,     // the user set this item's shelf life by hand
    LocalRule,  // a rule the user confirmed earlier (本地记忆)
    Reference,  // the built-in reference table (USDA FoodKeeper based)
    AiEstimate, // DeepSeek's estimate, cached per standard name + storage
};

QString shelfLifeSourceKey(ShelfLifeSource source);
std::optional<ShelfLifeSource> shelfLifeSourceFromKey(const QString &key);
// 包装标注 / 你填写的 / 你的设置 / 参考表 / AI 估计; everything except Package is an estimate.
QString shelfLifeSourceLabel(ShelfLifeSource source);

// Which date the shelf life counts from.
enum class StartBasis { None, ProductionDate, PurchaseDate, EntryDate };
QString startBasisKey(StartBasis basis);
std::optional<StartBasis> startBasisFromKey(const QString &key);

// One row of app/assets/data/shelf-life.json.
struct ShelfLifeReference {
    StorageDays days;
    QString source;       // e.g. "USDA FoodKeeper"
    QString foodkeeperId; // as text; empty when not from FoodKeeper
    QString basis;        // free text, e.g. how the number was chosen
};

// The built-in reference table, keyed by standard name. An absent file is an empty table.
// Accepted layouts: { "<name>": {...}, ... }, { "items": { "<name>": {...} } } or
// { "items": [ { "name": "<name>", ... } ] }; non-object members (e.g. "version") are skipped.
class ShelfLifeTable
{
public:
    static std::optional<ShelfLifeTable> fromJson(const QByteArray &json, QString *error = nullptr);

    void insert(const QString &name, const ShelfLifeReference &reference) { m_rows.insert(name, reference); }
    bool isEmpty() const { return m_rows.isEmpty(); }
    int size() const { return m_rows.size(); }

    struct Hit {
        int days = 0;
        QString matchedName; // the entry itself or the ancestor that had a value
        ShelfLifeReference reference;
    };
    // Looks up `name`, then each ancestor in the catalog, for a value in `storage`.
    std::optional<Hit> lookup(const QString &name, Storage storage, const IngredientCatalog *catalog) const;
    // First row found for `name` or an ancestor, regardless of storage.
    const ShelfLifeReference *row(const QString &name, const IngredientCatalog *catalog,
                                  QString *matchedName = nullptr) const;

private:
    QHash<QString, ShelfLifeReference> m_rows;
};

// Rules the user confirmed ("以后同类食材都按这个"), held in memory; persisted by the data layer.
struct ShelfLifeRule {
    QString name;
    Storage storage = Storage::Fridge;
    int days = 0;
};
struct StorageRule {
    QString name;
    Storage storage = Storage::Fridge;
};

class RuleBook
{
public:
    void setShelfLife(const ShelfLifeRule &rule) { m_days.insert(key(rule.name, rule.storage), rule.days); }
    void setStorage(const StorageRule &rule) { m_storage.insert(rule.name, rule.storage); }
    std::optional<int> shelfLifeDays(const QString &name, Storage storage) const;
    std::optional<Storage> storage(const QString &name) const;

    static QString key(const QString &name, Storage storage);

private:
    QHash<QString, int> m_days;
    QHash<QString, Storage> m_storage;
};

// The first AI estimate seen for a standard name + storage; reused afterwards so the same food
// does not get a different estimate every import.
class AiEstimateCache
{
public:
    void insert(const QString &name, Storage storage, int days) { m_days.insert(RuleBook::key(name, storage), days); }
    std::optional<int> days(const QString &name, Storage storage) const;

private:
    QHash<QString, int> m_days;
};

struct AiCacheEntry {
    QString name;
    Storage storage = Storage::Fridge;
    int days = 0;
};

struct ShelfLifeInput {
    QString name;          // standard name; empty or "未归类" disables rules, reference and cache
    Storage storage = Storage::Fridge;
    QDate productionDate;  // only when visible or typed by the user
    QDate purchaseDate;
    QDate entryDate;       // set automatically on import
    QDate packageExpiry;   // only when visible or typed by the user
    std::optional<int> labelledDays; // "保质期 N 天" on the package
    std::optional<int> manualDays;   // the user's override for this item
    StorageDays aiDays;              // the recognizer's estimate for all three storages
};

struct ShelfLifeContext {
    const IngredientCatalog *catalog = nullptr;
    const ShelfLifeTable *table = nullptr;
    const RuleBook *rules = nullptr;
    const AiEstimateCache *aiCache = nullptr;
};

struct ShelfLifeResolution {
    ShelfLifeSource source = ShelfLifeSource::None;
    std::optional<int> days; // total shelf life used for freshness; set whenever expiryDate is
    QDate expiryDate;
    StartBasis startBasis = StartBasis::None;
    QDate startDate;
    QString note; // Chinese, shown next to the source label
    // AI values seen for the first time; the caller stores them in the cache.
    QList<AiCacheEntry> newAiEstimates;

    bool isEstimate() const { return source != ShelfLifeSource::Package; }
};

// The source chain (ADR-7, user-confirmed priorities):
//   expiry: package expiry date as is; otherwise start date + days, where start is
//           production date > purchase date > entry date;
//   days:   manual override > labelled on package > local rule > reference table (with parent
//           fallback) > cached AI estimate > fresh AI estimate.
// Changing the storage just re-runs this with the item's stored AI days; no new AI call.
ShelfLifeResolution resolveShelfLife(const ShelfLifeInput &input, const ShelfLifeContext &context);

enum class StorageSource { Default, LocalRule, Reference, AiEstimate };
QString storageSourceKey(StorageSource source);
std::optional<StorageSource> storageSourceFromKey(const QString &key);

struct StorageSuggestion {
    Storage storage = Storage::Fridge;
    StorageSource source = StorageSource::Default;
};

// Storage suggestion: local rule > reference table > AI > fallback (room for seasonings,
// fridge otherwise). From the reference table, the AI's storage wins if the table has a value
// for it; otherwise the only storage with a value, then fridge > room > freezer.
StorageSuggestion suggestStorage(const QString &name, std::optional<Storage> aiStorage, bool isSeasoning,
                                 const ShelfLifeContext &context);

// Offered after the user edits an item; written to the rule book only once the user agrees.
struct RuleProposal {
    enum class Kind { ShelfLife, Storage };
    Kind kind = Kind::ShelfLife;
    QString name;
    Storage storage = Storage::Fridge;
    int days = 0; // for ShelfLife
    QString question; // "以后「鸡蛋」冷藏都按 21 天算吗？"
};

std::optional<RuleProposal> proposeShelfLifeRule(const QString &name, Storage storage, int days);
std::optional<RuleProposal> proposeStorageRule(const QString &name, Storage storage);

bool isRuleEligibleName(const QString &name);

}
