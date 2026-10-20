#include "core/date-text.h"

#include <QTest>

using namespace whilefresh::core;
using Status = DateParse::Status;

Q_DECLARE_METATYPE(DateDirection)

class DateTextTest : public QObject
{
    Q_OBJECT

private slots:
    void parses_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QDate>("base");
        QTest::addColumn<DateDirection>("direction");
        QTest::addColumn<QDate>("expected");

        const QDate base(2026, 10, 21);
        const auto past = DateDirection::Past;
        const auto future = DateDirection::Future;
        QTest::newRow("today") << "今天生产" << base << past << QDate(2026, 10, 21);
        QTest::newRow("yesterday") << "昨天生产" << base << past << QDate(2026, 10, 20);
        QTest::newRow("day before") << "前天" << base << past << QDate(2026, 10, 19);
        QTest::newRow("three days") << "大前天生产" << base << past << QDate(2026, 10, 18);
        QTest::newRow("tomorrow") << "明天到期" << base << future << QDate(2026, 10, 22);
        QTest::newRow("n days ago") << "3天前生产" << base << past << QDate(2026, 10, 18);
        QTest::newRow("month day") << "10月18日生产" << base << past << QDate(2026, 10, 18);
        QTest::newRow("iso") << "2026-10-18" << base << past << QDate(2026, 10, 18);
        QTest::newRow("slash expiry") << "10/18 到期" << QDate(2026, 10, 10) << future << QDate(2025, 10, 18);
        QTest::newRow("chinese full") << "2026年10月18日" << base << past << QDate(2026, 10, 18);
        QTest::newRow("dots") << "生产日期：2026.10.18" << base << past << QDate(2026, 10, 18);
        QTest::newRow("compact") << "20261018" << base << past << QDate(2026, 10, 18);
        QTest::newRow("full width") << "１０月１８日" << base << past << QDate(2026, 10, 18);
        QTest::newRow("full date ignores base") << "2026-10-18" << QDate() << past << QDate(2026, 10, 18);
        // Past dates never land after the photo day: across the new year it is last year.
        QTest::newRow("past crosses year") << "12月30日生产" << QDate(2027, 1, 2) << past << QDate(2026, 12, 30);
        QTest::newRow("past same day") << "1月2日" << QDate(2027, 1, 2) << past << QDate(2027, 1, 2);
        // Expiry dates without a year obey the same most-recent-occurrence rule.
        QTest::newRow("future crosses year") << "1/5到期" << QDate(2026, 12, 28) << future << QDate(2026, 1, 5);
        QTest::newRow("yesterday across year") << "昨天生产" << QDate(2027, 1, 1) << past << QDate(2026, 12, 31);
        QTest::newRow("leap day") << "2月29日" << QDate(2027, 3, 1) << past << QDate(2024, 2, 29);
    }

    void parses()
    {
        QFETCH(QString, text);
        QFETCH(QDate, base);
        QFETCH(DateDirection, direction);
        QFETCH(QDate, expected);
        const DateParse result = parseDateText(text, base, direction);
        QCOMPARE(result.status, Status::Parsed);
        QCOMPARE(result.date, expected);
        QCOMPARE(result.text, text.trimmed());
    }

    void needsBaseDateWhenPhotoDateIsMissing_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("today") << "今天生产";
        QTest::newRow("yesterday") << "昨天生产";
        QTest::newRow("month day") << "10月18日生产";
        QTest::newRow("slash") << "10/18 到期";
        QTest::newRow("n days ago") << "2天前";
    }

    void needsBaseDateWhenPhotoDateIsMissing()
    {
        QFETCH(QString, text);
        const DateParse result = parseDateText(text, QDate(), DateDirection::Past);
        QCOMPARE(result.status, Status::NeedsBaseDate);
        QVERIFY(!result.date.isValid());
        QCOMPARE(result.text, text);
    }

    void keepsUnparseableText_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("words") << "新鲜直达";
        QTest::newRow("bad date") << "2026-02-30";
        QTest::newRow("bad month") << "13月5日";
    }

    void keepsUnparseableText()
    {
        QFETCH(QString, text);
        const DateParse result = parseDateText(text, QDate(2026, 10, 21), DateDirection::Past);
        QCOMPARE(result.status, Status::Unparseable);
        QVERIFY(!result.date.isValid());
        QCOMPARE(result.text, text);
    }

    void emptyTextIsEmpty()
    {
        QCOMPARE(parseDateText(QStringLiteral("  "), QDate(2026, 10, 21), DateDirection::Past).status, Status::Empty);
    }

    void labelledShelfLife()
    {
        QCOMPARE(parseShelfLifeText(QStringLiteral("保质期180天")), std::optional<int>(180));
        QCOMPARE(parseShelfLifeText(QStringLiteral("保质期：12个月")), std::optional<int>(360));
        QCOMPARE(parseShelfLifeText(QStringLiteral("保质期 1 年")), std::optional<int>(365));
        QCOMPARE(parseShelfLifeText(QStringLiteral("2026-11-03到期")), std::nullopt);
    }
};

QTEST_GUILESS_MAIN(DateTextTest)
#include "date-text-test.moc"
