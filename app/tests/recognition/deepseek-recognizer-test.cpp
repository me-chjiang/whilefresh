#include "recognition/deepseek-recognizer.h"
#include "recognition/request-builder.h"

#include "fake-http-server.h"
#include "recognition-test-support.h"

#include <QElapsedTimer>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::recognition;
using namespace whilefresh::recognition::testing;

namespace {

constexpr auto kTestKey = "test-key-9f3a7c";

QStringList g_log;

void captureLog(QtMsgType, const QMessageLogContext &, const QString &message)
{
    g_log.append(message);
}

QByteArray okBody(const QStringList &items, int promptTokens = 1000, int completionTokens = 200)
{
    return chatCompletion(modelJson(items), promptTokens, completionTokens);
}

struct Fixture {
    FakeHttpServer server;
    DeepSeekRecognizer recognizer;
    QSignalSpy succeeded{ &recognizer, &RecognitionService::succeeded };
    QSignalSpy failed{ &recognizer, &RecognitionService::failed };

    Fixture()
    {
        if (!server.listen())
            qFatal("fake server cannot listen");
        recognizer.setApiKey(QString::fromLatin1(kTestKey));
        recognizer.setEndpoint(server.url());
        recognizer.setIdleTimeoutMs(5000);
        recognizer.setDeadlineMs(20000);
        recognizer.setRetryDelayMs(0);
    }

    void waitForAnswer()
    {
        QTRY_VERIFY_WITH_TIMEOUT(succeeded.count() + failed.count() > 0, 20000);
    }

    RecognitionError error() const { return failed.at(0).at(1).value<RecognitionError>(); }
    RecognitionResult result() const { return succeeded.at(0).at(1).value<RecognitionResult>(); }
};

}

class DeepSeekRecognizerTest : public QObject
{
    Q_OBJECT

private:
    // A 1100x3000 image slices into 3 pieces (0-1500, 1300-2800, 2600-3000).
    RecognitionRequest imageRequest() const
    {
        RecognitionRequest request;
        request.kind = InputKind::Screenshot;
        request.images = { { m_imagePath, QDate(2026, 10, 18) } };
        request.vocabulary = syntheticVocabulary();
        return request;
    }

    RecognitionRequest textRequest(const QString &text) const
    {
        RecognitionRequest request;
        request.kind = InputKind::OrderText;
        request.text = text;
        request.vocabulary = syntheticVocabulary();
        return request;
    }

    QTemporaryDir m_dir;
    QString m_imagePath;
    QtMessageHandler m_previousHandler = nullptr;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<RecognitionResult>();
        qRegisterMetaType<RecognitionError>();

        QVERIFY(m_dir.isValid());
        m_imagePath = m_dir.filePath(QStringLiteral("cart.png"));
        QImage image(1100, 3000, QImage::Format_RGB32);
        image.fill(Qt::lightGray);
        QVERIFY(image.save(m_imagePath));

        m_previousHandler = qInstallMessageHandler(captureLog);
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(m_previousHandler);
    }

    void successSendsAWellFormedRequest()
    {
        g_log.clear();
        Fixture f;
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋")) }) });

        const quint64 id = f.recognizer.recognize(imageRequest());
        QCOMPARE(f.succeeded.count(), 0);
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.succeeded.at(0).at(0).toULongLong(), id);

        QCOMPARE(f.server.received().size(), 1);
        const auto &received = f.server.received().first();
        QVERIFY(received.requestLine.startsWith("POST /chat/completions"));
        QCOMPARE(received.headers.value("authorization"), QByteArray("Bearer ") + kTestKey);
        QVERIFY(received.headers.value("content-type").startsWith("application/json"));

        const QJsonObject body = QJsonDocument::fromJson(received.body).object();
        QCOMPARE(body.value("model").toString(), QStringLiteral("deepseek-flash"));
        QCOMPARE(body.value("response_format").toObject().value("type").toString(), QStringLiteral("json_object"));
        const QJsonArray content = body.value("messages").toArray().at(0).toObject().value("content").toArray();
        QCOMPARE(content.size(), 1 + 3 * 2);
        int images = 0;
        for (const QJsonValue &block : content) {
            if (block.toObject().value("type").toString() == QLatin1String("image_url")) {
                ++images;
                QVERIFY(block.toObject().value("image_url").toObject().value("url").toString()
                            .startsWith(QStringLiteral("data:image/jpeg;base64,")));
            }
        }
        QCOMPARE(images, 3);
        QVERIFY(!received.body.contains(kTestKey));

        const RecognitionResult result = f.result();
        QCOMPARE(result.items.size(), 1);
        QCOMPARE(result.items.first().canonicalName, QStringLiteral("鸡蛋"));
        QCOMPARE(result.usage.totalTokens, 1200);
        QCOMPARE(result.rawResponses.size(), 1);
        QVERIFY(result.elapsedMs >= 0);

        // Logs carry sizes and status codes only.
        QVERIFY(!g_log.isEmpty());
        const QString log = g_log.join(QLatin1Char('\n'));
        QVERIFY(!log.contains(QString::fromLatin1(kTestKey)));
        QVERIFY(!log.contains(QStringLiteral("Bearer")));
        QVERIFY(!log.contains(QStringLiteral("base64")));
        QVERIFY(!log.contains(QStringLiteral("data:image")));
        QVERIFY(!log.contains(QStringLiteral("标准食材词表")));
        QVERIFY(!log.contains(QStringLiteral("鸡蛋")));
    }

    void orderTextIsSentAsDelimitedText()
    {
        Fixture f;
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("五花肉"), QStringLiteral("五花肉")) }) });
        f.recognizer.recognize(textRequest(QStringLiteral("猪五花肉 500g x1")));
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);

        const QJsonObject body = QJsonDocument::fromJson(f.server.received().first().body).object();
        const QJsonArray content = body.value("messages").toArray().at(0).toObject().value("content").toArray();
        QCOMPARE(content.size(), 2);
        QVERIFY(content.at(1).toObject().value("text").toString().contains(QStringLiteral("猪五花肉 500g x1")));
        QCOMPARE(f.result().sourceType, SourceType::Cart); // as declared by the model
    }

    void failures_data()
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

    void failures()
    {
        QFETCH(int, status);
        QFETCH(ErrorKind, kind);
        QFETCH(int, requests);

        Fixture f;
        for (int i = 0; i < requests; ++i)
            f.server.enqueue({ status, R"({"error":{"message":"scripted"}})" });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();

        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.error().kind, kind);
        QCOMPARE(f.error().httpStatus, status);
        QVERIFY(!f.error().userMessage.isEmpty());
        QVERIFY(f.error().detail.contains(QString::number(status)));
        QVERIFY(!f.error().userMessage.contains(QString::number(status)));
        QCOMPARE(f.server.received().size(), requests);
    }

    void serverErrorThenSuccessRecovers()
    {
        Fixture f;
        f.server.enqueue({ 500, "boom" });
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.server.received().size(), 2);
        QCOMPARE(f.result().items.size(), 1);
    }

    void silenceBeyondTheIdleTimeoutIsATimeoutAndNotRetried()
    {
        Fixture f;
        f.recognizer.setIdleTimeoutMs(300);
        f.server.enqueue({ 200, {}, true });
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Timeout);
        QTest::qWait(300); // a retry would show up as a second request
        QCOMPARE(f.server.received().size(), 1);
    }

    void aSlowAnswerWithinBothLimitsSucceeds()
    {
        // The model works silently before answering; that must not look like a dead connection.
        Fixture f;
        f.recognizer.setIdleTimeoutMs(3000);
        f.recognizer.setDeadlineMs(6000);
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }), false, 800 });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.server.received().size(), 1);
        QVERIFY(f.result().elapsedMs >= 700);
    }

    void exceedingTheOverallDeadlineIsATimeoutAndNotRetried()
    {
        Fixture f;
        f.recognizer.setIdleTimeoutMs(10000);
        f.recognizer.setDeadlineMs(300);
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }), false, 1500 });
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        QElapsedTimer timer;
        timer.start();
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Timeout);
        QVERIFY2(timer.elapsed() < 1200, "gave up at the deadline, not when the server answered");
        QTest::qWait(300);
        QCOMPARE(f.server.received().size(), 1);
        QCOMPARE(f.succeeded.count(), 0);
    }

    void theDeadlineAppliesToEachBatch()
    {
        Fixture f;
        BatchLimits limits;
        limits.maxImages = 2;
        f.recognizer.setBatchLimits(limits);
        f.recognizer.setDeadlineMs(1500);
        // Each batch takes 800 ms: over 1500 ms in total, but within the deadline per request.
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }), false, 800 });
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("洋葱"), QStringLiteral("洋葱")) }), false, 800 });
        f.recognizer.recognize(imageRequest());
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.result().items.size(), 2);
    }

    void cancellingDuringTheRetryWaitStopsTheRetry()
    {
        Fixture f;
        f.recognizer.setRetryDelayMs(600);
        f.server.enqueue({ 500, "boom" });
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        const quint64 id = f.recognizer.recognize(textRequest(QStringLiteral("x")));
        QTRY_COMPARE_WITH_TIMEOUT(f.server.received().size(), 1, 10000);
        QTest::qWait(150); // the failed answer has been processed, the retry is waiting
        f.recognizer.cancel(id);
        QTest::qWait(1000);
        QCOMPARE(f.server.received().size(), 1);
        QCOMPARE(f.succeeded.count() + f.failed.count(), 0);
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
        f.recognizer.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(closedPort)));
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::NoNetwork);
        QVERIFY(!f.error().userMessage.isEmpty());
    }

    void missingKeyIsReportedWithoutTouchingTheNetwork()
    {
        Fixture f;
        f.recognizer.setApiKey(QString());
        const quint64 id = f.recognizer.recognize(textRequest(QStringLiteral("x")));
        QCOMPARE(f.failed.count(), 0); // asynchronous like every other outcome
        f.waitForAnswer();
        QCOMPARE(f.failed.at(0).at(0).toULongLong(), id);
        QCOMPARE(f.error().kind, ErrorKind::NotConfigured);
        QCOMPARE(f.server.received().size(), 0);
    }

    void unparsableAnswersAreNotRetried_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("html") << QByteArray("<html>gateway</html>");
        QTest::newRow("prose") << chatCompletion(QStringLiteral("抱歉，无法识别"));
    }

    void unparsableAnswersAreNotRetried()
    {
        QFETCH(QByteArray, body);
        Fixture f;
        f.server.enqueue({ 200, body });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::BadResponse);
        QCOMPARE(f.server.received().size(), 1);
    }

    void invalidInputIsRejected()
    {
        Fixture f;

        RecognitionRequest noVocabulary = textRequest(QStringLiteral("x"));
        noVocabulary.vocabulary.reset();
        f.recognizer.recognize(noVocabulary);
        f.recognizer.recognize(textRequest(QStringLiteral("   ")));
        f.recognizer.recognize(textRequest(QString(kMaxOrderTextChars + 1, QLatin1Char('x'))));
        RecognitionRequest noImages = imageRequest();
        noImages.images.clear();
        f.recognizer.recognize(noImages);
        RecognitionRequest unreadable = imageRequest();
        unreadable.images = { { QStringLiteral("/nonexistent/cart.png"), {} } };
        f.recognizer.recognize(unreadable);

        QTRY_COMPARE_WITH_TIMEOUT(f.failed.count(), 5, 10000);
        for (const auto &args : f.failed)
            QCOMPARE(args.at(1).value<RecognitionError>().kind, ErrorKind::InvalidInput);
        QCOMPARE(f.server.received().size(), 0);
    }

    void largeImportsAreSplitIntoBatchesAndMerged()
    {
        Fixture f;
        BatchLimits limits;
        limits.maxImages = 2;
        f.recognizer.setBatchLimits(limits);
        const QString egg = itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋"), QStringLiteral(R"("quantity_text":"10枚")"));
        const QString onion = itemJson(QStringLiteral("洋葱"), QStringLiteral("洋葱"));
        const QString milk = itemJson(QStringLiteral("牛奶"), QStringLiteral("牛奶"));
        f.server.enqueue({ 200, okBody({ egg, onion }, 1000, 100) });
        f.server.enqueue({ 200, okBody({ egg, milk }, 800, 80) });

        f.recognizer.recognize(imageRequest()); // 3 slices of one image -> 2 + 1
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);

        QCOMPARE(f.server.received().size(), 2);
        const auto imageBlocks = [&](int index) {
            const QJsonObject body = QJsonDocument::fromJson(f.server.received().at(index).body).object();
            const QJsonArray content = body.value("messages").toArray().at(0).toObject().value("content").toArray();
            int count = 0;
            for (const QJsonValue &block : content)
                count += block.toObject().value("type").toString() == QLatin1String("image_url");
            return count;
        };
        QCOMPARE(imageBlocks(0), 2);
        QCOMPARE(imageBlocks(1), 1);

        const RecognitionResult result = f.result();
        QCOMPARE(result.items.size(), 3); // the repeated egg is dropped
        QCOMPARE(result.rawResponses.size(), 2);
        QCOMPARE(result.usage.promptTokens, 1800);
        QCOMPARE(result.usage.totalTokens, 1980);
    }

    void aCutOffAnswerYieldsAPartialFlaggedResult()
    {
        Fixture f;
        const QString cut = QStringLiteral("{\"source_type\":\"cart\",\"items\":[%1,{\"raw_name\":\"牛奶\",\"cano")
                                .arg(itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋")));
        f.server.enqueue({ 200, chatCompletion(cut, 900, 131072, QStringLiteral("length")) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QVERIFY(f.result().truncated);
        QCOMPARE(f.result().items.size(), 1);
        QCOMPARE(f.server.received().size(), 1); // no automatic second attempt
    }

    void anEmptyAnswerThatSpentTheBudgetIsAnErrorNotZeroItems()
    {
        Fixture f;
        f.server.enqueue({ 200, chatCompletion(QString(), 900, 131072, QStringLiteral("length")) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.error().kind, ErrorKind::BadResponse);
        QCOMPARE(f.server.received().size(), 1);
    }

    void requestBodyCarriesTheConfiguredOptions()
    {
        Fixture f;
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });

        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QJsonObject body = QJsonDocument::fromJson(f.server.received().at(0).body).object();
        QCOMPARE(body.value("max_tokens").toInt(), 131072);
        QCOMPARE(body.value("temperature").toInt(), 0);
        QVERIFY(!body.contains("thinking"));

        RequestOptions options;
        options.thinking = false;
        options.maxTokens = 9000;
        f.recognizer.setRequestOptions(options);
        f.recognizer.recognize(textRequest(QStringLiteral("y")));
        QTRY_COMPARE_WITH_TIMEOUT(f.server.received().size(), 2, 10000);
        body = QJsonDocument::fromJson(f.server.received().at(1).body).object();
        QCOMPARE(body.value("max_tokens").toInt(), 9000);
        QCOMPARE(body.value("thinking").toObject().value("type").toString(), QStringLiteral("disabled"));
    }

    void aFailingLaterBatchFailsTheWholeImport()
    {
        Fixture f;
        BatchLimits limits;
        limits.maxImages = 2;
        f.recognizer.setBatchLimits(limits);
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        f.server.enqueue({ 401, "nope" });
        f.recognizer.recognize(imageRequest());
        f.waitForAnswer();
        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.error().kind, ErrorKind::AuthFailed);
    }

    void cancelledRequestsStaySilent()
    {
        Fixture f;
        f.server.enqueue({ 200, {}, true });
        const quint64 id = f.recognizer.recognize(textRequest(QStringLiteral("x")));
        QTRY_COMPARE_WITH_TIMEOUT(f.server.received().size(), 1, 10000);
        f.recognizer.cancel(id);
        QTest::qWait(300);
        QCOMPARE(f.succeeded.count() + f.failed.count(), 0);

        // The recognizer stays usable.
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        f.recognizer.recognize(textRequest(QStringLiteral("y")));
        f.waitForAnswer();
        QCOMPARE(f.succeeded.count(), 1);
    }

    void requestIdsAreUniqueAndAnswersMatchThem()
    {
        Fixture f;
        f.server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        const quint64 a = f.recognizer.recognize(textRequest(QStringLiteral("a")));
        f.waitForAnswer();
        f.server.enqueue({ 401, "x" });
        const quint64 b = f.recognizer.recognize(textRequest(QStringLiteral("b")));
        QTRY_COMPARE_WITH_TIMEOUT(f.failed.count(), 1, 10000);
        QVERIFY(a != b);
        QCOMPARE(f.succeeded.at(0).at(0).toULongLong(), a);
        QCOMPARE(f.failed.at(0).at(0).toULongLong(), b);
    }
};

QTEST_GUILESS_MAIN(DeepSeekRecognizerTest)

#include "deepseek-recognizer-test.moc"
