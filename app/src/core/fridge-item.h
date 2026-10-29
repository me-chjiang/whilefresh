#pragma once

#include "core/freshness.h"
#include "core/shelf-life.h"
#include "core/storage.h"

#include <QDate>
#include <QDateTime>
#include <QString>

#include <optional>

namespace whilefresh::core {

enum class ItemStatus { InStock, Used, Absent, Discarded };
QString itemStatusKey(ItemStatus status);
std::optional<ItemStatus> itemStatusFromKey(const QString &key);
QString itemStatusLabel(ItemStatus status);

// Food is grouped by storage; staples get their own fridge groups (see staples.h).
enum class ItemKind { Food, StapleSeasoning, StapleFood };
QString itemKindKey(ItemKind kind); // "food", "staple_seasoning", "staple_food"
std::optional<ItemKind> itemKindFromKey(const QString &key);

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
    ItemKind kind = ItemKind::Food;
    // Total in stock parsed from quantityText x count (see quantity.h); 0 / empty when unknown.
    double amount = 0.0;
    QString unit;
    Storage storage = Storage::Fridge;
    StorageSource storageSource = StorageSource::Default;
    bool useFirst = false;
    ItemStatus status = ItemStatus::InStock;
    QDateTime statusChangedAt;
    QDate snoozedUntil; // "忽略" an expiring item: no reminder before this day

    RecordedDate purchaseDate;
    QDate entryDate;
    RecordedDate productionDate;
    RecordedDate packageExpiry;

    std::optional<int> labelledDays;
    std::optional<int> manualDays;
    StorageDays aiDays;
    bool referenceConditionsConfirmed = false;

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
    bool isSnoozed(const QDate &today) const { return snoozedUntil.isValid() && today < snoozedUntil; }
    // Staples keep their place even without a shelf life; the user is asked to enter one (待录保质期).
    bool needsShelfLife() const { return kind != ItemKind::Food && !expiryDate.isValid(); }
};

}
