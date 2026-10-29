#include "core/fridge-item.h"

#include "core/texts.h"

namespace whilefresh::core {

QString itemStatusKey(ItemStatus status)
{
    switch (status) {
    case ItemStatus::InStock:
        return QStringLiteral("in_stock");
    case ItemStatus::Used:
        return QStringLiteral("used");
    case ItemStatus::Absent:
        return QStringLiteral("absent");
    case ItemStatus::Discarded:
        return QStringLiteral("discarded");
    }
    return QStringLiteral("in_stock");
}

std::optional<ItemStatus> itemStatusFromKey(const QString &key)
{
    for (ItemStatus status : { ItemStatus::InStock, ItemStatus::Used, ItemStatus::Absent, ItemStatus::Discarded }) {
        if (itemStatusKey(status) == key)
            return status;
    }
    return std::nullopt;
}

QString itemStatusLabel(ItemStatus status)
{
    switch (status) {
    case ItemStatus::InStock:
        return texts::statusInStock();
    case ItemStatus::Used:
        return texts::statusUsed();
    case ItemStatus::Absent:
        return texts::statusAbsent();
    case ItemStatus::Discarded:
        return texts::statusDiscarded();
    }
    return {};
}

QString itemKindKey(ItemKind kind)
{
    switch (kind) {
    case ItemKind::Food:
        return QStringLiteral("food");
    case ItemKind::StapleSeasoning:
        return QStringLiteral("staple_seasoning");
    case ItemKind::StapleFood:
        return QStringLiteral("staple_food");
    }
    return QStringLiteral("food");
}

std::optional<ItemKind> itemKindFromKey(const QString &key)
{
    for (ItemKind kind : { ItemKind::Food, ItemKind::StapleSeasoning, ItemKind::StapleFood }) {
        if (itemKindKey(kind) == key)
            return kind;
    }
    return std::nullopt;
}

QString dateOriginKey(DateOrigin origin)
{
    switch (origin) {
    case DateOrigin::None:
        return QStringLiteral("none");
    case DateOrigin::Visible:
        return QStringLiteral("visible");
    case DateOrigin::User:
        return QStringLiteral("user");
    }
    return QStringLiteral("none");
}

std::optional<DateOrigin> dateOriginFromKey(const QString &key)
{
    for (DateOrigin origin : { DateOrigin::None, DateOrigin::Visible, DateOrigin::User }) {
        if (dateOriginKey(origin) == key)
            return origin;
    }
    return std::nullopt;
}

ShelfLifeInput FridgeItem::shelfLifeInput() const
{
    ShelfLifeInput input;
    input.name = canonicalName;
    input.storage = storage;
    input.productionDate = productionDate.date;
    input.purchaseDate = purchaseDate.date;
    input.entryDate = entryDate;
    input.packageExpiry = packageExpiry.date;
    input.labelledDays = labelledDays;
    input.manualDays = manualDays;
    input.aiDays = aiDays;
    input.referenceConditionsConfirmed = referenceConditionsConfirmed;
    return input;
}

void FridgeItem::applyShelfLife(const ShelfLifeResolution &resolution)
{
    shelfLifeSource = resolution.source;
    shelfLifeDays = resolution.days;
    expiryDate = resolution.expiryDate;
    startBasis = resolution.startBasis;
    shelfLifeNote = resolution.note;
}

}
