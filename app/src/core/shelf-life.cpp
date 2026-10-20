#include "core/shelf-life.h"

#include "core/texts.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace whilefresh::core {

namespace {

const QString uncategorized = QStringLiteral("未归类");

std::optional<int> positiveDays(const QJsonValue &value)
{
    if (!value.isDouble())
        return std::nullopt;
    const int days = value.toInt();
    return days > 0 ? std::optional<int>(days) : std::nullopt;
}

ShelfLifeReference referenceFromJson(const QJsonObject &object)
{
    ShelfLifeReference reference;
    reference.days.fridge = positiveDays(object.value(QStringLiteral("fridge_days")));
    reference.days.freezer = positiveDays(object.value(QStringLiteral("freezer_days")));
    reference.days.room = positiveDays(object.value(QStringLiteral("room_days")));
    reference.source = object.value(QStringLiteral("source")).toString();
    const QJsonValue id = object.value(QStringLiteral("foodkeeper_id"));
    reference.foodkeeperId = id.isDouble() ? QString::number(id.toInteger()) : id.toString();
    reference.basis = object.value(QStringLiteral("basis")).toString();
    return reference;
}

QString startBasisNote(StartBasis basis)
{
    switch (basis) {
    case StartBasis::ProductionDate:
        return QStringLiteral("从生产日期起算");
    case StartBasis::PurchaseDate:
        return QStringLiteral("从购买日期起算");
    case StartBasis::EntryDate:
        return QStringLiteral("从入库日期起算");
    case StartBasis::None:
        break;
    }
    return {};
}

}

QString shelfLifeSourceKey(ShelfLifeSource source)
{
    switch (source) {
    case ShelfLifeSource::None:
        return QStringLiteral("none");
    case ShelfLifeSource::Package:
        return QStringLiteral("package");
    case ShelfLifeSource::Manual:
        return QStringLiteral("manual");
    case ShelfLifeSource::LocalRule:
        return QStringLiteral("local_rule");
    case ShelfLifeSource::Reference:
        return QStringLiteral("reference");
    case ShelfLifeSource::AiEstimate:
        return QStringLiteral("ai_estimate");
    }
    return QStringLiteral("none");
}

std::optional<ShelfLifeSource> shelfLifeSourceFromKey(const QString &key)
{
    for (ShelfLifeSource source : { ShelfLifeSource::None, ShelfLifeSource::Package, ShelfLifeSource::Manual,
                                    ShelfLifeSource::LocalRule, ShelfLifeSource::Reference,
                                    ShelfLifeSource::AiEstimate }) {
        if (shelfLifeSourceKey(source) == key)
            return source;
    }
    return std::nullopt;
}

QString shelfLifeSourceLabel(ShelfLifeSource source)
{
    switch (source) {
    case ShelfLifeSource::None:
        return texts::sourceNone();
    case ShelfLifeSource::Package:
        return texts::sourcePackage();
    case ShelfLifeSource::Manual:
        return texts::sourceManual();
    case ShelfLifeSource::LocalRule:
        return texts::sourceLocalRule();
    case ShelfLifeSource::Reference:
        return texts::sourceReference();
    case ShelfLifeSource::AiEstimate:
        return texts::sourceAiEstimate();
    }
    return texts::sourceNone();
}

QString startBasisKey(StartBasis basis)
{
    switch (basis) {
    case StartBasis::None:
        return QStringLiteral("none");
    case StartBasis::ProductionDate:
        return QStringLiteral("production");
    case StartBasis::PurchaseDate:
        return QStringLiteral("purchase");
    case StartBasis::EntryDate:
        return QStringLiteral("entry");
    }
    return QStringLiteral("none");
}

std::optional<StartBasis> startBasisFromKey(const QString &key)
{
    for (StartBasis basis : { StartBasis::None, StartBasis::ProductionDate, StartBasis::PurchaseDate,
                              StartBasis::EntryDate }) {
        if (startBasisKey(basis) == key)
            return basis;
    }
    return std::nullopt;
}

std::optional<ShelfLifeTable> ShelfLifeTable::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("shelf-life: %1").arg(doc.isNull() ? parseError.errorString()
                                                                         : QStringLiteral("not an object"));
        return std::nullopt;
    }
    ShelfLifeTable table;
    const QJsonObject root = doc.object();
    const QJsonValue items = root.value(QStringLiteral("items"));
    if (items.isArray()) {
        for (const QJsonValue &value : items.toArray()) {
            const QJsonObject object = value.toObject();
            const QString name = object.value(QStringLiteral("name")).toString().trimmed();
            if (!name.isEmpty())
                table.insert(name, referenceFromJson(object));
        }
        return table;
    }
    const QJsonObject rows = items.isObject() ? items.toObject() : root;
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        if (it.value().isObject())
            table.insert(it.key().trimmed(), referenceFromJson(it.value().toObject()));
    }
    return table;
}

std::optional<ShelfLifeTable::Hit> ShelfLifeTable::lookup(const QString &name, Storage storage,
                                                          const IngredientCatalog *catalog) const
{
    QStringList chain{ name };
    if (catalog)
        chain += catalog->ancestors(name);
    for (const QString &candidate : chain) {
        const auto it = m_rows.constFind(candidate);
        if (it == m_rows.constEnd())
            continue;
        if (const auto days = it->days.forStorage(storage))
            return Hit{ *days, candidate, *it };
    }
    return std::nullopt;
}

const ShelfLifeReference *ShelfLifeTable::row(const QString &name, const IngredientCatalog *catalog,
                                              QString *matchedName) const
{
    QStringList chain{ name };
    if (catalog)
        chain += catalog->ancestors(name);
    for (const QString &candidate : chain) {
        const auto it = m_rows.constFind(candidate);
        if (it != m_rows.constEnd() && !it->days.isEmpty()) {
            if (matchedName)
                *matchedName = candidate;
            return &*it;
        }
    }
    return nullptr;
}

QString RuleBook::key(const QString &name, Storage storage)
{
    return name + QChar(0x1f) + storageKey(storage);
}

std::optional<int> RuleBook::shelfLifeDays(const QString &name, Storage storage) const
{
    const auto it = m_days.constFind(key(name, storage));
    return it == m_days.constEnd() ? std::nullopt : std::optional<int>(*it);
}

std::optional<Storage> RuleBook::storage(const QString &name) const
{
    const auto it = m_storage.constFind(name);
    return it == m_storage.constEnd() ? std::nullopt : std::optional<Storage>(*it);
}

std::optional<int> AiEstimateCache::days(const QString &name, Storage storage) const
{
    const auto it = m_days.constFind(RuleBook::key(name, storage));
    return it == m_days.constEnd() ? std::nullopt : std::optional<int>(*it);
}

bool isRuleEligibleName(const QString &name)
{
    return !name.trimmed().isEmpty() && name != uncategorized;
}

ShelfLifeResolution resolveShelfLife(const ShelfLifeInput &input, const ShelfLifeContext &context)
{
    ShelfLifeResolution result;
    if (input.productionDate.isValid()) {
        result.startBasis = StartBasis::ProductionDate;
        result.startDate = input.productionDate;
    } else if (input.purchaseDate.isValid()) {
        result.startBasis = StartBasis::PurchaseDate;
        result.startDate = input.purchaseDate;
    } else if (input.entryDate.isValid()) {
        result.startBasis = StartBasis::EntryDate;
        result.startDate = input.entryDate;
    }

    const bool eligible = isRuleEligibleName(input.name);
    const QString storageName = storageDisplayName(input.storage);

    // A manual override beats everything, including the package expiry it was meant to correct.
    if (!input.manualDays && input.packageExpiry.isValid()) {
        result.source = ShelfLifeSource::Package;
        result.expiryDate = input.packageExpiry;
        if (result.startDate.isValid())
            result.days = std::max(1, static_cast<int>(result.startDate.daysTo(input.packageExpiry)));
        result.note = QStringLiteral("包装到期日 %1").arg(input.packageExpiry.toString(Qt::ISODate));
        return result;
    }

    std::optional<int> days;
    if (input.manualDays && *input.manualDays > 0) {
        days = input.manualDays;
        result.source = ShelfLifeSource::Manual;
        result.note = QStringLiteral("你为这件食材设置的 %1 天").arg(*days);
    } else if (input.labelledDays && *input.labelledDays > 0) {
        days = input.labelledDays;
        result.source = ShelfLifeSource::Package;
        result.note = QStringLiteral("包装标注保质期 %1 天").arg(*days);
    } else if (eligible && context.rules && (days = context.rules->shelfLifeDays(input.name, input.storage))) {
        result.source = ShelfLifeSource::LocalRule;
        result.note = QStringLiteral("你确认过：%1%2 %3 天").arg(input.name, storageName).arg(*days);
    } else if (eligible && context.table) {
        if (const auto hit = context.table->lookup(input.name, input.storage, context.catalog)) {
            days = hit->days;
            result.source = ShelfLifeSource::Reference;
            const QString basis = hit->matchedName == input.name
                ? QString()
                : QStringLiteral("（按「%1」）").arg(hit->matchedName);
            const QString attribution = hit->reference.source.isEmpty()
                ? texts::sourceReference()
                : QStringLiteral("参考：%1").arg(hit->reference.source);
            result.note = QStringLiteral("%1%2，%3 %4 天").arg(attribution, basis, storageName).arg(*days);
        }
    }

    if (!days) {
        std::optional<int> cached;
        if (eligible && context.aiCache)
            cached = context.aiCache->days(input.name, input.storage);
        if (cached) {
            days = cached;
        } else if (const auto fresh = input.aiDays.forStorage(input.storage); fresh && *fresh > 0) {
            days = fresh;
        }
        if (days) {
            result.source = ShelfLifeSource::AiEstimate;
            result.note = QStringLiteral("AI 估计%1 %2 天").arg(storageName).arg(*days);
        }
        if (eligible) {
            // Cache every storage the first time this food shows up, so a later storage change or
            // the next import of the same food reuses the same numbers.
            for (Storage storage : allStorages) {
                const auto value = input.aiDays.forStorage(storage);
                if (value && *value > 0 && !(context.aiCache && context.aiCache->days(input.name, storage)))
                    result.newAiEstimates.append({ input.name, storage, *value });
            }
        }
    }

    if (!days) {
        result.source = ShelfLifeSource::None;
        result.note = texts::freshnessUnknownHint();
        return result;
    }
    result.days = days;
    if (result.startDate.isValid()) {
        result.expiryDate = result.startDate.addDays(*days);
        result.note += QStringLiteral("，") + startBasisNote(result.startBasis);
    }
    return result;
}

QString storageSourceKey(StorageSource source)
{
    switch (source) {
    case StorageSource::Default:
        return QStringLiteral("default");
    case StorageSource::LocalRule:
        return QStringLiteral("local_rule");
    case StorageSource::Reference:
        return QStringLiteral("reference");
    case StorageSource::AiEstimate:
        return QStringLiteral("ai_estimate");
    }
    return QStringLiteral("default");
}

std::optional<StorageSource> storageSourceFromKey(const QString &key)
{
    for (StorageSource source : { StorageSource::Default, StorageSource::LocalRule, StorageSource::Reference,
                                  StorageSource::AiEstimate }) {
        if (storageSourceKey(source) == key)
            return source;
    }
    return std::nullopt;
}

StorageSuggestion suggestStorage(const QString &name, std::optional<Storage> aiStorage, bool isSeasoning,
                                 const ShelfLifeContext &context)
{
    const bool eligible = isRuleEligibleName(name);
    if (eligible && context.rules) {
        if (const auto storage = context.rules->storage(name))
            return { *storage, StorageSource::LocalRule };
    }
    if (eligible && context.table) {
        if (const ShelfLifeReference *row = context.table->row(name, context.catalog)) {
            if (aiStorage && row->days.forStorage(*aiStorage))
                return { *aiStorage, StorageSource::Reference };
            int available = 0;
            Storage only = Storage::Fridge;
            for (Storage storage : allStorages) {
                if (row->days.forStorage(storage)) {
                    ++available;
                    only = storage;
                }
            }
            if (available == 1)
                return { only, StorageSource::Reference };
            for (Storage storage : { Storage::Fridge, Storage::Room, Storage::Freezer }) {
                if (row->days.forStorage(storage))
                    return { storage, StorageSource::Reference };
            }
        }
    }
    if (aiStorage)
        return { *aiStorage, StorageSource::AiEstimate };
    return { isSeasoning ? Storage::Room : Storage::Fridge, StorageSource::Default };
}

std::optional<RuleProposal> proposeShelfLifeRule(const QString &name, Storage storage, int days)
{
    if (!isRuleEligibleName(name) || days <= 0)
        return std::nullopt;
    RuleProposal proposal;
    proposal.kind = RuleProposal::Kind::ShelfLife;
    proposal.name = name;
    proposal.storage = storage;
    proposal.days = days;
    proposal.question = texts::ruleQuestionShelfLife(name, storageDisplayName(storage), days);
    return proposal;
}

std::optional<RuleProposal> proposeStorageRule(const QString &name, Storage storage)
{
    if (!isRuleEligibleName(name))
        return std::nullopt;
    RuleProposal proposal;
    proposal.kind = RuleProposal::Kind::Storage;
    proposal.name = name;
    proposal.storage = storage;
    proposal.question = texts::ruleQuestionStorage(name, storageDisplayName(storage));
    return proposal;
}

}
