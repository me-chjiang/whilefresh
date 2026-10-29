#pragma once

#include "core/clock.h"
#include "core/name-mapping.h"

namespace whilefresh::data {

class Database;

// NameMappingStore on the name_mappings table of migration v5 (T15):
//   name_mappings(raw_name TEXT PRIMARY KEY, canonical TEXT NOT NULL, confirmed_at TEXT)
// raw_name holds NameMappingStore::key(). Until that migration exists in the database, lookups
// find nothing and remember() fails with a logged warning; callers treat both as "not
// remembered".
class SqliteNameMappingStore final : public core::NameMappingStore
{
public:
    SqliteNameMappingStore(Database &database, const core::Clock &clock) : m_database(database), m_clock(clock) {}

    QString mapped(const QString &rawName) const override;
    bool remember(const QString &rawName, const QString &canonical) override;

private:
    Database &m_database;
    const core::Clock &m_clock;
};

}
