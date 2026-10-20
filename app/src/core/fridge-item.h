#pragma once

#include "core/freshness.h"
#include "core/shelf-life.h"
#include "core/storage.h"

#include <QDate>
#include <QDateTime>
#include <QString>

#include <optional>

namespace whilefresh::core {

enum class ItemStatus { InStock, Used, Absent };
QString itemStatusKey(ItemStatus status);
std::optional<ItemStatus> itemStatusFromKey(const QString &key);
QString itemStatusLabel(ItemStatus status);

// Who supplied a date: visible on the material, or typed by the user. Entry dates are always
// automatic and have no source.
enum class DateOrigin { None, Visible, User };
QString dateOriginKey(DateOrigin origin);
std::optional<DateOrigin> dateOriginFromKey(const QString &key);

struct RecordedDate {
    QDate date;
    DateOrigin origin = DateOrigin::None;
    QString text; // verbatim text from recognition, kept even when it could not be parsed

    bool isSet() const { return date.isValid(); }
};

// One thing in the fridge (or cupboard). The entry date is when it was imported and is never
// used as its production date.
struct FridgeItem {
    qint64 id = 0;
    qint64 importId = 0; // 0 for manually added items
    QString canonicalName; // standard name or "未归类"
    QString displayName;
    QString rawName;       // as recognized, for reference
    QString category;      // category code
    QString quantityText;  // "500g", "10枚"
    int count = 1;
    Storage storage = Storage::Fridge;
    StorageSource storageSource = StorageSource::Default;
    bool useFirst = false;
    ItemStatus status = ItemStatus::InStock;
    QDateTime statusChangedAt;

    RecordedDate purchaseDate;
    QDate entryDate;
    RecordedDate productionDate;
    RecordedDate packageExpiry;

    std::optional<int> labelledDays;
    std::optional<int> manualDays;
    StorageDays aiDays;

    // Result of the source chain at the last edit; not recomputed when rules change later.
    ShelfLifeSource shelfLifeSource = ShelfLifeSource::None;
    std::optional<int> shelfLifeDays;
    QDate expiryDate;
    StartBasis startBasis = StartBasis::None;
    QString shelfLifeNote;

    QDateTime createdAt;
    QDateTime updatedAt;

    ShelfLifeInput shelfLifeInput() const;
    void applyShelfLife(const ShelfLifeResolution &resolution);
    FreshnessState freshness(const QDate &today) const
    {
        return evaluateFreshness(expiryDate, shelfLifeDays, today);
    }
};

}
