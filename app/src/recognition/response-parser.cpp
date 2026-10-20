#include "recognition/response-parser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace whilefresh::recognition {

namespace {

constexpr int kMaxCount = 999;
constexpr int kMaxShelfLifeDays = 3650;

ParseOutcome fail(const QString &detail)
{
    return { std::nullopt, makeError(ErrorKind::BadResponse, detail) };
}

bool isNullLike(const QString &text)
{
    static const QSet<QString> kNullLike = { QStringLiteral(""), QStringLiteral("null"),
                                             QStringLiteral("none"), QStringLiteral("n/a"),
                                             QStringLiteral("无"), QStringLiteral("-"),
                                             QStringLiteral("—"), QStringLiteral("未知") };
    return kNullLike.contains(text.toLower());
}

// Optional free text; the model's placeholders for "nothing" become empty.
QString optionalText(const QJsonValue &value)
{
    QString text;
    if (value.isString())
        text = value.toString().trimmed();
    else if (value.isDouble())
        text = QString::number(value.toDouble(), 'g', 12);
    return isNullLike(text) ? QString() : text;
}

QString requiredText(const QJsonValue &value)
{
    return value.isString() ? value.toString().trimmed() : optionalText(value);
}

std::optional<int> integerOf(const QJsonValue &value)
{
    if (value.isDouble())
        return qRound(value.toDouble());
    if (value.isString()) {
        static const QRegularExpression digits(QStringLiteral("\\d+"));
        const auto match = digits.match(value.toString());
        if (match.hasMatch())
            return match.captured().toInt();
    }
    return std::nullopt;
}

bool boolOf(const QJsonValue &value, bool fallback)
{
    if (value.isBool())
        return value.toBool();
    if (value.isDouble())
        return value.toDouble() != 0;
    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();
        if (text == QLatin1String("true") || text == QLatin1String("yes") || text == QStringLiteral("是")
            || text == QLatin1String("1"))
            return true;
        if (text == QLatin1String("false") || text == QLatin1String("no") || text == QStringLiteral("否")
            || text == QLatin1String("0"))
            return false;
    }
    return fallback;
}

std::optional<int> shelfLifeOf(const QJsonValue &value)
{
    const std::optional<int> days = integerOf(value);
    if (!days || *days <= 0 || *days > kMaxShelfLifeDays)
        return std::nullopt;
    return days;
}

QString canonicalOf(const QString &modelName, const Vocabulary *vocabulary)
{
    if (!vocabulary)
        return modelName.isEmpty() ? uncategorizedName() : modelName;
    const QString resolved = vocabulary->resolve(modelName);
    return resolved.isEmpty() ? uncategorizedName() : resolved;
}

std::optional<RecognizedItem> parseItem(const QJsonValue &value, const Vocabulary *vocabulary)
{
    if (!value.isObject())
        return std::nullopt;
    const QJsonObject object = value.toObject();

    RecognizedItem item;
    item.rawName = requiredText(object.value(QStringLiteral("raw_name")));
    const QString modelName = requiredText(object.value(QStringLiteral("canonical_name")));
    if (item.rawName.isEmpty() && modelName.isEmpty())
        return std::nullopt;
    if (item.rawName.isEmpty())
        item.rawName = modelName;
    item.canonicalName = canonicalOf(modelName, vocabulary);

    item.quantityText = optionalText(object.value(QStringLiteral("quantity_text")));
    item.count = qBound(1, integerOf(object.value(QStringLiteral("count"))).value_or(1), kMaxCount);
    item.category = itemCategoryFromString(requiredText(object.value(QStringLiteral("category"))))
                        .value_or(ItemCategory::Food);
    item.selected = boolOf(object.value(QStringLiteral("selected")), true);
    item.isGift = boolOf(object.value(QStringLiteral("is_gift")), false);
    item.productionDateText = optionalText(object.value(QStringLiteral("production_date_text")));
    item.expiryDateText = optionalText(object.value(QStringLiteral("expiry_date_text")));
    item.purchaseDateText = optionalText(object.value(QStringLiteral("purchase_date_text")));
    item.storage = storageKindFromString(requiredText(object.value(QStringLiteral("storage"))))
                       .value_or(StorageKind::Unknown);

    const QJsonObject shelf = object.value(QStringLiteral("shelf_life_days")).toObject();
    item.shelfLifeDays.fridge = shelfLifeOf(shelf.value(QStringLiteral("fridge")));
    item.shelfLifeDays.freezer = shelfLifeOf(shelf.value(QStringLiteral("freezer")));
    item.shelfLifeDays.room = shelfLifeOf(shelf.value(QStringLiteral("room")));

    // The prompt numbers images from 1.
    const std::optional<int> imageNumber = integerOf(object.value(QStringLiteral("image_index")));
    item.imageIndex = (imageNumber && *imageNumber >= 1) ? *imageNumber - 1 : -1;
    return item;
}

QString stripCodeFence(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1String("```"))) {
        const qsizetype firstNewline = text.indexOf(QLatin1Char('\n'));
        text = firstNewline < 0 ? QString() : text.mid(firstNewline + 1);
        const qsizetype lastFence = text.lastIndexOf(QLatin1String("```"));
        if (lastFence >= 0)
            text.truncate(lastFence);
    }
    return text.trimmed();
}

// Falls back to the outermost braces when the model added prose around the JSON.
QJsonDocument parseJsonLoosely(const QString &content)
{
    const QString text = stripCodeFence(content);
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isNull() && (doc.isObject() || doc.isArray()))
        return doc;

    const qsizetype open = text.indexOf(QLatin1Char('{'));
    const qsizetype close = text.lastIndexOf(QLatin1Char('}'));
    if (open >= 0 && close > open)
        return QJsonDocument::fromJson(text.mid(open, close - open + 1).toUtf8());
    return {};
}

QString itemKey(const RecognizedItem &item)
{
    return item.rawName + QLatin1Char('\x1f') + item.quantityText + QLatin1Char('\x1f')
        + (item.isGift ? QLatin1Char('1') : QLatin1Char('0'));
}

}

ParseOutcome parseModelContent(const QString &content, const Vocabulary *vocabulary, InputKind kind)
{
    const QJsonDocument doc = parseJsonLoosely(content);
    if (doc.isNull())
        return fail(QStringLiteral("model text is not JSON (%1 chars)").arg(content.size()));

    QJsonObject root;
    QJsonValue itemsValue;
    if (doc.isArray()) {
        itemsValue = doc.array();
    } else {
        root = doc.object();
        itemsValue = root.value(QStringLiteral("items"));
    }
    if (!itemsValue.isArray())
        return fail(QStringLiteral("JSON has no \"items\" array"));

    RecognitionResult result;
    for (const QJsonValue &value : itemsValue.toArray()) {
        if (const auto item = parseItem(value, vocabulary))
            result.items.append(*item);
    }
    result.ignoredSummary = optionalText(root.value(QStringLiteral("ignored_summary")));

    const auto declared = sourceTypeFromString(requiredText(root.value(QStringLiteral("source_type"))));
    if (declared) {
        result.sourceType = *declared;
    } else {
        switch (kind) {
        case InputKind::Receipt: result.sourceType = SourceType::Receipt; break;
        case InputKind::OrderText: result.sourceType = SourceType::Text; break;
        case InputKind::Screenshot: result.sourceType = SourceType::Other; break;
        }
    }
    return { result, {} };
}

ParseOutcome parseChatCompletion(const QByteArray &body, const Vocabulary *vocabulary, InputKind kind)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (!doc.isObject())
        return fail(QStringLiteral("HTTP body is not a JSON object: %1").arg(parseError.errorString()));

    const QJsonObject choice = doc.object().value(QStringLiteral("choices")).toArray().first().toObject();
    const QJsonObject message = choice.value(QStringLiteral("message")).toObject();
    const QString finishReason = choice.value(QStringLiteral("finish_reason")).toString();
    if (finishReason == QLatin1String("length"))
        return fail(QStringLiteral("model reply was cut off (finish_reason=length)"));
    if (!message.value(QStringLiteral("content")).isString())
        return fail(QStringLiteral("response has no message content (finish_reason=%1)").arg(finishReason));

    ParseOutcome outcome = parseModelContent(message.value(QStringLiteral("content")).toString(), vocabulary, kind);
    if (!outcome.result)
        return outcome;

    const QJsonObject usage = doc.object().value(QStringLiteral("usage")).toObject();
    outcome.result->usage.promptTokens = usage.value(QStringLiteral("prompt_tokens")).toInt();
    outcome.result->usage.completionTokens = usage.value(QStringLiteral("completion_tokens")).toInt();
    outcome.result->usage.totalTokens = usage.value(QStringLiteral("total_tokens")).toInt();
    outcome.result->rawResponses.append(body);
    return outcome;
}

RecognitionResult mergeResults(const QList<RecognitionResult> &results)
{
    RecognitionResult merged;
    QSet<QString> earlierKeys;
    bool sourceTypeSet = false;
    QStringList summaries;

    for (const RecognitionResult &part : results) {
        QSet<QString> thisKeys;
        for (const RecognizedItem &item : part.items) {
            const QString key = itemKey(item);
            thisKeys.insert(key);
            if (!earlierKeys.contains(key))
                merged.items.append(item);
        }
        earlierKeys.unite(thisKeys);

        if (!sourceTypeSet || (merged.sourceType == SourceType::Other && part.sourceType != SourceType::Other)) {
            merged.sourceType = part.sourceType;
            sourceTypeSet = true;
        }
        if (!part.ignoredSummary.isEmpty() && !summaries.contains(part.ignoredSummary))
            summaries.append(part.ignoredSummary);

        merged.rawResponses.append(part.rawResponses);
        merged.usage.promptTokens += part.usage.promptTokens;
        merged.usage.completionTokens += part.usage.completionTokens;
        merged.usage.totalTokens += part.usage.totalTokens;
        merged.isDemo = merged.isDemo || part.isDemo;
    }
    merged.ignoredSummary = summaries.join(QStringLiteral("；"));
    return merged;
}

}
