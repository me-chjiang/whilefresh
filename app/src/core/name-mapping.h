#pragma once

#include <QHash>
#include <QString>

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
    qsizetype size() const { return m_names.size(); }

private:
    QHash<QString, QString> m_names;
};

}
