#pragma once

#include <QString>

#include <optional>

namespace whilefresh::core {

enum class Storage { Fridge, Freezer, Room };

constexpr Storage allStorages[] = { Storage::Fridge, Storage::Freezer, Storage::Room };

// Stable keys used in the database and JSON: "fridge", "freezer", "room".
QString storageKey(Storage storage);
std::optional<Storage> storageFromKey(const QString &key);
// 冷藏 / 冷冻 / 常温
QString storageDisplayName(Storage storage);

// Shelf life in days for each storage; a missing value means "unknown / not suitable".
struct StorageDays {
    std::optional<int> fridge;
    std::optional<int> freezer;
    std::optional<int> room;

    std::optional<int> forStorage(Storage storage) const;
    void set(Storage storage, std::optional<int> days);
    bool isEmpty() const { return !fridge && !freezer && !room; }
    bool operator==(const StorageDays &other) const
    {
        return fridge == other.fridge && freezer == other.freezer && room == other.room;
    }
};

}
