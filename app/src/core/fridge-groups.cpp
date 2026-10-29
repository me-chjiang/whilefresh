#include "core/fridge-groups.h"

#include <algorithm>
#include <tuple>

namespace whilefresh::core {

ItemKind classifyItemKind(const QString &canonicalName, bool recognizedAsSeasoning, Storage storage,
                          const IngredientCatalog *catalog, const ShelfLifeTable *reference)
{
    const CatalogEntry *entry = catalog ? catalog->entry(canonicalName) : nullptr;
    if (recognizedAsSeasoning || (entry && entry->category == QLatin1String("seasoning")))
        return ItemKind::StapleSeasoning;
    if (!entry || entry->category != QLatin1String("staple"))
        return ItemKind::Food;
    if (entry->pantryDefault || entry->bulkDefault)
        return ItemKind::StapleFood;
    if (storage != Storage::Room)
        return ItemKind::Food;
    const auto hit = reference ? reference->lookup(canonicalName, Storage::Room, catalog) : std::nullopt;
    return hit && hit->days < StapleRules::minimumRoomDays ? ItemKind::Food : ItemKind::StapleFood;
}

FridgeGroup fridgeGroup(const FridgeItem &item)
{
    switch (item.kind) {
    case ItemKind::StapleSeasoning:
        return FridgeGroup::StapleSeasoning;
    case ItemKind::StapleFood:
        return FridgeGroup::StapleFood;
    case ItemKind::Food:
        break;
    }
    switch (item.storage) {
    case Storage::Fridge:
        return FridgeGroup::Fridge;
    case Storage::Freezer:
        return FridgeGroup::Freezer;
    case Storage::Room:
        return FridgeGroup::Room;
    }
    return FridgeGroup::Fridge;
}

QString fridgeGroupName(FridgeGroup group)
{
    switch (group) {
    case FridgeGroup::Fridge:
        return storageDisplayName(Storage::Fridge);
    case FridgeGroup::Freezer:
        return storageDisplayName(Storage::Freezer);
    case FridgeGroup::Room:
        return storageDisplayName(Storage::Room);
    case FridgeGroup::StapleSeasoning:
        return QStringLiteral("常备调料");
    case FridgeGroup::StapleFood:
        return QStringLiteral("常备食材");
    }
    return {};
}

QList<FridgeItem> sortForFridge(QList<FridgeItem> items, const QDate &today)
{
    const auto key = [&today](const FridgeItem &item) {
        const auto remaining = item.freshness(today).remainingDays;
        const QString name = item.displayName.isEmpty() ? item.canonicalName : item.displayName;
        return std::make_tuple(int(fridgeGroup(item)), !remaining.has_value(), remaining.value_or(0), name, item.id);
    };
    std::sort(items.begin(), items.end(), [&key](const FridgeItem &a, const FridgeItem &b) { return key(a) < key(b); });
    return items;
}

}
