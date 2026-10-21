#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace whilefresh::core {

struct CatalogEntry {
    QString name;
    QString category; // category code, e.g. "vegetable"
    QStringList aliases;
    QString parent; // empty when none; "having this entry satisfies the parent"
    bool pantryDefault = false;
    bool bulkDefault = false;
};

// The standard ingredient vocabulary as the domain needs it (app/assets/data/vocabulary.json):
// names, categories, the parent hierarchy and the pantry / bulk defaults.
class IngredientCatalog
{
public:
    // Accepts "categories" as an array of {code, name} or as an object code -> name.
    static std::optional<IngredientCatalog> fromJson(const QByteArray &json, QString *error = nullptr);

    void addCategory(const QString &code, const QString &displayName);
    bool add(const CatalogEntry &entry); // false when the name is empty or taken

    bool contains(const QString &name) const { return m_index.contains(name); }
    const CatalogEntry *entry(const QString &name) const;
    const QList<CatalogEntry> &entries() const { return m_entries; }

    // Standard name for a standard name or alias; empty when unknown.
    QString resolve(const QString &nameOrAlias) const;

    QString categoryOf(const QString &name) const; // "other" when unknown
    QString categoryDisplayName(const QString &code) const;
    // Declared category codes in declaration order; used to order grouped lists.
    QStringList categoryOrder() const;
    int categoryRank(const QString &code) const; // position in categoryOrder(), unknown last

    // Parent chain from the direct parent upwards (cycles are cut).
    QStringList ancestors(const QString &name) const;
    // True when `name` is `requirement` itself or one of its descendants.
    bool satisfies(const QString &name, const QString &requirement) const;

private:
    QList<CatalogEntry> m_entries;
    QHash<QString, int> m_index;
    QHash<QString, QString> m_aliasToName;
    QList<std::pair<QString, QString>> m_categories;
};

}
