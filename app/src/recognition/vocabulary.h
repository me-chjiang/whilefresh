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
    QString category;
    QStringList aliases;
};

// The fixed set of standard ingredient names the model may choose from (ADR-6).
//
// JSON format (extra fields are ignored, so the full vocabulary.json can be loaded as is):
//   { "items": [ { "name": "五花肉", "category": "肉类", "aliases": ["猪五花", "带皮五花"] }, ... ] }
// A bare top-level array of the same objects is accepted too.
class Vocabulary
{
public:
    static std::optional<Vocabulary> fromJson(const QByteArray &json, QString *error = nullptr);
    static std::optional<Vocabulary> fromFile(const QString &path, QString *error = nullptr);

    // Adds an entry; ignored when the name (or an alias) is already taken. Returns success.
    bool add(const VocabularyEntry &entry);

    const QList<VocabularyEntry> &entries() const { return m_entries; }
    bool isEmpty() const { return m_entries.isEmpty(); }

    // True for standard names only, not for aliases.
    bool contains(const QString &name) const { return !name.isEmpty() && m_byName.value(name) == name; }

    // Maps a standard name or an alias to the standard name; empty when unknown.
    QString resolve(const QString &nameOrAlias) const;

    // Names grouped by category, in first-appearance order. Used for the prompt.
    QList<std::pair<QString, QStringList>> namesByCategory() const;

private:
    QList<VocabularyEntry> m_entries;
    QHash<QString, QString> m_byName; // name or alias -> standard name
};

}
