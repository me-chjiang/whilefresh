#pragma once

#include <QDate>
#include <QString>

#include <optional>

namespace whilefresh::core {

// Result of turning verbatim date text from recognition ("昨天生产", "10/18 到期") into a date.
struct DateParse {
    enum class Status {
        Empty,         // no text
        Parsed,        // `date` is set
        NeedsBaseDate, // relative or year-less text, but the photo date is unknown: ask the user
        Unparseable,   // keep the text, leave the date empty
    };
    Status status = Status::Empty;
    QDate date;
    QString text;
};

// Year-less dates ("10月18日") are ambiguous. Production and purchase dates lie in the past, so
// they resolve to the latest occurrence not after the base day; expiry dates lie ahead, so they
// resolve to the earliest occurrence not before it.
enum class DateDirection { Past, Future };

// `baseDate` is the day the photo was taken (relative words refer to it); invalid when unknown.
// Understands 今天/今日/昨天/昨日/前天/大前天/明天/后天/大后天, "N天前", full dates
// (2026-10-18, 2026/10/18, 2026.10.18, 2026年10月18日, 20261018) and month-day forms
// (10月18日, 10/18, 10-18, 10.18). Surrounding words such as "生产" or "到期" are ignored.
DateParse parseDateText(const QString &text, const QDate &baseDate, DateDirection direction);

// A labelled shelf life such as "保质期180天", "保质期12个月", "保质期1年"; months count as 30
// days and years as 365. Empty when the text holds no such label.
std::optional<int> parseShelfLifeText(const QString &text);

}
