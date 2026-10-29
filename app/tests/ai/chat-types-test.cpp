#include "ai/chat-types.h"

#include "ai-test-support.h"

#include <QTest>

using namespace whilefresh::ai;
using namespace whilefresh::ai::testing;

namespace {

ChatResult withContent(const QString &content, const QString &finishReason = QStringLiteral("stop"))
{
    ChatResult result;
    result.content = content;
    result.finishReason = finishReason;
    result.truncated = finishReason == QLatin1String("length");
    return result;
}

}

class ChatTypesTest : public QObject
{
    Q_OBJECT

private slots:
    void requestEncodingFollowsTheWireFormat()
    {
        ChatRequest request;
        request.messages = { ChatMessage::user(QStringLiteral("hi")) };
        QJsonObject body = request.toJson();
        QCOMPARE(body.value("messages").toArray().at(0).toObject().value("content").toString(), QStringLiteral("hi"));
        QVERIFY(!body.contains("response_format"));
        QVERIFY(!body.contains("max_tokens"));
        QVERIFY(!body.contains("temperature"));
        QVERIFY(!body.contains("thinking"));

        request.thinking = false;
        request.temperature = 0.3;
        body = request.toJson();
        QCOMPARE(body.value("thinking").toObject().value("type").toString(), QStringLiteral("disabled"));
        QCOMPARE(body.value("temperature").toDouble(), 0.3);

        const EncodedChatRequest encoded = encodeChatRequest(userRequest(QStringLiteral("12345")));
        QCOMPARE(encoded.summary.messageCount, 2);
        QCOMPARE(encoded.summary.imageCount, 0);
        QCOMPARE(encoded.summary.textChars, 9 + 5);
        QCOMPARE(encoded.summary.maxTokens, 4000);
        QVERIFY(encoded.summary.jsonMode);
        QCOMPARE(requestDigest(encoded.body).size(), 64);
        QCOMPARE(requestDigest(encoded.body), requestDigest(encodeChatRequest(userRequest(QStringLiteral("12345"))).body));
        QVERIFY(requestDigest(encoded.body) != requestDigest(encodeChatRequest(userRequest(QStringLiteral("1234"))).body));
    }

    void perRequestLimitsAreCarriedButNotSent()
    {
        ChatRequest request = userRequest(QStringLiteral("x"));
        request.idleTimeoutMs = 20000;
        request.deadlineMs = 60000;
        const EncodedChatRequest encoded = encodeChatRequest(request);
        QCOMPARE(encoded.idleTimeoutMs, std::optional<int>(20000));
        QCOMPARE(encoded.deadlineMs, std::optional<int>(60000));
        QCOMPARE(encoded.body, encodeChatRequest(userRequest(QStringLiteral("x"))).body);
        QVERIFY(!encodeChatRequest(userRequest(QStringLiteral("x"))).deadlineMs);
    }

    void jsonContentIsParsed_data()
    {
        QTest::addColumn<QString>("content");
        QTest::newRow("bare") << QStringLiteral("{\"dish\":\"番茄炒蛋\"}");
        QTest::newRow("whitespace") << QStringLiteral("\n  {\"dish\":\"番茄炒蛋\"}  \n");
        QTest::newRow("json fence") << QStringLiteral("```json\n{\"dish\":\"番茄炒蛋\"}\n```");
        QTest::newRow("bare fence") << QStringLiteral("```\n{\"dish\":\"番茄炒蛋\"}\n```\n");
    }

    void jsonContentIsParsed()
    {
        QFETCH(QString, content);
        ChatError error;
        const auto object = parseJsonContent(withContent(content), &error);
        QVERIFY2(object, qPrintable(error.detail));
        QCOMPARE(object->value("dish").toString(), QStringLiteral("番茄炒蛋"));
    }

    void unusableJsonContentIsABadResponse_data()
    {
        QTest::addColumn<QString>("content");
        QTest::addColumn<QString>("finishReason");
        QTest::newRow("empty") << QString() << QStringLiteral("stop");
        QTest::newRow("prose") << QStringLiteral("抱歉，我无法回答") << QStringLiteral("stop");
        QTest::newRow("array") << QStringLiteral("[1,2]") << QStringLiteral("stop");
        QTest::newRow("cut off") << QStringLiteral("{\"dishes\":[{\"name\":\"番") << QStringLiteral("length");
        QTest::newRow("unclosed fence") << QStringLiteral("```json\n{\"a\":") << QStringLiteral("length");
    }

    void unusableJsonContentIsABadResponse()
    {
        QFETCH(QString, content);
        QFETCH(QString, finishReason);
        ChatError error;
        QVERIFY(!parseJsonContent(withContent(content, finishReason), &error));
        QCOMPARE(error.kind, ErrorKind::BadResponse);
        QVERIFY(error.detail.contains(finishReason));
        QVERIFY(!error.detail.contains(QStringLiteral("番"))); // model text stays out of logs
        QVERIFY(!error.detail.contains(QStringLiteral("抱歉")));
        QVERIFY(!parseJsonContent(withContent(content, finishReason))); // error is optional
    }

    void responseBodiesAreRead()
    {
        ChatError error;
        const auto result = parseChatResponse(chatCompletion(QStringLiteral("{}"), QStringLiteral("length"), 10, 5), &error);
        QVERIFY(result);
        QVERIFY(result->truncated);
        QCOMPARE(result->usage.totalTokens, 15);
        QVERIFY(!parseChatResponse("<html>", &error));
        QCOMPARE(error.kind, ErrorKind::BadResponse);
        QCOMPARE(errorKindFromString(enumKey(ErrorKind::QuotaOrRateLimit)), std::optional(ErrorKind::QuotaOrRateLimit));
        QVERIFY(!errorKindFromString(QStringLiteral("nonsense")));
    }
};

QTEST_GUILESS_MAIN(ChatTypesTest)

#include "chat-types-test.moc"
