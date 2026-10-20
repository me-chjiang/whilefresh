#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

namespace whilefresh::data {

// One SQLite connection plus the schema migrations. The schema version lives in
// PRAGMA user_version. Migrations are append-only: a released migration is never edited; a
// schema change is a new entry at the end of the list in database.cpp.
class Database
{
public:
    // `path` is a file path, or ":memory:" for tests.
    explicit Database(QString path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(QString *error = nullptr);
    // Applies migrations up to `targetVersion` (default: all). Each runs in its own transaction.
    bool migrate(QString *error = nullptr, int targetVersion = -1);
    int schemaVersion() const;
    static int latestVersion();

    QSqlDatabase handle() const { return QSqlDatabase::database(m_connectionName, false); }
    const QString &path() const { return m_path; }

    bool transaction();
    bool commit();
    void rollback();

private:
    QString m_path;
    QString m_connectionName;
};

}
