#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <algorithm>
#include <utility>

namespace whilefresh::core {

// Raw product names the user confirmed for a standard name (PRD v6: after the user keeps an AI
// classification or corrects a name, the same raw name resolves directly next time).
class NameMappingStore
{
public:
    virtual ~NameMappingStore() = default;

    // Standard name remembered for `rawName`; empty when none.
    virtual QString mapped(const QString &rawName) const = 0;
    virtual bool remember(const QString &rawName, const QString &canonical) = 0;
    // Everything remembered as (key, standard name), sorted by key; for the page that lets the
    // user forget wrong guesses.
    virtual QList<std::pair<QString, QString>> entries() const = 0;
    virtual bool forget(const QString &rawName) = 0;
    virtual bool clear() = 0;

    // Lookup key: the raw name trimmed, inner whitespace collapsed.
    static QString key(const QString &rawName) { return rawName.simplified(); }
};

// For tests and sessions that must not persist anything.
class MemoryNameMappingStore final : public NameMappingStore
{
public:
    QString mapped(const QString &rawName) const override { return m_names.value(key(rawName)); }
    bool remember(const QString &rawName, const QString &canonical) override
    {
        const QString k = key(rawName);
        if (k.isEmpty() || canonical.isEmpty())
            return false;
        m_names.insert(k, canonical);
        return true;
    }
    QList<std::pair<QString, QString>> entries() const override
    {
        QList<std::pair<QString, QString>> list;
        for (auto it = m_names.cbegin(); it != m_names.cend(); ++it)
            list.append({ it.key(), it.value() });
        std::sort(list.begin(), list.end());
        return list;
    }
    bool forget(const QString &rawName) override { return m_names.remove(key(rawName)) > 0; }
    bool clear() override
    {
        m_names.clear();
        return true;
    }
    qsizetype size() const { return m_names.size(); }

private:
    QHash<QString, QString> m_names;
};

}
