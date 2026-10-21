#include "core/storage.h"

namespace whilefresh::core {

QString storageKey(Storage storage)
{
    switch (storage) {
    case Storage::Fridge:
        return QStringLiteral("fridge");
    case Storage::Freezer:
        return QStringLiteral("freezer");
    case Storage::Room:
        return QStringLiteral("room");
    }
    return QStringLiteral("fridge");
}

std::optional<Storage> storageFromKey(const QString &key)
{
    for (Storage storage : allStorages) {
        if (storageKey(storage) == key)
            return storage;
    }
    return std::nullopt;
}

QString storageDisplayName(Storage storage)
{
    switch (storage) {
    case Storage::Fridge:
        return QStringLiteral("冷藏");
    case Storage::Freezer:
        return QStringLiteral("冷冻");
    case Storage::Room:
        return QStringLiteral("常温");
    }
    return {};
}

std::optional<int> StorageDays::forStorage(Storage storage) const
{
    switch (storage) {
    case Storage::Fridge:
        return fridge;
    case Storage::Freezer:
        return freezer;
    case Storage::Room:
        return room;
    }
    return std::nullopt;
}

void StorageDays::set(Storage storage, std::optional<int> days)
{
    switch (storage) {
    case Storage::Fridge:
        fridge = days;
        break;
    case Storage::Freezer:
        freezer = days;
        break;
    case Storage::Room:
        room = days;
        break;
    }
}

}
