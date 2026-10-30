#pragma once

#include "core/clock.h"
#include "core/name-mapping.h"

namespace whilefresh::data {

class Database;

// NameMappingStore on the name_mappings table of migration v5:
//   name_mappings(raw_name TEXT PRIMARY KEY, canonical_name TEXT NOT NULL, confirmed_at TEXT NOT NULL)
// raw_name holds NameMappingStore::key(). Failures are logged; callers treat them as "not
// remembered". Store::deleteAllData() clears the table.
class SqliteNameMappingStore final : public core::NameMappingStore
{
public:
    SqliteNameMappingStore(Database &database, const core::Clock &clock) : m_database(database), m_clock(clock) {}

    QString mapped(const QString &rawName) const override;
    bool remember(const QString &rawName, const QString &canonical) override;
    QList<std::pair<QString, QString>> entries() const override;
    bool forget(const QString &rawName) override;
    bool clear() override;

private:
    Database &m_database;
    const core::Clock &m_clock;
};

}
