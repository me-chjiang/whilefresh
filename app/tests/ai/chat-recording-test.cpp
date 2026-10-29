#include "ai/api-keys.h"
#include "ai/chat-recording.h"
#include "ai/deepseek-chat-client.h"
#include "ai/recorded-chat-client.h"

#include "ai-test-support.h"
#include "fake-http-server.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::ai;
using namespace whilefresh::ai::testing;

namespace {

QString testDataPath(const QString &relative)
{
    return QDir(QStringLiteral(WHILEFRESH_TEST_DATA_DIR)).filePath(relative);
}

ChatExchange answer(const QByteArray &body, const QByteArray &digest = {})
{
    ChatExchange exchange;
    exchange.response = body;
    exchange.requestDigest = digest;
    return exchange;
}

// Sends `requests` one after another and returns each outcome: the content, or "error:<kind>".
QStringList runAll(ChatClient &client, const QList<ChatRequest> &requests)
{
    QStringList outcomes;
    QObject context;
    QObject::connect(&client, &ChatClient::succeeded, &context,
                     [&](quint64, const ChatResult &result) { outcomes.append(result.content); });
    QObject::connect(&client, &ChatClient::failed, &context, [&](quint64, const ChatError &error) {
        outcomes.append(QStringLiteral("error:") + enumKey(error.kind));
    });
    for (const ChatRequest &request : requests) {
        const qsizetype before = outcomes.size();
        client.send(request);
        if (!QTest::qWaitFor([&] { return outcomes.size() > before; }, 10000))
            return { QStringLiteral("no answer") };
    }
    return outcomes;
}

}

class ChatRecordingTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ChatResult>();
        qRegisterMetaType<ChatError>();
    }

    void replayAnswersInOrderAndKeepsTheRequests()
    {
        ChatExchange failure;
        failure.error = ChatError{ ErrorKind::QuotaOrRateLimit, QStringLiteral("HTTP 429"), 429 };
        RecordedChatClient client(ChatRecording{ { answer(chatCompletion(QStringLiteral("first"))),
                                                   answer(chatCompletion(QStringLiteral("second"))), failure } });
        QVERIFY(client.isConfigured());
        QSignalSpy succeeded(&client, &ChatClient::succeeded);
        QSignalSpy failed(&client, &ChatClient::failed);

        const quint64 a = client.send(userRequest(QStringLiteral("a")));
        const quint64 b = client.send(userRequest(QStringLiteral("b")));
        const quint64 c = client.send(userRequest(QStringLiteral("c")));
        QCOMPARE(succeeded.count(), 0); // never synchronous
        QTRY_COMPARE_WITH_TIMEOUT(succeeded.count() + failed.count(), 3, 5000);

        QCOMPARE(succeeded.at(0).at(0).toULongLong(), a);
        QCOMPARE(succeeded.at(0).at(1).value<ChatResult>().content, QStringLiteral("first"));
        QCOMPARE(succeeded.at(1).at(0).toULongLong(), b);
        QCOMPARE(succeeded.at(1).at(1).value<ChatResult>().content, QStringLiteral("second"));
        QCOMPARE(failed.at(0).at(0).toULongLong(), c);
        QCOMPARE(failed.at(0).at(1).value<ChatError>().kind, ErrorKind::QuotaOrRateLimit);
        QCOMPARE(failed.at(0).at(1).value<ChatError>().httpStatus, 429);

        QCOMPARE(client.requests().size(), 3);
        QCOMPARE(client.requests().at(1).body, encodeChatRequest(userRequest(QStringLiteral("b"))).body);
        QCOMPARE(client.remaining(), 0);

        client.send(userRequest(QStringLiteral("d")));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 2, 5000);
        QCOMPARE(failed.at(1).at(1).value<ChatError>().kind, ErrorKind::BadResponse); // recording exhausted
    }

    void aRecordedDigestOnlyAnswersTheSameRequest()
    {
        const QByteArray digest = requestDigest(encodeChatRequest(userRequest(QStringLiteral("same"))).body);
        RecordedChatClient client(ChatRecording{ { answer(chatCompletion(QStringLiteral("ok")), digest),
                                                   answer(chatCompletion(QStringLiteral("ok")), digest) } });
        QSignalSpy succeeded(&client, &ChatClient::succeeded);
        QSignalSpy failed(&client, &ChatClient::failed);

        client.send(userRequest(QStringLiteral("same")));
        QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 5000);
        client.send(userRequest(QStringLiteral("changed prompt")));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
        QCOMPARE(failed.at(0).at(1).value<ChatError>().kind, ErrorKind::BadResponse);
        QVERIFY(failed.at(0).at(1).value<ChatError>().detail.contains(QStringLiteral("re-record")));
    }

    void truncatedRecordedAnswersStayPartialSuccesses()
    {
        RecordedChatClient client(
            ChatRecording{ { answer(chatCompletion(QStringLiteral("{\"a\":[1,"), QStringLiteral("length"))) } });
        QSignalSpy succeeded(&client, &ChatClient::succeeded);
        client.send(userRequest(QStringLiteral("x")));
        QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 5000);
        QVERIFY(succeeded.at(0).at(1).value<ChatResult>().truncated);
        QCOMPARE(succeeded.at(0).at(1).value<ChatResult>().content, QStringLiteral("{\"a\":[1,"));
    }

    void cancelledReplaysStaySilent()
    {
        RecordedChatClient client(ChatRecording{ { answer(chatCompletion(QStringLiteral("x"))) } });
        QSignalSpy succeeded(&client, &ChatClient::succeeded);
        client.cancel(client.send(userRequest(QStringLiteral("x"))));
        QTest::qWait(50);
        QCOMPARE(succeeded.count(), 0);
    }

    void recordWriteReadReplayRoundTrip()
    {
        FakeHttpServer server;
        QVERIFY(server.listen());
        server.enqueue({ 200, chatCompletion(QStringLiteral("{\"dish\":\"番茄炒蛋\"}"), QStringLiteral("stop"), 120, 30) });
        server.enqueue({ 401, R"({"error":{"message":"bad key"}})" });
        server.enqueue({ 200, chatCompletion(QStringLiteral("{\"dish\":\"清炒"), QStringLiteral("length")) });

        DeepSeekChatClient network(std::make_shared<StaticCredentialProvider>(QStringLiteral("test-key")));
        network.setEndpoint(server.url());
        network.setRetryDelayMs(0);
        RecordingChatClient recorder(&network);
        QVERIFY(recorder.isConfigured());

        const QList<ChatRequest> requests{ userRequest(QStringLiteral("一")), userRequest(QStringLiteral("二")),
                                           userRequest(QStringLiteral("三")) };
        const QStringList live = runAll(recorder, requests);
        QCOMPARE(live, (QStringList{ QStringLiteral("{\"dish\":\"番茄炒蛋\"}"), QStringLiteral("error:auth-failed"),
                                     QStringLiteral("{\"dish\":\"清炒") }));

        const ChatRecording recorded = recorder.recording();
        QCOMPARE(recorded.exchanges.size(), 3);
        QCOMPARE(recorded.exchanges.at(0).request.messageCount, 2);
        QCOMPARE(recorded.exchanges.at(0).requestDigest, requestDigest(encodeChatRequest(requests.at(0)).body));
        QVERIFY(recorded.exchanges.at(1).error);
        QCOMPARE(recorded.exchanges.at(1).error->httpStatus, 401);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("chat.json"));
        QString error;
        QVERIFY2(writeChatRecording(path, recorded, &error), qPrintable(error));
        QCOMPARE(QFile::permissions(path) & (QFileDevice::ReadGroup | QFileDevice::ReadOther), QFileDevice::Permissions());

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        QVERIFY(!text.contains("test-key"));
        QVERIFY(!text.contains("只输出")); // prompts are summarised, never stored

        const auto loaded = readChatRecording(path, &error);
        QVERIFY2(loaded, qPrintable(error));
        RecordedChatClient replay(*loaded);
        QCOMPARE(runAll(replay, requests), live);
        QCOMPARE(server.received().size(), 3); // the replay stayed offline

        RecordedChatClient stale(*loaded);
        QCOMPARE(runAll(stale, { userRequest(QStringLiteral("改过的")) }), QStringList{ QStringLiteral("error:bad-response") });
    }

    void cancelledRequestsAreLeftOutOfARecording()
    {
        RecordedChatClient inner(ChatRecording{ { answer(chatCompletion(QStringLiteral("a"))),
                                                  answer(chatCompletion(QStringLiteral("b"))) } });
        RecordingChatClient recorder(&inner);
        QSignalSpy succeeded(&recorder, &ChatClient::succeeded);
        recorder.cancel(recorder.send(userRequest(QStringLiteral("a"))));
        recorder.send(userRequest(QStringLiteral("b")));
        QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 5000);
        QCOMPARE(recorder.recording().exchanges.size(), 1);
    }

    void existingFormat1RecordingsAreRead_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("exchanges");
        QTest::newRow("synthetic sample") << testDataPath(QStringLiteral("recordings/synthetic/cart-fresh-01.json")) << 1;
        QTest::newRow("two batches") << testDataPath(QStringLiteral("recordings/synthetic-two-batches.json")) << 2;
        QTest::newRow("demo preset") << QStringLiteral(WHILEFRESH_DEMO_ASSETS_DIR "/order-text.json") << 1;
    }

    void existingFormat1RecordingsAreRead()
    {
        QFETCH(QString, path);
        QFETCH(int, exchanges);

        QString error;
        const auto recording = readChatRecording(path, &error);
        QVERIFY2(recording, qPrintable(error));
        QCOMPARE(recording->exchanges.size(), exchanges);
        for (const ChatExchange &exchange : recording->exchanges) {
            QVERIFY(exchange.requestDigest.isEmpty()); // matches any request
            QVERIFY(!exchange.error);
        }

        RecordedChatClient client(*recording);
        QSignalSpy succeeded(&client, &ChatClient::succeeded);
        for (int i = 0; i < exchanges; ++i)
            client.send(userRequest(QStringLiteral("anything")));
        QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), exchanges, 5000);
        const ChatResult first = succeeded.at(0).at(1).value<ChatResult>();
        QVERIFY(first.content.contains(QStringLiteral("items")));
    }

    void format1RequestDigestsAreChecked()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("format-1.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray digest = requestDigest(encodeChatRequest(userRequest(QStringLiteral("recorded"))).body);
        const QJsonObject root{
            { "format", 1 },
            { "request", QJsonObject{ { "inputKind", "order-text" } } },
            { "requestDigests", QJsonArray{ QString::fromLatin1(digest) } },
            { "responses", QJsonArray{ QJsonDocument::fromJson(chatCompletion(QStringLiteral("{}"))).object() } },
        };
        file.write(QJsonDocument(root).toJson());
        file.close();

        const auto recording = readChatRecording(path);
        QVERIFY(recording);
        QCOMPARE(recording->exchanges.first().requestDigest, digest);
        RecordedChatClient same(*recording);
        QCOMPARE(runAll(same, { userRequest(QStringLiteral("recorded")) }), QStringList{ QStringLiteral("{}") });
        RecordedChatClient other(*recording);
        QCOMPARE(runAll(other, { userRequest(QStringLiteral("other")) }), QStringList{ QStringLiteral("error:bad-response") });
    }

    void unusableFilesAreRejected()
    {
        QTemporaryDir dir;
        const auto write = [&](const QString &name, const QByteArray &content) {
            QFile file(dir.filePath(name));
            if (file.open(QIODevice::WriteOnly))
                file.write(content);
            return file.fileName();
        };
        QString error;
        QVERIFY(!readChatRecording(dir.filePath(QStringLiteral("missing.json")), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!readChatRecording(write(QStringLiteral("garbage.json"), "not json")));
        QVERIFY(!readChatRecording(write(QStringLiteral("format-9.json"), R"({"format":9,"exchanges":[{}]})")));
        QVERIFY(!readChatRecording(write(QStringLiteral("empty.json"), R"({"format":2,"exchanges":[]})")));
    }
};

QTEST_GUILESS_MAIN(ChatRecordingTest)

#include "chat-recording-test.moc"
