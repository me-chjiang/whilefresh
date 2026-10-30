#include "data/database.h"
#include "data/name-mappings.h"
#include "data/store.h"

#include <QDir>
#include <QSqlQuery>
#include <QTimeZone>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace whilefresh::core;
using namespace whilefresh::data;

class NameMappingsTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_database;
    std::unique_ptr<FixedClock> m_clock;

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_database = std::make_unique<Database>(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(m_database->open(&error) && m_database->migrate(&error), qPrintable(error));
        QVERIFY(m_database->schemaVersion() >= 5);
        m_clock = std::make_unique<FixedClock>(QDateTime(QDate(2026, 10, 30), QTime(9, 0), QTimeZone::UTC));
    }

    void cleanup()
    {
        m_database.reset();
        m_dir.reset();
    }

    void remembersAndOverwritesByKey()
    {
        SqliteNameMappingStore names(*m_database, *m_clock);
        QVERIFY(names.remember(QStringLiteral(" 农家  土猪肉 "), QStringLiteral("猪肉")));
        QCOMPARE(names.mapped(QStringLiteral("农家 土猪肉")), QStringLiteral("猪肉"));
        m_clock->set(QDateTime(QDate(2026, 10, 31), QTime(9, 0), QTimeZone::UTC));
        QVERIFY(names.remember(QStringLiteral("农家 土猪肉"), QStringLiteral("五花肉")));
        QCOMPARE(names.mapped(QStringLiteral("农家 土猪肉")), QStringLiteral("五花肉"));
        QVERIFY(names.mapped(QStringLiteral("别的")).isEmpty());
        QVERIFY(!names.remember(QStringLiteral("  "), QStringLiteral("猪肉")));

        QSqlQuery query(m_database->handle());
        QVERIFY(query.exec(QStringLiteral("SELECT raw_name, canonical_name, confirmed_at FROM name_mappings")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("农家 土猪肉"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("五花肉"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("2026-10-31T09:00:00.000Z"));
        QVERIFY(!query.next());
    }

    void deleteAllDataForgetsMappings()
    {
        SqliteNameMappingStore names(*m_database, *m_clock);
        QVERIFY(names.remember(QStringLiteral("冰鲜鸡全腿"), QStringLiteral("鸡腿")));
        const QString imports = m_dir->filePath(QStringLiteral("imports"));
        QVERIFY(QDir().mkpath(imports));
        Store store(*m_database, *m_clock, imports);
        QVERIFY(store.deleteAllData());
        QVERIFY(names.mapped(QStringLiteral("冰鲜鸡全腿")).isEmpty());
    }
};

QTEST_MAIN(NameMappingsTest)
#include "name-mappings-test.moc"
