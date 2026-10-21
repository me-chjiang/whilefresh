#include "core/catalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace whilefresh::core {

std::optional<IngredientCatalog> IngredientCatalog::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull()) {
        if (error)
            *error = QStringLiteral("vocabulary: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    const QJsonValue categories = root.value(QStringLiteral("categories"));

    IngredientCatalog catalog;
    if (categories.isArray()) {
        for (const QJsonValue &value : categories.toArray()) {
            const QJsonObject object = value.toObject();
            catalog.addCategory(object.value(QStringLiteral("code")).toString(),
                                object.value(QStringLiteral("name")).toString());
        }
    } else if (categories.isObject()) {
        const QJsonObject object = categories.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            catalog.addCategory(it.key(), it.value().toString());
    }

    const QJsonArray items = doc.isArray() ? doc.array() : root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        CatalogEntry entry;
        entry.name = object.value(QStringLiteral("name")).toString().trimmed();
        entry.category = object.value(QStringLiteral("category")).toString().trimmed();
        for (const QJsonValue &alias : object.value(QStringLiteral("aliases")).toArray())
            entry.aliases.append(alias.toString().trimmed());
        entry.parent = object.value(QStringLiteral("parent")).toString().trimmed();
        entry.pantryDefault = object.value(QStringLiteral("pantry_default")).toBool();
        entry.bulkDefault = object.value(QStringLiteral("bulk_default")).toBool();
        catalog.add(entry);
    }
    if (catalog.m_entries.isEmpty()) {
        if (error)
            *error = QStringLiteral("vocabulary: no items");
        return std::nullopt;
    }
    return catalog;
}

void IngredientCatalog::addCategory(const QString &code, const QString &displayName)
{
    if (code.isEmpty())
        return;
    for (const auto &category : std::as_const(m_categories)) {
        if (category.first == code)
            return;
    }
    m_categories.append({ code, displayName.isEmpty() ? code : displayName });
}

bool IngredientCatalog::add(const CatalogEntry &entry)
{
    if (entry.name.isEmpty() || m_index.contains(entry.name))
        return false;
    m_index.insert(entry.name, m_entries.size());
    m_entries.append(entry);
    for (const QString &alias : entry.aliases) {
        if (!alias.isEmpty() && !m_aliasToName.contains(alias))
            m_aliasToName.insert(alias, entry.name);
    }
    return true;
}

const CatalogEntry *IngredientCatalog::entry(const QString &name) const
{
    const auto it = m_index.constFind(name);
    return it == m_index.constEnd() ? nullptr : &m_entries.at(*it);
}

QString IngredientCatalog::resolve(const QString &nameOrAlias) const
{
    const QString trimmed = nameOrAlias.trimmed();
    if (m_index.contains(trimmed))
        return trimmed;
    return m_aliasToName.value(trimmed);
}

QString IngredientCatalog::categoryOf(const QString &name) const
{
    const CatalogEntry *found = entry(name);
    return found && !found->category.isEmpty() ? found->category : QStringLiteral("other");
}

QString IngredientCatalog::categoryDisplayName(const QString &code) const
{
    for (const auto &category : m_categories) {
        if (category.first == code)
            return category.second;
    }
    return code == QStringLiteral("other") ? QStringLiteral("其他") : code;
}

QStringList IngredientCatalog::categoryOrder() const
{
    QStringList codes;
    for (const auto &category : m_categories)
        codes.append(category.first);
    return codes;
}

int IngredientCatalog::categoryRank(const QString &code) const
{
    for (int i = 0; i < m_categories.size(); ++i) {
        if (m_categories.at(i).first == code)
            return i;
    }
    return m_categories.size();
}

QStringList IngredientCatalog::ancestors(const QString &name) const
{
    QStringList chain;
    QSet<QString> seen{ name };
    const CatalogEntry *current = entry(name);
    while (current && !current->parent.isEmpty() && !seen.contains(current->parent)) {
        chain.append(current->parent);
        seen.insert(current->parent);
        current = entry(current->parent);
    }
    return chain;
}

bool IngredientCatalog::satisfies(const QString &name, const QString &requirement) const
{
    return name == requirement || ancestors(name).contains(requirement);
}

}
