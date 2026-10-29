#pragma once

#include "core/catalog.h"
#include "core/name-mapping.h"

#include <QString>
#include <QStringList>

namespace whilefresh::core {

enum class NameSource {
    Unresolved,
    UserMapping,  // a raw name the user confirmed earlier
    Recognized,   // the recognizer's choice, possibly refined to a contained child
    LocalMatch,   // resolveFinest() on the raw name
    AiClassified, // the batched classification call
    UserEdited,   // changed on the review page
};

QString enumKey(NameSource source);

struct NameResolution {
    QString canonical; // a catalog name, empty when unresolved
    NameSource source = NameSource::Unresolved;
    bool resolved() const { return !canonical.isEmpty(); }
};

// Maps a recognized product to a standard name, in this order:
//   1. a mapping the user confirmed for the raw name;
//   2. the recognizer's suggestion when it is a catalog name, replaced by resolveFinest(raw)
//      when that is one of its descendants (model said 猪肉, title says 五花肉), or when the
//      suggestion is a parent entry and resolveFinest(raw) a different entry of the same
//      category (model said 猪肉, title says 梅花肉, which deliberately has no parent);
//   3. resolveFinest(raw);
//   4. unresolved: the caller collects these for one batched AI classification.
// The suggestion goes before the substring match because the model read the whole title.
class VocabularyResolver
{
public:
    explicit VocabularyResolver(const IngredientCatalog &catalog, const NameMappingStore *mappings = nullptr)
        : m_catalog(catalog), m_mappings(mappings)
    {
    }

    NameResolution resolve(const QString &rawName, const QString &suggested = {}) const;

    // Raw names of the unresolved entries, in order, without repeats (by NameMappingStore::key).
    static QStringList unresolvedNames(const QStringList &rawNames, const QList<NameResolution> &resolutions);

private:
    const IngredientCatalog &m_catalog;
    const NameMappingStore *m_mappings;
};

}
