#include "core/quantity.h"

#include <QHash>
#include <QList>
#include <QRegularExpression>

#include <algorithm>

namespace whilefresh::core {

namespace {

struct Measure {
    const char *unit; // as written, lower case
    const char *base; // "g" or "ml"
    double factor;
};

constexpr Measure measures[] = {
    { "kg", "g", 1000 }, { "千克", "g", 1000 }, { "公斤", "g", 1000 }, { "g", "g", 1 }, { "克", "g", 1 },
    { "斤", "g", 500 },  { "两", "g", 50 },     { "ml", "ml", 1 },     { "毫升", "ml", 1 }, { "l", "ml", 1000 },
    { "升", "ml", 1000 },
};

constexpr const char *countUnits[] = { "个", "件", "枚", "只", "盒", "袋", "包", "瓶", "罐", "听", "桶", "箱", "把", "根",
                                       "颗", "粒", "条", "块", "片", "份", "支", "棵", "头", "束", "扎", "板", "卷",
                                       "串", "盘", "杯", "碗", "提", "筒", "节", "张", "瓣" };

const QHash<QString, double> &chineseNumbers()
{
    static const QHash<QString, double> numbers{
        { QStringLiteral("半"), 0.5 }, { QStringLiteral("一"), 1 }, { QStringLiteral("二"), 2 },
        { QStringLiteral("两"), 2 },   { QStringLiteral("三"), 3 }, { QStringLiteral("四"), 4 },
        { QStringLiteral("五"), 5 },   { QStringLiteral("六"), 6 }, { QStringLiteral("七"), 7 },
        { QStringLiteral("八"), 8 },   { QStringLiteral("九"), 9 }, { QStringLiteral("十"), 10 },
    };
    return numbers;
}

const QRegularExpression &tokenPattern()
{
    static const QRegularExpression pattern = [] {
        QStringList units;
        for (const auto &measure : measures)
            units.append(QRegularExpression::escape(QString::fromUtf8(measure.unit)));
        for (const char *unit : countUnits)
            units.append(QString::fromUtf8(unit));
        // Longest first so "kg" is not read as "g" and "毫升" not as "升".
        std::sort(units.begin(), units.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
        return QRegularExpression(QStringLiteral("(\\d+(?:\\.\\d+)?|[半一二两三四五六七八九十])\\s*(%1)?")
                                      .arg(units.join(QLatin1Char('|'))));
    }();
    return pattern;
}

struct Token {
    double number = 0;
    QString unit;
    qsizetype start = 0;
    qsizetype end = 0;
    const Measure *measure = nullptr;
};

double numberOf(const QString &text)
{
    const auto it = chineseNumbers().constFind(text);
    return it != chineseNumbers().cend() ? *it : text.toDouble();
}

bool joinedByMultiplier(const QString &text, qsizetype from, qsizetype to)
{
    const QString gap = text.mid(from, to - from).trimmed();
    return gap.size() == 1 && QStringLiteral("×x*").contains(gap.at(0));
}

}

Amount parseAmount(const QString &input)
{
    const QString text = input.normalized(QString::NormalizationForm_KC).toLower();
    QList<Token> tokens;
    auto it = tokenPattern().globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        Token token{ numberOf(match.captured(1)), match.captured(2), match.capturedStart(), match.capturedEnd() };
        // A Chinese numeral without a unit is ordinary text, not a quantity.
        if (token.unit.isEmpty() && chineseNumbers().contains(match.captured(1)))
            continue;
        for (const auto &measure : measures) {
            if (token.unit == QString::fromUtf8(measure.unit))
                token.measure = &measure;
        }
        tokens.append(token);
    }

    qsizetype base = -1;
    for (qsizetype i = 0; i < tokens.size() && base < 0; ++i) {
        if (tokens.at(i).measure)
            base = i;
    }
    for (qsizetype i = 0; i < tokens.size() && base < 0; ++i) {
        if (!tokens.at(i).unit.isEmpty())
            base = i;
    }
    if (base < 0 || tokens.at(base).number <= 0)
        return {};

    const Token &token = tokens.at(base);
    double value = token.number;
    QString unit = token.unit;
    if (token.measure) {
        value *= token.measure->factor;
        unit = QString::fromUtf8(token.measure->base);
        // "500g×2袋" and "2袋×500g": the neighbouring package count multiplies the measure.
        const auto multiplies = [&](qsizetype other) {
            return other >= 0 && other < tokens.size() && !tokens.at(other).measure && tokens.at(other).number > 0;
        };
        if (multiplies(base + 1) && joinedByMultiplier(text, token.end, tokens.at(base + 1).start))
            value *= tokens.at(base + 1).number;
        else if (multiplies(base - 1) && joinedByMultiplier(text, tokens.at(base - 1).end, token.start))
            value *= tokens.at(base - 1).number;
    }
    return { value, unit };
}

Amount totalAmount(const QString &quantityText, int count)
{
    Amount amount = parseAmount(quantityText);
    if (amount.isKnown())
        amount.value *= std::max(1, count);
    return amount;
}

}
