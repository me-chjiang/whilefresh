#include "platform/captured-at.h"

#include <QRegularExpression>
#include <QTimeZone>

namespace whilefresh::platform {

namespace {

bool isPlausible(const QDateTime &when, const QDateTime &now)
{
    static const QDateTime earliest(QDate(2000, 1, 1), QTime(0, 0), QTimeZone::UTC);
    return when.isValid() && when >= earliest && when <= now.addDays(1);
}

QDateTime fromMs(qint64 ms)
{
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms) : QDateTime();
}

}

QDateTime parseExifDateTime(const QString &dateTime, const QString &offset)
{
    static const QRegularExpression pattern(
            QStringLiteral("^(\\d{4})[:-](\\d{2})[:-](\\d{2})[ T](\\d{2}):(\\d{2}):(\\d{2})"));
    const QRegularExpressionMatch match = pattern.match(dateTime.trimmed());
    if (!match.hasMatch())
        return {};
    const QDate date(match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt());
    const QTime time(match.captured(4).toInt(), match.captured(5).toInt(), match.captured(6).toInt());
    if (!date.isValid() || !time.isValid())
        return {};

    static const QRegularExpression offsetPattern(QStringLiteral("^([+-])(\\d{2}):(\\d{2})$"));
    const QRegularExpressionMatch offsetMatch = offsetPattern.match(offset.trimmed());
    if (!offsetMatch.hasMatch())
        return QDateTime(date, time);
    const int sign = offsetMatch.captured(1) == QLatin1String("-") ? -1 : 1;
    const int seconds = sign * (offsetMatch.captured(2).toInt() * 3600 + offsetMatch.captured(3).toInt() * 60);
    return QDateTime(date, time, QTimeZone::fromSecondsAheadOfUtc(seconds));
}

CapturedAt resolveCapturedAt(const ImageDateHints &hints, const QDateTime &now)
{
    using Source = ImportedImage::DateSource;
    const std::pair<QDateTime, Source> candidates[] = {
        { fromMs(hints.cameraCaptureMs), Source::Camera },
        { parseExifDateTime(hints.exifDateTimeOriginal, hints.exifOffsetTimeOriginal), Source::Exif },
        { fromMs(hints.mediaDateTakenMs), Source::MediaTaken },
        { fromMs(hints.mediaDateAddedMs), Source::MediaAdded },
        { fromMs(hints.fileModifiedMs), Source::FileModified },
    };
    for (const auto &[when, source] : candidates) {
        if (isPlausible(when, now))
            return { when, source };
    }
    return {};
}

}
