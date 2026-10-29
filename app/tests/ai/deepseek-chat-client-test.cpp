#include "ai/api-keys.h"
#include "ai/deepseek-chat-client.h"
#include "ai/recorded-chat-client.h"

#include "ai-test-support.h"
#include "fake-http-server.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTest>

using namespace whilefresh::ai;
using namespace whilefresh::ai::testing;

namespace {

constexpr auto kTestKey = "test-key-9f3a7c";

QStringList g_log;

void captureLog(QtMsgType, const QMessageLogContext &, const QString &message)
{
    g_log.append(message);
}

struct Fixture {
    FakeHttpServer server;
    std::shared_ptr<StaticCredentialProvider> credentials =
        std::make_shared<StaticCredentialProvider>(QString::fromLatin1(kTestKey));
    DeepSeekChatClient client{ credentials };
    QSignalSpy succeeded{ &client, &ChatClient::succeeded };
    QSignalSpy failed{ &client, &ChatClient::failed };

    Fixture()
    {
        if (!server.listen())
            qFatal("fake server cannot listen");
        client.setEndpoint(server.url());
        client.setIdleTimeoutMs(5000);
        client.setDeadlineMs(20000);
        client.setRetryDelayMs(0);
    }

    quint64 send(const QString &text = QStringLiteral("x")) { return client.send(userRequest(text)); }

    void waitForAnswer()
    {
        QTRY_VERIFY_WITH_TIMEOUT(succeeded.count() + failed.count() > 0, 20000);
    }

    ChatError error() const { return failed.at(0).at(1).value<ChatError>(); }
    ChatResult result() const { return succeeded.at(0).at(1).value<ChatResult>(); }
};

}

class DeepSeekChatClientTest : public QObject
{
    Q_OBJECT

private:
    QtMessageHandler m_previousHandler = nullptr;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ChatResult>();
        qRegisterMetaType<ChatError>();
        m_previousHandler = qInstallMessageHandler(captureLog);
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(m_previousHandler);
    }

    void successPostsTheEncodedBodyWithTheKeyInAHeader()
    {
        g_log.clear();
        Fixture f;
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{\"秘方\":\"红烧\"}"), QStringLiteral("stop"), 300, 40) });

        const ChatRequest request = userRequest(QStringLiteral("冰箱里有五花肉"));
        const quint64 id = f.client.send(request);
        QCOMPARE(f.succeeded.count(), 0);
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.succeeded.at(0).at(0).toULongLong(), id);

        QCOMPARE(f.server.received().size(), 1);
        const auto &received = f.server.received().first();
        QVERIFY(received.requestLine.startsWith("POST /chat/completions"));
        QCOMPARE(received.headers.value("authorization"), QByteArray("Bearer ") + kTestKey);
        QVERIFY(received.headers.value("content-type").startsWith("application/json"));
        QCOMPARE(received.body, encodeChatRequest(request).body);
        QVERIFY(!received.body.contains(kTestKey));

        const QJsonObject body = QJsonDocument::fromJson(received.body).object();
        QCOMPARE(body.value("model").toString(), QStringLiteral("deepseek-flash"));
        QCOMPARE(body.value("response_format").toObject().value("type").toString(), QStringLiteral("json_object"));
        QCOMPARE(body.value("max_tokens").toInt(), 4000);
        const QJsonArray messages = body.value("messages").toArray();
        QCOMPARE(messages.at(0).toObject().value("role").toString(), QStringLiteral("system"));
        QVERIFY(messages.at(0).toObject().value("content").isString());

        const ChatResult result = f.result();
        QCOMPARE(result.content, QStringLiteral("{\"秘方\":\"红烧\"}"));
        QCOMPARE(result.finishReason, QStringLiteral("stop"));
        QVERIFY(!result.truncated);
        QCOMPARE(result.usage.promptTokens, 300);
        QCOMPARE(result.usage.totalTokens, 340);
        QVERIFY(result.elapsedMs >= 0);
        QVERIFY(!result.rawBody.isEmpty());

        // Logs carry sizes, counts and status codes only.
        QVERIFY(!g_log.isEmpty());
        const QString log = g_log.join(QLatin1Char('\n'));
        QVERIFY(!log.contains(QString::fromLatin1(kTestKey)));
        QVERIFY(!log.contains(QStringLiteral("Bearer")));
        QVERIFY(!log.contains(QStringLiteral("五花肉")));
        QVERIFY(!log.contains(QStringLiteral("只输出")));
        QVERIFY(!log.contains(QStringLiteral("红烧")));
    }

    void imagesAreSentAsDataUrlsAndNeverLogged()
    {
        g_log.clear();
        Fixture f;
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        ChatRequest request = userRequest(QStringLiteral("看图"));
        request.messages.last().parts.append(ChatPart::jpegImage(QByteArray("\xff\xd8 fake jpeg")));
        f.client.send(request);
        f.waitForAnswer();

        const QJsonArray content = QJsonDocument::fromJson(f.server.received().first().body)
                                       .object().value("messages").toArray().at(1).toObject()
                                       .value("content").toArray();
        QCOMPARE(content.size(), 2);
        QCOMPARE(content.at(1).toObject().value("type").toString(), QStringLiteral("image_url"));
        QVERIFY(content.at(1).toObject().value("image_url").toObject().value("url").toString()
                    .startsWith(QStringLiteral("data:image/jpeg;base64,")));
        const QString log = g_log.join(QLatin1Char('\n'));
        QVERIFY(log.contains(QStringLiteral("images=1")));
        QVERIFY(!log.contains(QStringLiteral("data:image")));
        QVERIFY(!log.contains(QStringLiteral("base64")));
    }

    void statusFailures_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<ErrorKind>("kind");
        QTest::addColumn<int>("requests");

        QTest::newRow("401") << 401 << ErrorKind::AuthFailed << 1;
        QTest::newRow("403") << 403 << ErrorKind::AuthFailed << 1;
        QTest::newRow("402 insufficient balance") << 402 << ErrorKind::QuotaOrRateLimit << 1;
        QTest::newRow("429") << 429 << ErrorKind::QuotaOrRateLimit << 1;
        QTest::newRow("400") << 400 << ErrorKind::RequestRejected << 1;
        QTest::newRow("422") << 422 << ErrorKind::RequestRejected << 1;
        QTest::newRow("500 is retried once") << 500 << ErrorKind::ServerError << 2;
        QTest::newRow("503 is retried once") << 503 << ErrorKind::ServerError << 2;
    }

    void statusFailures()
    {
        QFETCH(int, status);
        QFETCH(ErrorKind, kind);
        QFETCH(int, requests);

        Fixture f;
        for (int i = 0; i < 3; ++i)
            f.server.enqueue({ status, R"({"error":{"message":"scripted"}})" });
        f.send();
        f.waitForAnswer();
        QTest::qWait(200); // a further attempt would show up as another request

        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.failed.count(), 1);
        QCOMPARE(f.error().kind, kind);
        QCOMPARE(f.error().httpStatus, status);
        QVERIFY(f.error().detail.contains(QString::number(status)));
        QCOMPARE(f.server.received().size(), requests);
    }

    void clientErrorsAreNeverRetried()
    {
        for (int status : { 400, 401, 402, 403, 404, 409, 413, 422, 429 }) {
            Fixture f;
            f.server.enqueue({ status, R"({"error":{"message":"scripted"}})" });
            f.server.enqueue({ 200, chatCompletion(QStringLiteral("must not be fetched")) });
            f.send();
            f.waitForAnswer();
            QTest::qWait(100);
            QCOMPARE(f.succeeded.count(), 0);
            QCOMPARE(f.error().httpStatus, status);
            QCOMPARE(f.server.received().size(), 1);
        }
    }

    void onlyServerErrorsAndDroppedConnectionsAreRetryable()
    {
        QVERIFY(isRetryableFailure(QNetworkReply::InternalServerError, 500));
        QVERIFY(isRetryableFailure(QNetworkReply::ServiceUnavailableError, 503));
        QVERIFY(isRetryableFailure(QNetworkReply::RemoteHostClosedError, 0));
        QVERIFY(isRetryableFailure(QNetworkReply::RemoteHostClosedError, 200)); // cut off mid-answer
        QVERIFY(!isRetryableFailure(QNetworkReply::ProtocolFailure, 0));
        QVERIFY(!isRetryableFailure(QNetworkReply::ProtocolInvalidOperationError, 0));
        QVERIFY(!isRetryableFailure(QNetworkReply::ProtocolUnknownError, 0));
        QVERIFY(!isRetryableFailure(QNetworkReply::TimeoutError, 0));
        QVERIFY(!isRetryableFailure(QNetworkReply::OperationCanceledError, 0));
        QVERIFY(!isRetryableFailure(QNetworkReply::ContentNotFoundError, 404));
        QVERIFY(!isRetryableFailure(QNetworkReply::ConnectionRefusedError, 0));
        // Still reported as a server problem to the user. (A local server cannot make Qt report
        // a protocol error reliably, so the predicate is tested directly.)
        QCOMPARE(kindForNetworkError(QNetworkReply::ProtocolFailure), ErrorKind::ServerError);
    }

    void serverErrorThenSuccessRecovers()
    {
        Fixture f;
        f.server.enqueue({ 500, "boom" });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.server.received().size(), 2);
    }

    void aConnectionTheServerDropsIsRetriedOnce()
    {
        // Qt itself may resend once on a dropped connection, so attempts are counted at the client.
        g_log.clear();
        Fixture f;
        for (int i = 0; i < 8; ++i)
            f.server.enqueue({ 200, {}, false, 0, true });
        f.send();
        f.waitForAnswer();
        QTest::qWait(200);
        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.error().kind, ErrorKind::ServerError);
        QCOMPARE(g_log.filter(QStringLiteral(" sending: ")).size(), 2);
        QCOMPARE(g_log.filter(QStringLiteral("attempt=2")).size(), 2); // sent and failed once more
    }

    void silenceBeyondTheIdleTimeoutIsATimeoutAndNotRetried()
    {
        Fixture f;
        f.client.setIdleTimeoutMs(300);
        f.server.enqueue({ 200, {}, true });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Timeout);
        QTest::qWait(300);
        QCOMPARE(f.server.received().size(), 1);
    }

    void aSlowAnswerWithinBothLimitsSucceeds()
    {
        // The model works silently before answering; that must not look like a dead connection.
        Fixture f;
        f.client.setIdleTimeoutMs(3000);
        f.client.setDeadlineMs(6000);
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")), false, 800 });
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.server.received().size(), 1);
        QVERIFY(f.result().elapsedMs >= 700);
    }

    void exceedingTheOverallDeadlineIsATimeoutAndNotRetried()
    {
        Fixture f;
        f.client.setIdleTimeoutMs(10000);
        f.client.setDeadlineMs(300);
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")), false, 1500 });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        QElapsedTimer timer;
        timer.start();
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Timeout);
        QVERIFY2(timer.elapsed() < 1200, "gave up at the deadline, not when the server answered");
        QTest::qWait(300);
        QCOMPARE(f.server.received().size(), 1);
        QCOMPARE(f.succeeded.count(), 0);
    }

    void eachCallHasItsOwnDeadline()
    {
        Fixture f;
        f.client.setDeadlineMs(1500);
        // Each call takes 800 ms: over 1500 ms in total, but within the deadline per call.
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")), false, 800 });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")), false, 800 });
        f.send();
        QTRY_COMPARE_WITH_TIMEOUT(f.succeeded.count(), 1, 10000);
        f.send();
        QTRY_COMPARE_WITH_TIMEOUT(f.succeeded.count(), 2, 10000);
        QCOMPARE(f.failed.count(), 0);
    }

    void theDeadlineCoversTheRetry()
    {
        Fixture f;
        f.client.setDeadlineMs(1000);
        // Each attempt alone is within the deadline, both together are not.
        f.server.enqueue({ 503, "busy", false, 600 });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")), false, 800 });
        QElapsedTimer timer;
        timer.start();
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Timeout);
        QVERIFY2(timer.elapsed() < 1350, "gave up at the deadline of the call");
        QCOMPARE(f.server.received().size(), 2);
    }

    void theDeadlineEndsAWaitForTheRetry()
    {
        Fixture f;
        f.client.setDeadlineMs(300);
        f.client.setRetryDelayMs(2000);
        f.server.enqueue({ 503, "busy" });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        QElapsedTimer timer;
        timer.start();
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Timeout);
        QVERIFY(timer.elapsed() < 1500);
        QTest::qWait(300);
        QCOMPARE(f.server.received().size(), 1);
    }

    void aRequestCanOverrideTheLimits()
    {
        Fixture f; // client limits: 5 s idle, 20 s deadline
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")), false, 1500 });
        ChatRequest shortDeadline = userRequest(QStringLiteral("x"));
        shortDeadline.deadlineMs = 300;
        QElapsedTimer timer;
        timer.start();
        f.client.send(shortDeadline);
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Timeout);
        QVERIFY(timer.elapsed() < 1200);

        f.server.enqueue({ 200, {}, true });
        ChatRequest shortIdle = userRequest(QStringLiteral("y"));
        shortIdle.idleTimeoutMs = 300;
        timer.restart();
        f.client.send(shortIdle);
        QTRY_COMPARE_WITH_TIMEOUT(f.failed.count(), 2, 5000);
        QCOMPARE(f.failed.at(1).at(1).value<ChatError>().kind, ErrorKind::Timeout);
        QVERIFY(timer.elapsed() < 3000);

        // Limits are not part of the request body.
        QCOMPARE(encodeChatRequest(shortDeadline).body, encodeChatRequest(userRequest(QStringLiteral("x"))).body);
    }

    void aCutOffAnswerIsAPartialSuccessFlaggedTruncated()
    {
        Fixture f;
        const QString partial = QStringLiteral("{\"dishes\":[{\"name\":\"番茄炒蛋\"},{\"na");
        f.server.enqueue({ 200, chatCompletion(partial, QStringLiteral("length"), 900, 4000) });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QVERIFY(f.result().truncated);
        QCOMPARE(f.result().finishReason, QStringLiteral("length"));
        QCOMPARE(f.result().content, partial);
        QTest::qWait(200);
        QCOMPARE(f.server.received().size(), 1); // no automatic second attempt
    }

    void unparsableAnswersAreBadResponsesAndNotRetried_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("html") << QByteArray("<html>gateway</html>");
        QTest::newRow("no content") << QByteArray(R"({"choices":[{"message":{},"finish_reason":"stop"}]})");
    }

    void unparsableAnswersAreBadResponsesAndNotRetried()
    {
        QFETCH(QByteArray, body);
        Fixture f;
        f.server.enqueue({ 200, body });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::BadResponse);
        QTest::qWait(200);
        QCOMPARE(f.server.received().size(), 1);
    }

    void cancellingDuringTheRetryWaitStopsTheRetry()
    {
        Fixture f;
        f.client.setRetryDelayMs(600);
        f.server.enqueue({ 500, "boom" });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        const quint64 id = f.send();
        QTRY_COMPARE_WITH_TIMEOUT(f.server.received().size(), 1, 10000);
        QTest::qWait(150); // the failed answer has been processed, the retry is waiting
        f.client.cancel(id);
        QTest::qWait(1000);
        QCOMPARE(f.server.received().size(), 1);
        QCOMPARE(f.succeeded.count() + f.failed.count(), 0);
    }

    void cancelledRequestsStaySilentAndTheClientStaysUsable()
    {
        Fixture f;
        f.server.enqueue({ 200, {}, true });
        const quint64 id = f.send();
        QTRY_COMPARE_WITH_TIMEOUT(f.server.received().size(), 1, 10000);
        f.client.cancel(id);
        QTest::qWait(300);
        QCOMPARE(f.succeeded.count() + f.failed.count(), 0);

        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        const quint64 next = f.send();
        QVERIFY(next != id);
        f.waitForAnswer();
        QCOMPARE(f.succeeded.count(), 1);
        QCOMPARE(f.succeeded.at(0).at(0).toULongLong(), next);
    }

    void unreachableServerIsNoNetwork()
    {
        quint16 closedPort = 0;
        {
            FakeHttpServer gone;
            QVERIFY(gone.listen());
            closedPort = gone.port();
        }
        Fixture f;
        f.client.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(closedPort)));
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::NoNetwork);
    }

    void aFailedTlsHandshakeIsNotReportedAsNoNetwork()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("no TLS backend");
        Fixture f;
        f.server.setGarbageResponse("HTTP/1.1 400 Bad Request\r\n\r\n"); // answers a ClientHello in plain text
        f.client.setEndpoint(QUrl(QStringLiteral("https://127.0.0.1:%1/chat/completions").arg(f.server.port())));
        f.send();
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::SecureConnectionFailed);
        QCOMPARE(f.server.received().size(), 0);
    }

    void missingKeyIsNotConfiguredAndSendsNothing()
    {
        Fixture f;
        f.credentials->setKey(Service::DeepSeek, QString());
        QVERIFY(!f.client.isConfigured());
        const quint64 id = f.send();
        QCOMPARE(f.failed.count(), 0); // asynchronous like every other outcome
        f.waitForAnswer();
        QCOMPARE(f.failed.at(0).at(0).toULongLong(), id);
        QCOMPARE(f.error().kind, ErrorKind::NotConfigured);
        QTest::qWait(100);
        QCOMPARE(f.server.received().size(), 0);

        // Cancelling such a request silences it as well.
        const quint64 cancelled = f.send();
        f.client.cancel(cancelled);
        QTest::qWait(100);
        QCOMPARE(f.failed.count(), 1);

        DeepSeekChatClient withoutProvider(nullptr);
        QVERIFY(!withoutProvider.isConfigured());
    }

    void theKeyIsReadWhenARequestIsSent()
    {
        Fixture f;
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("{}")) });
        f.send();
        QTRY_COMPARE_WITH_TIMEOUT(f.succeeded.count(), 1, 10000);
        f.client.setCredentials(std::make_shared<StaticCredentialProvider>(QStringLiteral("user-key-2")));
        f.send();
        QTRY_COMPARE_WITH_TIMEOUT(f.succeeded.count(), 2, 10000);
        QCOMPARE(f.server.received().at(0).headers.value("authorization"), QByteArray("Bearer ") + kTestKey);
        QCOMPARE(f.server.received().at(1).headers.value("authorization"), QByteArray("Bearer user-key-2"));
    }

    void consumersSharingAClientOnlySeeTheirOwnAnswers()
    {
        Fixture f;
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("for a")) });
        f.server.enqueue({ 200, chatCompletion(QStringLiteral("for b")) });
        RecordingChatClient a(&f.client);
        RecordingChatClient b(&f.client);
        QStringList seenByA, seenByB;
        connect(&a, &ChatClient::succeeded, this, [&](quint64, const ChatResult &r) { seenByA.append(r.content); });
        connect(&b, &ChatClient::succeeded, this, [&](quint64, const ChatResult &r) { seenByB.append(r.content); });
        a.send(userRequest(QStringLiteral("a")));
        QTRY_COMPARE_WITH_TIMEOUT(seenByA.size(), 1, 10000);
        b.send(userRequest(QStringLiteral("b")));
        QTRY_COMPARE_WITH_TIMEOUT(seenByB.size(), 1, 10000);
        QCOMPARE(seenByA, QStringList{ QStringLiteral("for a") });
        QCOMPARE(seenByB, QStringList{ QStringLiteral("for b") });
        QCOMPARE(a.recording().exchanges.size(), 1);
        QCOMPARE(b.recording().exchanges.size(), 1);
        QCOMPARE(f.succeeded.count(), 2); // the shared client itself answered both
    }

    void destroyingTheClientMidFlightEmitsNothing_data()
    {
        QTest::addColumn<bool>("waitingToRetry");
        QTest::newRow("reply in flight") << false;
        QTest::newRow("waiting to retry") << true;
    }

    void destroyingTheClientMidFlightEmitsNothing()
    {
        QFETCH(bool, waitingToRetry);
        FakeHttpServer server;
        QVERIFY(server.listen());
        if (waitingToRetry)
            server.enqueue({ 503, "busy" });
        server.enqueue({ 200, {}, true });

        auto *client = new DeepSeekChatClient(std::make_shared<StaticCredentialProvider>(QStringLiteral("k")));
        client->setEndpoint(server.url());
        client->setRetryDelayMs(waitingToRetry ? 5000 : 0);
        int answers = 0;
        QObject probe;
        connect(client, &ChatClient::succeeded, &probe, [&] { ++answers; });
        connect(client, &ChatClient::failed, &probe, [&] { ++answers; });
        client->send(userRequest(QStringLiteral("x")));
        QTRY_COMPARE_WITH_TIMEOUT(server.received().size(), 1, 10000);
        QTest::qWait(100);
        delete client;
        QTest::qWait(300);
        QCOMPARE(answers, 0);
    }

    void builtinProviderHasNoLexmountKey()
    {
        QVERIFY(!BuiltinCredentialProvider().isConfigured(Service::Lexmount));
        StaticCredentialProvider keys(QStringLiteral("a"));
        QVERIFY(keys.isConfigured(Service::DeepSeek));
        QVERIFY(!keys.isConfigured(Service::Lexmount));
        keys.setKey(Service::Lexmount, QStringLiteral("b"));
        QCOMPARE(keys.key(Service::Lexmount), QStringLiteral("b"));
    }

    void networkAndStatusErrorsMapToTheRightKinds()
    {
        QCOMPARE(kindForNetworkError(QNetworkReply::SslHandshakeFailedError), ErrorKind::SecureConnectionFailed);
        QCOMPARE(kindForNetworkError(QNetworkReply::ConnectionRefusedError), ErrorKind::NoNetwork);
        QCOMPARE(kindForNetworkError(QNetworkReply::HostNotFoundError), ErrorKind::NoNetwork);
        QCOMPARE(kindForNetworkError(QNetworkReply::TemporaryNetworkFailureError), ErrorKind::NoNetwork);
        QCOMPARE(kindForNetworkError(QNetworkReply::TimeoutError), ErrorKind::Timeout);
        QCOMPARE(kindForNetworkError(QNetworkReply::RemoteHostClosedError), ErrorKind::ServerError);
        QCOMPARE(kindForHttpStatus(401), ErrorKind::AuthFailed);
        QCOMPARE(kindForHttpStatus(429), ErrorKind::QuotaOrRateLimit);
        QCOMPARE(kindForHttpStatus(503), ErrorKind::ServerError);
        QCOMPARE(kindForHttpStatus(404), ErrorKind::RequestRejected);
    }
};

QTEST_GUILESS_MAIN(DeepSeekChatClientTest)

#include "deepseek-chat-client-test.moc"
