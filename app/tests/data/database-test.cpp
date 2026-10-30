#include "data/database.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::data;

namespace {

QStringList tables(const Database &database)
{
    QStringList names;
    QSqlQuery query(database.handle());
    query.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name"));
    while (query.next())
        names.append(query.value(0).toString());
    return names;
}

}

class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void migratesEmptyDatabase()
    {
        Database database(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(database.open(&error), qPrintable(error));
        QCOMPARE(database.schemaVersion(), 0);
        QVERIFY2(database.migrate(&error), qPrintable(error));
        QCOMPARE(database.schemaVersion(), Database::latestVersion());
        QCOMPARE(tables(database),
                 (QStringList{ "ai_shelf_life_cache", "cleanup_sessions", "file_cleanup", "import_images", "import_job_batches",
                               "import_jobs", "import_products", "imports", "items", "name_mappings", "pantry_overrides",
                               "preferences", "settings", "shelf_life_rules", "shopping_items", "storage_rules", "suggestions" }));
        // Running again is a no-op.
        QVERIFY(database.migrate(&error));
        QCOMPARE(database.schemaVersion(), Database::latestVersion());
    }

    void upgradesFromVersionOneKeepingData()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("whilefresh.db"));
        {
            Database old(path);
            QVERIFY(old.open());
            QVERIFY(old.migrate(nullptr, 1));
            QCOMPARE(old.schemaVersion(), 1);
            QVERIFY(!tables(old).contains(QStringLiteral("settings")));
            QSqlQuery insert(old.handle());
            QVERIFY(insert.exec(QStringLiteral(
                "INSERT INTO shelf_life_rules (canonical_name, storage, days, updated_at) "
                "VALUES ('鸡蛋', 'fridge', 30, '2026-10-01T00:00:00.000Z')")));
        }
        Database upgraded(path);
        QVERIFY(upgraded.open());
        QString error;
        QVERIFY2(upgraded.migrate(&error), qPrintable(error));
        QCOMPARE(upgraded.schemaVersion(), Database::latestVersion());
        QVERIFY(tables(upgraded).contains(QStringLiteral("settings")));
        QSqlQuery query(upgraded.handle());
        QVERIFY(query.exec(QStringLiteral("SELECT days FROM shelf_life_rules WHERE canonical_name = '鸡蛋'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 30);
    }

    void upgradesFromVersionFourWithRound2Defaults()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("whilefresh.db"));
        {
            Database old(path);
            QVERIFY(old.open());
            QVERIFY(old.migrate(nullptr, 4));
            QCOMPARE(old.schemaVersion(), 4);
            QSqlQuery insert(old.handle());
            QVERIFY(insert.exec(QStringLiteral(
                "INSERT INTO imports (created_at, input_kind) VALUES ('2026-10-01T00:00:00.000Z', 'screenshot')")));
            QVERIFY(insert.exec(QStringLiteral(
                "INSERT INTO items (import_id, canonical_name, display_name, quantity_text, storage, entry_date, created_at, updated_at) "
                "VALUES (1, '鸡蛋', '鸡蛋', '10枚', 'fridge', '2026-10-01', '2026-10-01T00:00:00.000Z', '2026-10-01T00:00:00.000Z')")));
        }
        Database upgraded(path);
        QVERIFY(upgraded.open());
        QString error;
        QVERIFY2(upgraded.migrate(&error), qPrintable(error));
        QCOMPARE(upgraded.schemaVersion(), 5);
        for (const auto &table : { "name_mappings", "preferences", "import_jobs", "import_job_batches", "cleanup_sessions" })
            QVERIFY2(tables(upgraded).contains(QLatin1String(table)), table);

        QSqlQuery query(upgraded.handle());
        QVERIFY(query.exec(QStringLiteral("SELECT kind, amount, unit, snoozed_until, status FROM items")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("food"));
        QCOMPARE(query.value(1).toDouble(), 0.0);
        QCOMPARE(query.value(2).toString(), QString());
        QVERIFY(query.value(3).isNull());
        QCOMPARE(query.value(4).toString(), QStringLiteral("in_stock"));
        QVERIFY(query.exec(QStringLiteral("SELECT status, duplicate_of, deleted_at FROM imports")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("confirmed"));
        QVERIFY(query.value(1).isNull());
        QVERIFY(query.value(2).isNull());
        QVERIFY(query.exec(QStringLiteral("INSERT INTO name_mappings (raw_name, canonical_name, confirmed_at) "
                                          "VALUES ('黑猪梅花肉切片', '梅花肉', '2026-10-30T00:00:00.000Z')")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO cleanup_sessions (state, created_at, updated_at) "
                                          "VALUES ('pending', '2026-10-30T00:00:00.000Z', '2026-10-30T00:00:00.000Z')")));
        QVERIFY(query.exec(QStringLiteral("SELECT payload, committed_at FROM cleanup_sessions")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("{}"));
        QVERIFY(query.value(1).isNull());
    }

    void refusesNewerSchema()
    {
        Database database(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QSqlQuery query(database.handle());
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = %1").arg(Database::latestVersion() + 1)));
        QString error;
        QVERIFY(!database.migrate(&error));
        QVERIFY(error.contains(QStringLiteral("newer")));
    }

    void twoConnectionsCoexist()
    {
        Database a(QStringLiteral(":memory:"));
        Database b(QStringLiteral(":memory:"));
        QVERIFY(a.open() && b.open());
        QVERIFY(a.migrate());
        QCOMPARE(b.schemaVersion(), 0);
    }
};

QTEST_GUILESS_MAIN(DatabaseTest)
#include "database-test.moc"
