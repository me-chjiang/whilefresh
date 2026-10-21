#pragma once

#include "core/catalog.h"
#include "core/clock.h"
#include "core/fridge-item.h"
#include "core/import-duplicates.h"
#include "core/pantry.h"
#include "core/shelf-life.h"
#include "core/shopping-list.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace whilefresh::data {

class Database;

struct ImportImageRecord {
    QString path; // in the app's private imports directory
    QString sha256;
    QString mimeType;
    QDateTime capturedAt;
    QString capturedAtSource; // platform DateSource name, empty when unknown
};

struct ImportRecord {
    qint64 id = 0;
    QDateTime createdAt;
    QString inputKind; // "screenshot", "receipt", "order-text" or "manual"
    QString sourceType = QStringLiteral("other"); // recognized cart / order / receipt / text / other
    QString origin;    // "gallery", "share", "camera", "paste", ...
    bool isDemo = false;
    bool truncated = false; // the recognizer's answer was cut off
    int textLength = 0;
    QList<ImportImageRecord> images;
};

// All persistent user data. Plain synchronous calls on the GUI thread; the data set is small.
// Failures are logged and reported through the return value.
class Store
{
public:
    // `importsDirectory` is the only directory whose files this store may delete.
    Store(Database &database, const core::Clock &clock, QString importsDirectory);

    bool transaction();
    bool commit();
    void rollback();

    // Items
    qint64 insertItem(core::FridgeItem &item); // sets id, createdAt, updatedAt
    bool updateItem(core::FridgeItem &item);   // refreshes updatedAt
    std::optional<core::FridgeItem> item(qint64 id) const;
    QList<core::FridgeItem> items(std::optional<core::ItemStatus> status = core::ItemStatus::InStock) const;
    QList<core::FridgeItem> itemsForImport(qint64 importId) const;

    // Imports
    qint64 insertImport(ImportRecord &record); // sets id and createdAt when unset
    std::optional<ImportRecord> importRecord(qint64 id) const;
    QList<ImportRecord> imports() const; // newest first
    QList<core::ImportFingerprint> importFingerprints() const;
    bool saveImportProducts(qint64 id, const QList<QPair<QString, QString>> &products);
    // Retry queued filesystem deletions after a crash or temporary failure; call at startup too.
    bool retryFileCleanup();
    bool discardImportFiles(const QStringList &paths);
    // Deletes the import, the items it produced and its image files.
    bool deleteImport(qint64 id);
    // Deletes every row and every file in the imports directory. The schema stays.
    bool deleteAllData();
    // Deletes a file only when it lies directly in the imports directory.
    bool removeImportFile(const QString &path) const;

    // Local memory rules
    QList<core::ShelfLifeRule> shelfLifeRules() const;
    bool saveShelfLifeRule(const core::ShelfLifeRule &rule);
    bool deleteShelfLifeRule(const QString &name, core::Storage storage);
    QList<core::StorageRule> storageRules() const;
    bool saveStorageRule(const core::StorageRule &rule);
    bool deleteStorageRule(const QString &name);
    core::RuleBook ruleBook() const;

    // AI estimate cache: first value wins.
    core::AiEstimateCache aiEstimateCache() const;
    bool cacheAiEstimate(const core::AiCacheEntry &entry);

    // Pantry / bulk overrides
    QList<core::PantryOverride> pantryOverrides() const;
    bool savePantryOverride(const core::PantryOverride &override);
    bool deletePantryOverride(const QString &name);
    core::PantryRules pantryRules(const core::IngredientCatalog *catalog) const;

    // Suggestions awaiting the user's decision
    // Returns the id; an identical pending suggestion (same kind and name) is reused.
    qint64 addSuggestion(core::Suggestion &suggestion);
    QList<core::Suggestion> pendingSuggestions() const;
    std::optional<core::Suggestion> suggestion(qint64 id) const;
    bool resolveSuggestion(qint64 id, bool accepted);

    // Shopping list
    // Returns the new id, or 0 when an entry with the same names already exists.
    qint64 addShoppingEntry(core::ShoppingEntry &entry);
    QList<core::ShoppingEntry> shoppingEntries() const;
    bool removeShoppingEntry(qint64 id);
    bool clearShoppingList();

    // Settings
    QString setting(const QString &key, const QString &fallback = {}) const;
    bool setSetting(const QString &key, const QString &value);

    const QString &importsDirectory() const { return m_importsDirectory; }

private:
    QString now() const;

    Database &m_database;
    const core::Clock &m_clock;
    QString m_importsDirectory;
};

}
