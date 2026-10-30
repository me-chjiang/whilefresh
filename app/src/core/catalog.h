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
    static std::optional<IngredientCatalog> fromFile(const QString &path, QString *error = nullptr);

    void addCategory(const QString &code, const QString &displayName);
    bool add(const CatalogEntry &entry); // false when the name is empty or taken

    bool contains(const QString &name) const { return m_index.contains(name); }
    const CatalogEntry *entry(const QString &name) const;
    const QList<CatalogEntry> &entries() const { return m_entries; }

    // Standard name for a standard name or alias; empty when unknown.
    QString resolve(const QString &nameOrAlias) const;

    // Standard name for a product title such as "黑猪梅花肉切片 300g"; empty when nothing fits.
    // An exact name or alias wins. Otherwise the title is split into tokens at spaces,
    // punctuation and numbers, and every name or alias contained in a token is a candidate; a
    // one-character one only when it ends its token (藕 is not 藕粉). A candidate whose entry is
    // an ancestor of another candidate's is dropped (老抽 beats 酱油). Titles are head-final, so
    // within a token the match that ends last wins (牛肉干 beats 五香牛肉 in 五香牛肉干), then the
    // longer one; across tokens the longest match wins (速冻水饺 over 大葱 in "速冻水饺 猪肉大葱"),
    // then the earlier token.
    QString resolveFinest(const QString &title) const;

    QString categoryOf(const QString &name) const; // "other" when unknown
    QString categoryDisplayName(const QString &code) const;
    // Declared category codes in declaration order; used to order grouped lists.
    QStringList categoryOrder() const;
    int categoryRank(const QString &code) const; // position in categoryOrder(), unknown last

    // Parent chain from the direct parent upwards (cycles are cut).
    QStringList ancestors(const QString &name) const;
    // True when `name` is `requirement` itself or one of its descendants.
    bool satisfies(const QString &name, const QString &requirement) const;

    // The vocabulary for prompts, one block per category in declaration order:
    //   【肉类】
    //   猪肉 > 五花肉、前腿肉、里脊
    //   乌鸡、兔肉、排骨
    // A parent and its children share a line in the parent's category; entries without parent
    // or children follow on one line.
    QString hierarchyText() const;

private:
    QList<CatalogEntry> m_entries;
    QHash<QString, int> m_index;
    QHash<QString, QString> m_aliasToName;
    QList<std::pair<QString, QString>> m_categories;
};

}
