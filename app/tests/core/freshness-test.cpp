#include "core/clock.h"
#include "core/freshness.h"
#include "core/fridge-item.h"
#include "core/texts.h"

#include <QTest>

using namespace whilefresh::core;

Q_DECLARE_METATYPE(Freshness)

class FreshnessTest : public QObject
{
    Q_OBJECT

private slots:
    void thresholds_data()
    {
        QTest::addColumn<int>("total");
        QTest::addColumn<int>("remaining");
        QTest::addColumn<Freshness>("expected");

        // 25% of 20 days = 5 days.
        QTest::newRow("20d, 6 left") << 20 << 6 << Freshness::Fresh;
        QTest::newRow("20d, 5 left") << 20 << 5 << Freshness::NearExpiry;
        QTest::newRow("20d, 1 left") << 20 << 1 << Freshness::NearExpiry;
        QTest::newRow("20d, 0 left") << 20 << 0 << Freshness::BeyondEstimate;
        QTest::newRow("20d, past") << 20 << -3 << Freshness::BeyondEstimate;
        // Short shelf lives still get one near-expiry day: max(1, 3 * 25%) = 1.
        QTest::newRow("3d, 2 left") << 3 << 2 << Freshness::Fresh;
        QTest::newRow("3d, 1 left") << 3 << 1 << Freshness::NearExpiry;
        // 25% of 10 = 2.5: 2 is near, 3 is fresh.
        QTest::newRow("10d, 3 left") << 10 << 3 << Freshness::Fresh;
        QTest::newRow("10d, 2 left") << 10 << 2 << Freshness::NearExpiry;
    }

    void thresholds()
    {
        QFETCH(int, total);
        QFETCH(int, remaining);
        QFETCH(Freshness, expected);
        const QDate today(2026, 10, 21);
        const FreshnessState state = evaluateFreshness(today.addDays(remaining), total, today);
        QCOMPARE(state.freshness, expected);
        QCOMPARE(state.remainingDays, std::optional<int>(remaining));
    }

    void unknownWithoutExpiry()
    {
        QCOMPARE(evaluateFreshness(QDate(), 5, QDate(2026, 10, 21)).freshness, Freshness::Unknown);
    }

    void advancingTheClockChangesState()
    {
        FixedClock clock(QDateTime(QDate(2026, 10, 21), QTime(9, 0)));
        FridgeItem item;
        item.entryDate = clock.today();
        item.shelfLifeDays = 8;
        item.expiryDate = item.entryDate.addDays(8);

        QCOMPARE(item.freshness(clock.today()).freshness, Freshness::Fresh);
        clock.advanceDays(5); // 3 left, 25% of 8 = 2
        QCOMPARE(item.freshness(clock.today()).freshness, Freshness::Fresh);
        clock.advanceDays(1); // 2 left
        QCOMPARE(item.freshness(clock.today()).freshness, Freshness::NearExpiry);
        clock.advanceDays(2); // 0 left
        QCOMPARE(item.freshness(clock.today()).freshness, Freshness::BeyondEstimate);
        QCOMPARE(item.freshness(clock.today()).remainingDays, std::optional<int>(0));
    }

    void wordingAvoidsSafetyClaims()
    {
        const QStringList texts{ freshnessLabel(Freshness::Fresh), freshnessLabel(Freshness::NearExpiry),
                                 freshnessLabel(Freshness::BeyondEstimate), freshnessLabel(Freshness::Unknown),
                                 texts::freshnessBeyondHint() };
        for (const QString &text : texts) {
            QVERIFY(!text.contains(QStringLiteral("安全")));
            QVERIFY(!text.contains(QStringLiteral("变质")));
        }
        QCOMPARE(texts::freshnessBeyondHint(), QStringLiteral("超出估计期限，请查看后决定"));
    }
};

QTEST_GUILESS_MAIN(FreshnessTest)
#include "freshness-test.moc"
