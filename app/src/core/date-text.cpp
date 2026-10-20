#include "core/date-text.h"

#include <QRegularExpression>

#include <array>
#include <utility>

namespace whilefresh::core {

namespace {

DateParse parsed(const QString &text, const QDate &date)
{
    if (!date.isValid())
        return { DateParse::Status::Unparseable, {}, text };
    return { DateParse::Status::Parsed, date, text };
}

// A month-day in the year closest to `base` in the given direction; Feb 29 skips to a leap year.
QDate resolveMonthDay(int month, int day, const QDate &base, DateDirection direction)
{
    if (month < 1 || month > 12 || day < 1 || day > 31)
        return {};
    Q_UNUSED(direction);
    const int step = -1;
    QDate candidate(base.year(), month, day);
    if (candidate.isValid()) {
        const bool wrongSide = candidate > base;
        if (!wrongSide)
            return candidate;
    }
    for (int offset = 1; offset <= 8; ++offset) {
        candidate = QDate(base.year() + step * offset, month, day);
        if (candidate.isValid())
            return candidate;
    }
    return {};
}

}

DateParse parseDateText(const QString &text, const QDate &baseDate, DateDirection direction)
{
    const QString original = text.trimmed();
    if (original.isEmpty())
        return {};
    const QString normalized = original.normalized(QString::NormalizationForm_KC);

    static const QRegularExpression fullDate(
        QStringLiteral("((?:19|20)\\d{2})\\s*[-/.年]\\s*(\\d{1,2})\\s*[-/.月]\\s*(\\d{1,2})(?!\\d)"));
    if (const auto match = fullDate.match(normalized); match.hasMatch()) {
        return parsed(original, QDate(match.captured(1).toInt(), match.captured(2).toInt(),
                                      match.captured(3).toInt()));
    }
    static const QRegularExpression compactDate(QStringLiteral("(?<!\\d)((?:19|20)\\d{2})(\\d{2})(\\d{2})(?!\\d)"));
    if (const auto match = compactDate.match(normalized); match.hasMatch()) {
        return parsed(original, QDate(match.captured(1).toInt(), match.captured(2).toInt(),
                                      match.captured(3).toInt()));
    }

    static const QRegularExpression monthDay(
        QStringLiteral("(?<!\\d)(\\d{1,2})\\s*(?:月|/|-|\\.)\\s*(\\d{1,2})(?!\\d)"));
    if (const auto match = monthDay.match(normalized); match.hasMatch()) {
        if (!baseDate.isValid())
            return { DateParse::Status::NeedsBaseDate, {}, original };
        return parsed(original, resolveMonthDay(match.captured(1).toInt(), match.captured(2).toInt(),
                                                baseDate, direction));
    }

    static const QRegularExpression daysAgo(QStringLiteral("(\\d{1,3})\\s*天前"));
    if (const auto match = daysAgo.match(normalized); match.hasMatch()) {
        if (!baseDate.isValid())
            return { DateParse::Status::NeedsBaseDate, {}, original };
        return parsed(original, baseDate.addDays(-match.captured(1).toInt()));
    }

    // Longer words first so that "大前天" is not read as "前天".
    static const std::array<std::pair<const char16_t *, int>, 11> relativeWords{ {
        { u"大前天", -3 }, { u"大后天", 3 }, { u"前天", -2 }, { u"后天", 2 }, { u"昨天", -1 },
        { u"昨日", -1 }, { u"明天", 1 }, { u"明日", 1 }, { u"今天", 0 }, { u"今日", 0 }, { u"当天", 0 },
    } };
    for (const auto &[word, offset] : relativeWords) {
        if (normalized.contains(QString::fromUtf16(word))) {
            if (!baseDate.isValid())
                return { DateParse::Status::NeedsBaseDate, {}, original };
            return parsed(original, baseDate.addDays(offset));
        }
    }
    return { DateParse::Status::Unparseable, {}, original };
}

std::optional<int> parseShelfLifeText(const QString &text)
{
    static const QRegularExpression label(
        QStringLiteral("保质期\\s*[:：]?\\s*(\\d{1,4})\\s*(天|日|个月|月|年)"));
    const auto match = label.match(text.normalized(QString::NormalizationForm_KC));
    if (!match.hasMatch())
        return std::nullopt;
    const int value = match.captured(1).toInt();
    const QString unit = match.captured(2);
    int days = value;
    if (unit == QStringLiteral("年"))
        days = value * 365;
    else if (unit.endsWith(QStringLiteral("月")))
        days = value * 30;
    if (days <= 0)
        return std::nullopt;
    return days;
}

}
