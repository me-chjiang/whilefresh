#pragma once

#include "recognition/recognition-types.h"
#include "recognition/vocabulary.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

// QCOMPARE prints values through QTest::toString; give the enums readable names.
namespace QTest {
template<>
inline char *toString(const whilefresh::recognition::ErrorKind &value)
{
    return toString(whilefresh::recognition::enumKey(value));
}
template<>
inline char *toString(const whilefresh::recognition::SourceType &value)
{
    return toString(whilefresh::recognition::enumKey(value));
}
template<>
inline char *toString(const whilefresh::recognition::ItemCategory &value)
{
    return toString(whilefresh::recognition::enumKey(value));
}
template<>
inline char *toString(const whilefresh::recognition::StorageKind &value)
{
    return toString(whilefresh::recognition::enumKey(value));
}
template<>
inline char *toString(const whilefresh::recognition::InputKind &value)
{
    return toString(whilefresh::recognition::enumKey(value));
}
}

namespace whilefresh::recognition::testing {

inline QString testDataPath(const QString &relative)
{
    return QDir(QStringLiteral(WHILEFRESH_TEST_DATA_DIR)).filePath(relative);
}

inline std::shared_ptr<const Vocabulary> syntheticVocabulary()
{
    QString error;
    auto vocabulary = Vocabulary::fromFile(testDataPath(QStringLiteral("vocabulary/synthetic-vocabulary.json")), &error);
    if (!vocabulary)
        qFatal("cannot load synthetic vocabulary: %s", qUtf8Printable(error));
    return std::make_shared<const Vocabulary>(*vocabulary);
}

// A /chat/completions body whose assistant message is `content`.
inline QByteArray chatCompletion(const QString &content, int promptTokens = 100, int completionTokens = 50,
                                 const QString &finishReason = QStringLiteral("stop"))
{
    const QJsonObject message{ { "role", "assistant" }, { "content", content } };
    const QJsonObject choice{ { "index", 0 }, { "message", message }, { "finish_reason", finishReason } };
    const QJsonObject usage{ { "prompt_tokens", promptTokens },
                             { "completion_tokens", completionTokens },
                             { "total_tokens", promptTokens + completionTokens } };
    return QJsonDocument(QJsonObject{ { "choices", QJsonArray{ choice } }, { "usage", usage } })
        .toJson(QJsonDocument::Compact);
}

inline QString itemJson(const QString &rawName, const QString &canonicalName, const QString &extra = {})
{
    return QStringLiteral("{\"raw_name\":\"%1\",\"canonical_name\":\"%2\"%3}")
        .arg(rawName, canonicalName, extra.isEmpty() ? QString() : QStringLiteral(",") + extra);
}

inline QString modelJson(const QStringList &items, const QString &extra = {})
{
    return QStringLiteral("{\"source_type\":\"cart\",\"items\":[%1]%2}")
        .arg(items.join(QLatin1Char(',')), extra.isEmpty() ? QString() : QStringLiteral(",") + extra);
}

}
