#pragma once

#include "core/catalog.h"
#include "core/fridge-item.h"
#include "core/shelf-life.h"

#include <QDate>
#include <QList>
#include <QString>

namespace whilefresh::core {

// Which staples get their own group instead of joining the storage groups.
struct StapleRules {
    // A "staple" category entry whose reference room-temperature life is shorter than this
    // (bread, 7 days) is everyday food, not a 常备食材.
    static constexpr int minimumRoomDays = 30;
};

// Seasonings (recognized as such, or in the vocabulary's "seasoning" category) are
// StapleSeasoning. Entries of the "staple" category (主食粮油) are StapleFood when the
// vocabulary marks them pantry / bulk (rice, flour, oil, starch), or when they are kept at room
// temperature and the reference table does not give them less than minimumRoomDays there
// (noodles, dried beans, grains). Everything else is Food. `reference` may be null.
ItemKind classifyItemKind(const QString &canonicalName, bool recognizedAsSeasoning, Storage storage,
                          const IngredientCatalog *catalog, const ShelfLifeTable *reference);

// 冷藏 / 冷冻 / 常温 / 常备调料 / 常备食材, in this display order.
enum class FridgeGroup { Fridge, Freezer, Room, StapleSeasoning, StapleFood };

FridgeGroup fridgeGroup(const FridgeItem &item); // staples by kind, food by storage
QString fridgeGroupName(FridgeGroup group);

// Display order: by group, then expired first, then days left ascending, unknown last; ties by
// display name, then id, so the list never reshuffles between refreshes.
QList<FridgeItem> sortForFridge(QList<FridgeItem> items, const QDate &today);

}
