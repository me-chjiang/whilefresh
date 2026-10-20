#include "platform/captured-at.h"

#include <QTest>
#include <QTimeZone>

using whilefresh::platform::CapturedAt;
using whilefresh::platform::ImageDateHints;
using whilefresh::platform::parseExifDateTime;
using whilefresh::platform::resolveCapturedAt;
using Source = whilefresh::platform::ImportedImage::DateSource;

namespace {

const QDateTime now(QDate(2026, 10, 21), QTime(12, 0), QTimeZone::UTC);

qint64 ms(int year, int month, int day, int hour = 9)
{
    return QDateTime(QDate(year, month, day), QTime(hour, 0), QTimeZone::UTC).toMSecsSinceEpoch();
}

ImageDateHints allHints()
{
    ImageDateHints hints;
    hints.cameraCaptureMs = ms(2026, 10, 21);
    hints.exifDateTimeOriginal = QStringLiteral("2026:10:20 08:30:00");
    hints.mediaDateTakenMs = ms(2026, 10, 19);
    hints.mediaDateAddedMs = ms(2026, 10, 18);
    hints.fileModifiedMs = ms(2026, 10, 17);
    return hints;
}

}

class CapturedAtTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesExifWithoutOffsetAsLocalTime()
    {
        const QDateTime parsed = parseExifDateTime(QStringLiteral("2026:10:20 08:30:15"));
        QVERIFY(parsed.isValid());
        QCOMPARE(parsed.timeSpec(), Qt::LocalTime);
        QCOMPARE(parsed.date(), QDate(2026, 10, 20));
        QCOMPARE(parsed.time(), QTime(8, 30, 15));
    }

    void parsesExifWithOffset()
    {
        const QDateTime parsed = parseExifDateTime(QStringLiteral("2026:10:20 08:30:00"), QStringLiteral("+08:00"));
        QCOMPARE(parsed.toUTC(), QDateTime(QDate(2026, 10, 20), QTime(0, 30), QTimeZone::UTC));
        const QDateTime west = parseExifDateTime(QStringLiteral("2026:10:20 08:30:00"), QStringLiteral("-05:30"));
        QCOMPARE(west.toUTC(), QDateTime(QDate(2026, 10, 20), QTime(14, 0), QTimeZone::UTC));
    }

    void acceptsDashSeparatedDates()
    {
        QVERIFY(parseExifDateTime(QStringLiteral("2026-10-20 08:30:00")).isValid());
    }

    void rejectsBlankAndMalformedExif()
    {
        QVERIFY(!parseExifDateTime(QString()).isValid());
        QVERIFY(!parseExifDateTime(QStringLiteral("    :  :     :  :  ")).isValid());
        QVERIFY(!parseExifDateTime(QStringLiteral("0000:00:00 00:00:00")).isValid());
        QVERIFY(!parseExifDateTime(QStringLiteral("2026:13:40 08:30:00")).isValid());
        QVERIFY(!parseExifDateTime(QStringLiteral("yesterday")).isValid());
    }

    void prefersSourcesInOrder()
    {
        ImageDateHints hints = allHints();
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::Camera);
        hints.cameraCaptureMs = -1;
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::Exif);
        hints.exifDateTimeOriginal.clear();
        CapturedAt result = resolveCapturedAt(hints, now);
        QCOMPARE(result.source, Source::MediaTaken);
        QCOMPARE(result.when.toMSecsSinceEpoch(), ms(2026, 10, 19));
        hints.mediaDateTakenMs = -1;
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::MediaAdded);
        hints.mediaDateAddedMs = -1;
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::FileModified);
        hints.fileModifiedMs = -1;
        result = resolveCapturedAt(hints, now);
        QCOMPARE(result.source, Source::None);
        QVERIFY(!result.when.isValid());
    }

    void screenshotWithoutExifUsesMediaStore()
    {
        ImageDateHints hints;
        hints.mediaDateAddedMs = ms(2026, 10, 20);
        hints.fileModifiedMs = ms(2026, 10, 20);
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::MediaAdded);
    }

    void skipsImplausibleDates()
    {
        ImageDateHints hints;
        hints.exifDateTimeOriginal = QStringLiteral("1980:01:01 00:00:00"); // unset camera clock
        hints.mediaDateTakenMs = ms(2026, 10, 25);                         // in the future
        hints.mediaDateAddedMs = 0;
        hints.fileModifiedMs = ms(2026, 10, 21) * 1000;                     // seconds mistaken for ms
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::None);

        hints.fileModifiedMs = ms(2026, 10, 22, 6); // less than a day ahead: clock skew is tolerated
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::FileModified);
    }

    void acceptsExactBoundaries()
    {
        ImageDateHints hints;
        hints.fileModifiedMs = QDateTime(QDate(2000, 1, 1), QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch();
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::FileModified);
        hints.fileModifiedMs -= 1;
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::None);

        hints.fileModifiedMs = now.addDays(1).toMSecsSinceEpoch();
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::FileModified);
        hints.fileModifiedMs += 1;
        QCOMPARE(resolveCapturedAt(hints, now).source, Source::None);
    }
};

QTEST_APPLESS_MAIN(CapturedAtTest)

#include "captured-at-test.moc"
