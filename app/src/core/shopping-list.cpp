#include "core/shopping-list.h"

#include "core/texts.h"

#include <QHash>

#include <algorithm>

namespace whilefresh::core {

QString shoppingOriginKey(ShoppingOrigin origin)
{
    switch (origin) {
    case ShoppingOrigin::Recipe:
        return QStringLiteral("recipe");
    case ShoppingOrigin::Pantry:
        return QStringLiteral("pantry");
    case ShoppingOrigin::Manual:
        return QStringLiteral("manual");
    }
    return QStringLiteral("manual");
}

std::optional<ShoppingOrigin> shoppingOriginFromKey(const QString &key)
{
    for (ShoppingOrigin origin : { ShoppingOrigin::Recipe, ShoppingOrigin::Pantry, ShoppingOrigin::Manual }) {
        if (shoppingOriginKey(origin) == key)
            return origin;
    }
    return std::nullopt;
}

QString ShoppingEntry::displayName() const
{
    return names.join(texts::orSeparator());
}

QString ShoppingEntry::dedupeKey() const
{
    QStringList sorted = names;
    sorted.sort();
    return sorted.join(QChar(0x1f));
}

ShoppingEntry makeShoppingEntry(const QStringList &names, ShoppingOrigin origin, const QString &note,
                                const IngredientCatalog *catalog)
{
    ShoppingEntry entry;
    for (const QString &name : names) {
        const QString trimmed = name.trimmed();
        if (!trimmed.isEmpty() && !entry.names.contains(trimmed))
            entry.names.append(trimmed);
    }
    entry.category = catalog && !entry.names.isEmpty() ? catalog->categoryOf(entry.names.first())
                                                       : QStringLiteral("other");
    entry.origin = origin;
    entry.note = note;
    return entry;
}

QList<ShoppingSection> groupShoppingEntries(const QList<ShoppingEntry> &entries, const IngredientCatalog *catalog)
{
    QList<ShoppingSection> sections;
    QHash<QString, int> index;
    for (const ShoppingEntry &entry : entries) {
        auto it = index.constFind(entry.category);
        if (it == index.constEnd()) {
            it = index.insert(entry.category, sections.size());
            sections.append({ entry.category,
                              catalog ? catalog->categoryDisplayName(entry.category) : entry.category,
                              {} });
        }
        sections[*it].entries.append(entry);
    }
    std::stable_sort(sections.begin(), sections.end(), [catalog](const ShoppingSection &a, const ShoppingSection &b) {
        return catalog && catalog->categoryRank(a.category) < catalog->categoryRank(b.category);
    });
    return sections;
}

QString shoppingListText(const QList<ShoppingEntry> &entries, const IngredientCatalog *catalog)
{
    QStringList lines{ texts::shoppingListTitle() };
    for (const ShoppingSection &section : groupShoppingEntries(entries, catalog)) {
        lines.append(QString());
        lines.append(QStringLiteral("【%1】").arg(section.title));
        for (const ShoppingEntry &entry : section.entries)
            lines.append(QStringLiteral("- ") + entry.displayName());
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

}
