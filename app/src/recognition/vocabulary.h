#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace whilefresh::recognition {

struct VocabularyEntry {
    QString name;
    QString category; // a category code when the vocabulary declares categories, else a display name
    QStringList aliases;
};

// What loading skipped. Only counts and vocabulary words are reported (also logged as a warning).
struct VocabularyLoadReport {
    int ignoredEntries = 0;  // empty or repeated standard names
    int ignoredAliases = 0;  // aliases already taken by a standard name or another entry's alias
    QStringList ignoredNames; // the offending names / aliases
};

// The fixed set of standard ingredient names the model may choose from (ADR-6).
//
// JSON format:
//   { "categories": { "vegetable": "蔬菜", "meat": "肉类", ... },      // optional, order matters
//     "items": [ { "name": "五花肉", "category": "meat", "aliases": ["猪五花"] }, ... ] }
// `category` is a code from `categories`. Without `categories`, `category` is used as the
// display name itself. Other fields (parent, pantry_default, bulk_default, ...) are ignored.
// A bare top-level array of items is accepted too.
class Vocabulary
{
public:
    static std::optional<Vocabulary> fromJson(const QByteArray &json, QString *error = nullptr,
                                              VocabularyLoadReport *report = nullptr);
    static std::optional<Vocabulary> fromFile(const QString &path, QString *error = nullptr,
                                              VocabularyLoadReport *report = nullptr);

    // Declares a category and its display name; declaration order is the prompt order.
    void addCategory(const QString &code, const QString &displayName);

    // Adds one entry (name first, then its aliases); ignored when the name is taken.
    bool add(const VocabularyEntry &entry);

    const QList<VocabularyEntry> &entries() const { return m_entries; }
    const QList<std::pair<QString, QString>> &categories() const { return m_categories; }
    bool isEmpty() const { return m_entries.isEmpty(); }

    // True for standard names only, not for aliases.
    bool contains(const QString &name) const { return !name.isEmpty() && m_byName.value(name) == name; }

    // Maps a standard name or an alias to the standard name; empty when unknown.
    QString resolve(const QString &nameOrAlias) const;

    QString categoryDisplayName(const QString &category) const;

    // Standard names grouped by category display name: declared categories in declaration
    // order, then undeclared ones in first-appearance order. Used for the prompt.
    QList<std::pair<QString, QStringList>> namesByCategory() const;

private:
    bool addName(const VocabularyEntry &entry);
    bool addAlias(const QString &alias, const QString &standardName);

    QList<VocabularyEntry> m_entries;
    QList<std::pair<QString, QString>> m_categories; // code -> display name, in order
    QHash<QString, QString> m_byName; // name or alias -> standard name
};

}
