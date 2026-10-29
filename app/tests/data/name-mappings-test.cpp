#include "data/database.h"
#include "data/name-mappings.h"

#include <QRegularExpression>
#include <QSqlQuery>
#include <QTimeZone>
#include <QTest>

using namespace whilefresh::core;
using namespace whilefresh::data;

class NameMappingsTest : public QObject
{
    Q_OBJECT

private slots:
    void withoutTheTableNothingIsRemembered()
    {
        Database database(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QSqlQuery(database.handle()).exec(QStringLiteral("DROP TABLE IF EXISTS name_mappings"));
        FixedClock clock(QDateTime(QDate(2026, 10, 30), QTime(9, 0), QTimeZone::UTC));
        SqliteNameMappingStore names(database, clock);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("remember failed")));
        QVERIFY(!names.remember(QStringLiteral("花胶"), QStringLiteral("鱼")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("lookup failed")));
        QVERIFY(names.mapped(QStringLiteral("花胶")).isEmpty());
    }

    void remembersAndOverwritesByKey()
    {
        Database database(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(database.open(&error) && database.migrate(&error), qPrintable(error));
        // Migration v5 (T15) creates this table; the statement is a no-op once it exists.
        QVERIFY(QSqlQuery(database.handle())
                    .exec(QStringLiteral("CREATE TABLE IF NOT EXISTS name_mappings (raw_name TEXT PRIMARY KEY, "
                                         "canonical TEXT NOT NULL, confirmed_at TEXT)")));
        FixedClock clock(QDateTime(QDate(2026, 10, 30), QTime(9, 0), QTimeZone::UTC));
        SqliteNameMappingStore names(database, clock);
        QVERIFY(names.remember(QStringLiteral(" 农家  土猪肉 "), QStringLiteral("猪肉")));
        QCOMPARE(names.mapped(QStringLiteral("农家 土猪肉")), QStringLiteral("猪肉"));
        QVERIFY(names.remember(QStringLiteral("农家 土猪肉"), QStringLiteral("五花肉")));
        QCOMPARE(names.mapped(QStringLiteral("农家 土猪肉")), QStringLiteral("五花肉"));
        QVERIFY(names.mapped(QStringLiteral("别的")).isEmpty());
        QVERIFY(!names.remember(QStringLiteral("  "), QStringLiteral("猪肉")));

        QSqlQuery query(database.handle());
        QVERIFY(query.exec(QStringLiteral("SELECT confirmed_at FROM name_mappings")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("2026-10-30T09:00:00.000Z"));
    }
};

QTEST_MAIN(NameMappingsTest)
#include "name-mappings-test.moc"
