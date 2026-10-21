#include "data/database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace whilefresh::data {

namespace {

// Append-only. Never edit an entry once it has shipped; add a new one instead.
const QList<QStringList> &migrations()
{
    static const QList<QStringList> list{
        // 1: imports, items, rules, AI cache, pantry overrides, suggestions, shopping list.
        {
            QStringLiteral(R"(CREATE TABLE imports (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                created_at TEXT NOT NULL,
                input_kind TEXT NOT NULL,
                origin TEXT NOT NULL DEFAULT '',
                is_demo INTEGER NOT NULL DEFAULT 0,
                truncated INTEGER NOT NULL DEFAULT 0,
                text_length INTEGER NOT NULL DEFAULT 0
            ))"),
            QStringLiteral(R"(CREATE TABLE import_images (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                import_id INTEGER NOT NULL REFERENCES imports(id) ON DELETE CASCADE,
                position INTEGER NOT NULL,
                path TEXT NOT NULL,
                sha256 TEXT NOT NULL,
                mime_type TEXT NOT NULL DEFAULT '',
                captured_at TEXT,
                captured_at_source TEXT NOT NULL DEFAULT ''
            ))"),
            QStringLiteral("CREATE INDEX import_images_sha256 ON import_images(sha256)"),
            QStringLiteral(R"(CREATE TABLE items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                import_id INTEGER REFERENCES imports(id) ON DELETE CASCADE,
                canonical_name TEXT NOT NULL,
                display_name TEXT NOT NULL,
                raw_name TEXT NOT NULL DEFAULT '',
                category TEXT NOT NULL DEFAULT 'other',
                quantity_text TEXT NOT NULL DEFAULT '',
                count INTEGER NOT NULL DEFAULT 1,
                storage TEXT NOT NULL,
                storage_source TEXT NOT NULL DEFAULT 'default',
                use_first INTEGER NOT NULL DEFAULT 0,
                status TEXT NOT NULL DEFAULT 'in_stock',
                status_changed_at TEXT,
                purchase_date TEXT,
                purchase_date_origin TEXT NOT NULL DEFAULT 'none',
                purchase_date_text TEXT NOT NULL DEFAULT '',
                entry_date TEXT NOT NULL,
                production_date TEXT,
                production_date_origin TEXT NOT NULL DEFAULT 'none',
                production_date_text TEXT NOT NULL DEFAULT '',
                package_expiry_date TEXT,
                package_expiry_origin TEXT NOT NULL DEFAULT 'none',
                package_expiry_text TEXT NOT NULL DEFAULT '',
                labelled_days INTEGER,
                manual_days INTEGER,
                ai_fridge_days INTEGER,
                ai_freezer_days INTEGER,
                ai_room_days INTEGER,
                shelf_life_source TEXT NOT NULL DEFAULT 'none',
                shelf_life_days INTEGER,
                expiry_date TEXT,
                start_basis TEXT NOT NULL DEFAULT 'none',
                shelf_life_note TEXT NOT NULL DEFAULT '',
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL
            ))"),
            QStringLiteral(R"(CREATE TABLE shelf_life_rules (
                canonical_name TEXT NOT NULL,
                storage TEXT NOT NULL,
                days INTEGER NOT NULL,
                updated_at TEXT NOT NULL,
                PRIMARY KEY (canonical_name, storage)
            ))"),
            QStringLiteral(R"(CREATE TABLE storage_rules (
                canonical_name TEXT PRIMARY KEY,
                storage TEXT NOT NULL,
                updated_at TEXT NOT NULL
            ))"),
            QStringLiteral(R"(CREATE TABLE ai_shelf_life_cache (
                canonical_name TEXT NOT NULL,
                storage TEXT NOT NULL,
                days INTEGER NOT NULL,
                created_at TEXT NOT NULL,
                PRIMARY KEY (canonical_name, storage)
            ))"),
            QStringLiteral(R"(CREATE TABLE pantry_overrides (
                canonical_name TEXT PRIMARY KEY,
                is_pantry INTEGER,
                is_bulk INTEGER,
                is_out INTEGER NOT NULL DEFAULT 0,
                updated_at TEXT NOT NULL
            ))"),
            QStringLiteral(R"(CREATE TABLE suggestions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                kind TEXT NOT NULL,
                canonical_name TEXT NOT NULL,
                message TEXT NOT NULL,
                import_id INTEGER REFERENCES imports(id) ON DELETE SET NULL,
                status TEXT NOT NULL DEFAULT 'pending',
                created_at TEXT NOT NULL,
                resolved_at TEXT
            ))"),
            QStringLiteral(R"(CREATE TABLE shopping_items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                names TEXT NOT NULL,
                dedupe_key TEXT NOT NULL UNIQUE,
                category TEXT NOT NULL DEFAULT 'other',
                origin TEXT NOT NULL,
                note TEXT NOT NULL DEFAULT '',
                created_at TEXT NOT NULL
            ))"),
        },
        // 2: app settings (demo mode, privacy notice acknowledged, ...) and lookup indexes.
        {
            QStringLiteral(R"(CREATE TABLE settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            ))"),
            QStringLiteral("CREATE INDEX items_import_id ON items(import_id)"),
            QStringLiteral("CREATE INDEX items_status ON items(status)"),
            QStringLiteral("CREATE INDEX suggestions_status ON suggestions(status)"),
        },
        // 3: immutable purchase fingerprints and retryable post-commit file cleanup.
        {
            QStringLiteral(R"(CREATE TABLE import_products (
                import_id INTEGER NOT NULL REFERENCES imports(id) ON DELETE CASCADE,
                canonical_name TEXT NOT NULL,
                quantity TEXT NOT NULL
            ))"),
            QStringLiteral("CREATE TABLE file_cleanup (path TEXT PRIMARY KEY)"),
        },
        // 4: separate reference applicability from dates; preserve recognized source type.
        {
            QStringLiteral("ALTER TABLE items ADD COLUMN reference_conditions_confirmed INTEGER NOT NULL DEFAULT 0"),
            QStringLiteral("ALTER TABLE imports ADD COLUMN source_type TEXT NOT NULL DEFAULT 'other'"),
        },
    };
    return list;
}

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

}

Database::Database(QString path)
    : m_path(std::move(path)),
      m_connectionName(QStringLiteral("whilefresh-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database()
{
    {
        QSqlDatabase db = handle();
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::open(QString *error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_path);
    if (!db.open()) {
        setError(error, QStringLiteral("database: cannot open: %1").arg(db.lastError().text()));
        return false;
    }
    QSqlQuery pragma(db);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setError(error, QStringLiteral("database: %1").arg(pragma.lastError().text()));
        return false;
    }
    return true;
}

int Database::latestVersion()
{
    return migrations().size();
}

int Database::schemaVersion() const
{
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return -1;
    return query.value(0).toInt();
}

bool Database::migrate(QString *error, int targetVersion)
{
    const int target = targetVersion < 0 ? latestVersion() : std::min(targetVersion, latestVersion());
    const int current = schemaVersion();
    if (current < 0) {
        setError(error, QStringLiteral("database: cannot read schema version"));
        return false;
    }
    if (current > latestVersion()) {
        setError(error, QStringLiteral("database: schema version %1 is newer than this app (%2)")
                            .arg(current)
                            .arg(latestVersion()));
        return false;
    }
    QSqlDatabase db = handle();
    for (int version = current + 1; version <= target; ++version) {
        if (!db.transaction()) {
            setError(error, QStringLiteral("database: %1").arg(db.lastError().text()));
            return false;
        }
        QSqlQuery query(db);
        for (const QString &statement : migrations().at(version - 1)) {
            if (!query.exec(statement)) {
                const QString message = query.lastError().text();
                db.rollback();
                setError(error, QStringLiteral("database: migration %1 failed: %2").arg(version).arg(message));
                return false;
            }
        }
        if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version)) || !db.commit()) {
            db.rollback();
            setError(error, QStringLiteral("database: cannot finish migration %1").arg(version));
            return false;
        }
    }
    return true;
}

bool Database::transaction()
{
    return handle().transaction();
}

bool Database::commit()
{
    return handle().commit();
}

void Database::rollback()
{
    handle().rollback();
}

}
