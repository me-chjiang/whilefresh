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
                 (QStringList{ "ai_shelf_life_cache", "file_cleanup", "import_images", "import_products", "imports", "items", "pantry_overrides",
                               "settings", "shelf_life_rules", "shopping_items", "storage_rules", "suggestions" }));
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
