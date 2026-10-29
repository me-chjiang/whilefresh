#include "ai/api-keys.h"
#include "ai/deepseek-chat-client.h"
#include "ai/recorded-chat-client.h"
#include "recognition/deepseek-recognizer.h"
#include "recognition/replay-recognizer.h"
#include "recognition/request-builder.h"

#include "fake-http-server.h"
#include "recognition-test-support.h"

#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::recognition;
using namespace whilefresh::recognition::testing;
namespace ai = whilefresh::ai;

namespace {

QStringList g_log;

void captureLog(QtMsgType, const QMessageLogContext &, const QString &message)
{
    g_log.append(message);
}

QByteArray okBody(const QStringList &items, int promptTokens = 1000, int completionTokens = 200)
{
    return chatCompletion(modelJson(items), promptTokens, completionTokens);
}

ai::ChatExchange answer(const QByteArray &body)
{
    ai::ChatExchange exchange;
    exchange.response = body;
    return exchange;
}

ai::ChatExchange failure(ErrorKind kind, int status = 0)
{
    ai::ChatExchange exchange;
    exchange.error = ai::ChatError{ kind, QStringLiteral("scripted %1").arg(status), status };
    return exchange;
}

// The recognizer in front of a recorded transport: what it sends is kept in client.requests().
struct Fixture {
    ai::RecordedChatClient client;
    DeepSeekRecognizer recognizer{ &client };
    QSignalSpy succeeded{ &recognizer, &RecognitionService::succeeded };
    QSignalSpy failed{ &recognizer, &RecognitionService::failed };

    explicit Fixture(const QList<ai::ChatExchange> &exchanges = {}) : client(ai::ChatRecording{ exchanges }) {}

    void waitForAnswer()
    {
        QTRY_VERIFY_WITH_TIMEOUT(succeeded.count() + failed.count() > 0, 20000);
    }

    qsizetype sent() const { return client.requests().size(); }
    QJsonObject sentBody(int index) const { return QJsonDocument::fromJson(client.requests().at(index).body).object(); }
    RecognitionError error() const { return failed.at(0).at(1).value<RecognitionError>(); }
    RecognitionResult result() const { return succeeded.at(0).at(1).value<RecognitionResult>(); }
};

int imageBlocks(const QJsonObject &body)
{
    const QJsonArray content = body.value("messages").toArray().at(0).toObject().value("content").toArray();
    int count = 0;
    for (const QJsonValue &block : content)
        count += block.toObject().value("type").toString() == QLatin1String("image_url");
    return count;
}

void compareResults(const RecognitionResult &actual, const RecognitionResult &expected)
{
    QCOMPARE(actual.sourceType, expected.sourceType);
    QCOMPARE(actual.ignoredSummary, expected.ignoredSummary);
    QCOMPARE(actual.truncated, expected.truncated);
    QCOMPARE(actual.usage.totalTokens, expected.usage.totalTokens);
    QCOMPARE(actual.rawResponses, expected.rawResponses);
    QCOMPARE(actual.batchSpans.size(), expected.batchSpans.size());
    for (qsizetype i = 0; i < actual.batchSpans.size(); ++i) {
        QCOMPARE(actual.batchSpans.at(i).firstImage, expected.batchSpans.at(i).firstImage);
        QCOMPARE(actual.batchSpans.at(i).lastImage, expected.batchSpans.at(i).lastImage);
    }
    QCOMPARE(actual.items.size(), expected.items.size());
    for (qsizetype i = 0; i < actual.items.size(); ++i) {
        const RecognizedItem &a = actual.items.at(i);
        const RecognizedItem &e = expected.items.at(i);
        QCOMPARE(a.rawName, e.rawName);
        QCOMPARE(a.canonicalName, e.canonicalName);
        QCOMPARE(a.quantityText, e.quantityText);
        QCOMPARE(a.count, e.count);
        QCOMPARE(a.category, e.category);
        QCOMPARE(a.selected, e.selected);
        QCOMPARE(a.isGift, e.isGift);
        QCOMPARE(a.storage, e.storage);
        QCOMPARE(a.productionDateText, e.productionDateText);
        QCOMPARE(a.expiryDateText, e.expiryDateText);
        QCOMPARE(a.purchaseDateText, e.purchaseDateText);
        QCOMPARE(a.imageIndex, e.imageIndex);
    }
}

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
        Fixture f({ answer(okBody({ itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋")) })) });

        const quint64 id = f.recognizer.recognize(imageRequest());
        QCOMPARE(f.succeeded.count(), 0);
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.succeeded.at(0).at(0).toULongLong(), id);

        QCOMPARE(f.sent(), 1);
        const ai::ChatRequestSummary summary = f.client.requests().first().summary;
        QCOMPARE(summary.imageCount, 3);
        QVERIFY(summary.jsonMode);
        const QJsonObject body = f.sentBody(0);
        QCOMPARE(body.value("model").toString(), QStringLiteral("deepseek-flash"));
        QCOMPARE(body.value("response_format").toObject().value("type").toString(), QStringLiteral("json_object"));
        const QJsonArray content = body.value("messages").toArray().at(0).toObject().value("content").toArray();
        QCOMPARE(content.size(), 1 + 3 * 2);
        QCOMPARE(imageBlocks(body), 3);

        const RecognitionResult result = f.result();
        QCOMPARE(result.items.size(), 1);
        QCOMPARE(result.items.first().canonicalName, QStringLiteral("鸡蛋"));
        QCOMPARE(result.usage.totalTokens, 1200);
        QCOMPARE(result.rawResponses.size(), 1);
        QVERIFY(result.elapsedMs >= 0);

        // Logs carry sizes and counts only.
        QVERIFY(!g_log.isEmpty());
        const QString log = g_log.join(QLatin1Char('\n'));
        QVERIFY(!log.contains(QStringLiteral("base64")));
        QVERIFY(!log.contains(QStringLiteral("data:image")));
        QVERIFY(!log.contains(QStringLiteral("标准食材词表")));
        QVERIFY(!log.contains(QStringLiteral("鸡蛋")));
    }

    void orderTextIsSentAsDelimitedText()
    {
        Fixture f({ answer(okBody({ itemJson(QStringLiteral("五花肉"), QStringLiteral("五花肉")) })) });
        f.recognizer.recognize(textRequest(QStringLiteral("猪五花肉 500g x1")));
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);

        const QJsonArray content = f.sentBody(0).value("messages").toArray().at(0).toObject().value("content").toArray();
        QCOMPARE(content.size(), 2);
        QVERIFY(content.at(1).toObject().value("text").toString().contains(QStringLiteral("猪五花肉 500g x1")));
        QCOMPARE(f.result().sourceType, SourceType::Cart); // as declared by the model
    }

    void transportErrorsKeepTheirKind_data()
    {
        QTest::addColumn<ErrorKind>("kind");
        QTest::addColumn<int>("status");

        QTest::newRow("auth") << ErrorKind::AuthFailed << 401;
        QTest::newRow("quota") << ErrorKind::QuotaOrRateLimit << 429;
        QTest::newRow("rejected") << ErrorKind::RequestRejected << 422;
        QTest::newRow("server") << ErrorKind::ServerError << 503;
        QTest::newRow("timeout") << ErrorKind::Timeout << 0;
        QTest::newRow("no network") << ErrorKind::NoNetwork << 0;
        QTest::newRow("tls") << ErrorKind::SecureConnectionFailed << 0;
    }

    void transportErrorsKeepTheirKind()
    {
        QFETCH(ErrorKind, kind);
        QFETCH(int, status);

        Fixture f({ failure(kind, status) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();

        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.error().kind, kind);
        QCOMPARE(f.error().httpStatus, status);
        QCOMPARE(f.error().userMessage, userMessageFor(kind));
        QCOMPARE(f.error().detail, QStringLiteral("scripted %1").arg(status));
        QCOMPARE(f.sent(), 1);
    }

    void userMessagesTellTlsTroubleFromNoNetwork()
    {
        QVERIFY(userMessageFor(ErrorKind::SecureConnectionFailed).contains(QStringLiteral("系统时间")));
        QVERIFY(userMessageFor(ErrorKind::SecureConnectionFailed) != userMessageFor(ErrorKind::NoNetwork));
    }

    void missingKeyIsReportedBeforePreparingAnything()
    {
        ai::DeepSeekChatClient client(std::make_shared<ai::StaticCredentialProvider>());
        DeepSeekRecognizer recognizer(&client);
        QSignalSpy failed(&recognizer, &RecognitionService::failed);
        RecognitionRequest unreadable = imageRequest();
        unreadable.images = { { QStringLiteral("/nonexistent/cart.png"), {} } };
        const quint64 id = recognizer.recognize(unreadable);
        QCOMPARE(failed.count(), 0); // asynchronous like every other outcome
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 10000);
        QCOMPARE(failed.at(0).at(0).toULongLong(), id);
        QCOMPARE(failed.at(0).at(1).value<RecognitionError>().kind, ErrorKind::NotConfigured);
    }

    void unparsableAnswersFail_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("html") << QByteArray("<html>gateway</html>");
        QTest::newRow("prose") << chatCompletion(QStringLiteral("抱歉，无法识别"));
    }

    void unparsableAnswersFail()
    {
        QFETCH(QByteArray, body);
        Fixture f({ answer(body), answer(okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) })) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::BadResponse);
        QCOMPARE(f.sent(), 1);
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
        QCOMPARE(f.sent(), 0);
    }

    void largeImportsAreSplitIntoBatchesAndMerged()
    {
        const QString egg = itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋"), QStringLiteral(R"("quantity_text":"10枚")"));
        const QString onion = itemJson(QStringLiteral("洋葱"), QStringLiteral("洋葱"));
        const QString milk = itemJson(QStringLiteral("牛奶"), QStringLiteral("牛奶"));
        Fixture f({ answer(okBody({ egg, onion }, 1000, 100)), answer(okBody({ egg, milk }, 800, 80)) });
        BatchLimits limits;
        limits.maxImages = 2;
        f.recognizer.setBatchLimits(limits);

        f.recognizer.recognize(imageRequest()); // 3 slices of one image -> 2 + 1
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);

        QCOMPARE(f.sent(), 2);
        QCOMPARE(imageBlocks(f.sentBody(0)), 2);
        QCOMPARE(imageBlocks(f.sentBody(1)), 1);

        const RecognitionResult result = f.result();
        QCOMPARE(result.items.size(), 3); // the repeated egg is dropped
        QCOMPARE(result.rawResponses.size(), 2);
        QCOMPARE(result.usage.promptTokens, 1800);
        QCOMPARE(result.usage.totalTokens, 1980);
    }

    void aCutOffAnswerYieldsAPartialFlaggedResult()
    {
        const QString cut = QStringLiteral("{\"source_type\":\"cart\",\"items\":[%1,{\"raw_name\":\"牛奶\",\"cano")
                                .arg(itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋")));
        Fixture f({ answer(chatCompletion(cut, 900, 131072, QStringLiteral("length"))) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QVERIFY(f.result().truncated);
        QCOMPARE(f.result().items.size(), 1);
        QCOMPARE(f.sent(), 1); // no automatic second attempt
    }

    void anEmptyAnswerThatSpentTheBudgetIsAnErrorNotZeroItems()
    {
        Fixture f({ answer(chatCompletion(QString(), 900, 131072, QStringLiteral("length"))) });
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.error().kind, ErrorKind::BadResponse);
        QCOMPARE(f.sent(), 1);
    }

    void requestBodyCarriesTheConfiguredOptions()
    {
        const QByteArray ok = okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) });
        Fixture f({ answer(ok), answer(ok) });

        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QJsonObject body = f.sentBody(0);
        QCOMPARE(body.value("max_tokens").toInt(), 131072);
        QCOMPARE(body.value("temperature").toInt(), 0);
        QVERIFY(!body.contains("thinking"));

        RequestOptions options;
        options.thinking = false;
        options.maxTokens = 9000;
        f.recognizer.setRequestOptions(options);
        f.recognizer.recognize(textRequest(QStringLiteral("y")));
        QTRY_COMPARE_WITH_TIMEOUT(f.sent(), 2, 10000);
        body = f.sentBody(1);
        QCOMPARE(body.value("max_tokens").toInt(), 9000);
        QCOMPARE(body.value("thinking").toObject().value("type").toString(), QStringLiteral("disabled"));
    }

    void sameItemInDifferentImagesIsNotMerged()
    {
        const QString egg = itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋"), QStringLiteral("\"quantity_text\":\"10枚\""));
        Fixture f({ answer(okBody({ egg })), answer(okBody({ egg })) });

        RecognitionRequest request = imageRequest();
        request.images = { request.images.first(), request.images.first() }; // two images, 3 slices each
        BatchLimits perImage;
        perImage.maxImages = 3; // one image per request
        f.recognizer.setBatchLimits(perImage);
        f.recognizer.recognize(request);
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.sent(), 2);
        QCOMPARE(f.result().items.size(), 2); // two real purchases of the same egg
        QCOMPARE(f.result().batchSpans.size(), 2);
        QCOMPARE(f.result().batchSpans.at(1).firstImage, 1);
    }

    void missingJpegPluginIsReportedAsAnInternalError()
    {
        Fixture f({ answer(okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) })) });
        f.recognizer.setJpegProbe([] { return false; });
        f.recognizer.recognize(imageRequest());
        f.waitForAnswer();
        QCOMPARE(f.error().kind, ErrorKind::Internal);
        QCOMPARE(f.error().userMessage, QStringLiteral("图片处理组件缺失，请更新应用。"));
        QVERIFY(f.error().detail.contains(QStringLiteral("qjpeg")));
        QCOMPARE(f.sent(), 0);

        // Text input needs no image plugin.
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        QTRY_COMPARE_WITH_TIMEOUT(f.succeeded.count(), 1, 10000);
    }

    void cancellingWhileImagesAreBeingPreparedSendsNothing()
    {
        Fixture f;
        RecognitionRequest request = imageRequest();
        request.images = QList<ImageInput>(12, request.images.first());
        const quint64 id = f.recognizer.recognize(request);
        f.recognizer.cancel(id);
        QTest::qWait(500);
        QCOMPARE(f.succeeded.count() + f.failed.count(), 0);
        QCOMPARE(f.sent(), 0);
    }

    void aFailingLaterBatchFailsTheWholeImport()
    {
        Fixture f({ answer(okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) })),
                    failure(ErrorKind::AuthFailed, 401) });
        BatchLimits limits;
        limits.maxImages = 2;
        f.recognizer.setBatchLimits(limits);
        f.recognizer.recognize(imageRequest());
        f.waitForAnswer();
        QCOMPARE(f.succeeded.count(), 0);
        QCOMPARE(f.error().kind, ErrorKind::AuthFailed);
    }

    void cancellingARequestInFlightCancelsItsChatRequest()
    {
        FakeHttpServer server;
        QVERIFY(server.listen());
        ai::DeepSeekChatClient client(std::make_shared<ai::StaticCredentialProvider>(QStringLiteral("test-key")));
        client.setEndpoint(server.url());
        DeepSeekRecognizer recognizer(&client);
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        QSignalSpy failed(&recognizer, &RecognitionService::failed);
        QSignalSpy chatFailed(&client, &ai::ChatClient::failed);

        server.enqueue({ 200, {}, true });
        const quint64 id = recognizer.recognize(textRequest(QStringLiteral("x")));
        QTRY_COMPARE_WITH_TIMEOUT(server.received().size(), 1, 10000);
        recognizer.cancel(id);
        QTest::qWait(300);
        QCOMPARE(succeeded.count() + failed.count(), 0);
        QCOMPARE(chatFailed.count(), 0); // cancelled in the client too, not left to time out

        // The recognizer stays usable.
        server.enqueue({ 200, okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) }) });
        recognizer.recognize(textRequest(QStringLiteral("y")));
        QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 10000);
    }

    void requestIdsAreUniqueAndAnswersMatchThem()
    {
        Fixture f({ answer(okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) })),
                    failure(ErrorKind::AuthFailed, 401) });
        const quint64 a = f.recognizer.recognize(textRequest(QStringLiteral("a")));
        f.waitForAnswer();
        const quint64 b = f.recognizer.recognize(textRequest(QStringLiteral("b")));
        QTRY_COMPARE_WITH_TIMEOUT(f.failed.count(), 1, 10000);
        QVERIFY(a != b);
        QCOMPARE(f.succeeded.at(0).at(0).toULongLong(), a);
        QCOMPARE(f.failed.at(0).at(0).toULongLong(), b);
    }

    void anotherServicesAnswersOnASharedClientAreIgnored()
    {
        const QByteArray ok = okBody({ itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋")) });
        Fixture f({ answer(chatCompletion(QStringLiteral("not for the recognizer"))), answer(ok) });
        ai::ChatRequest other;
        other.messages = { ai::ChatMessage::user(QStringLiteral("hi")) };
        f.client.send(other);
        f.recognizer.recognize(textRequest(QStringLiteral("x")));
        f.waitForAnswer();
        QCOMPARE(f.failed.count(), 0);
        QCOMPARE(f.succeeded.count(), 1);
    }

    // Format-1 recordings replayed at the transport level, through image preparation and request
    // building, give exactly what the recognition-level replay gives.
    void transportReplayOfFormat1RecordingsMatchesRecognitionReplay_data()
    {
        QTest::addColumn<QString>("sample");
        QTest::addColumn<QString>("input");
        QTest::addColumn<InputKind>("kind");
        QTest::newRow("long cart") << QStringLiteral("cart-fresh-long-01")
                                   << QStringLiteral("synthetic/screenshots/cart-fresh-long-01.png") << InputKind::Screenshot;
        QTest::newRow("receipt") << QStringLiteral("receipt-01-flat")
                                 << QStringLiteral("synthetic/receipts/receipt-01-flat.jpg") << InputKind::Receipt;
        QTest::newRow("text") << QStringLiteral("text-delivery-02")
                              << QStringLiteral("synthetic/texts/text-delivery-02.txt") << InputKind::OrderText;
    }

    void transportReplayOfFormat1RecordingsMatchesRecognitionReplay()
    {
        QFETCH(QString, sample);
        QFETCH(QString, input);
        QFETCH(InputKind, kind);

        QString error;
        const auto vocabulary = Vocabulary::fromFile(
            QStringLiteral(WHILEFRESH_DEMO_ASSETS_DIR "/../data/vocabulary.json"), &error);
        QVERIFY2(vocabulary, qPrintable(error));
        RecognitionRequest request;
        request.kind = kind;
        request.vocabulary = std::make_shared<const Vocabulary>(*vocabulary);
        if (kind == InputKind::OrderText) {
            QFile file(testDataPath(input));
            QVERIFY(file.open(QIODevice::ReadOnly));
            request.text = QString::fromUtf8(file.readAll());
        } else {
            request.images = { { testDataPath(input), {} } };
        }
        const QString path = testDataPath(QStringLiteral("recordings/synthetic/%1.json").arg(sample));

        ReplayRecognizer replay(path);
        QSignalSpy replayed(&replay, &RecognitionService::succeeded);
        replay.recognize(request);
        QTRY_COMPARE_WITH_TIMEOUT(replayed.count(), 1, 10000);

        const auto recording = ai::readChatRecording(path, &error);
        QVERIFY2(recording, qPrintable(error));
        ai::RecordedChatClient client(*recording);
        DeepSeekRecognizer recognizer(&client);
        QSignalSpy recognized(&recognizer, &RecognitionService::succeeded);
        QSignalSpy failed(&recognizer, &RecognitionService::failed);
        recognizer.recognize(request);
        QTRY_COMPARE_WITH_TIMEOUT(recognized.count() + failed.count(), 1, 20000);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(client.remaining(), 0);

        compareResults(recognized.at(0).at(1).value<RecognitionResult>(),
                       replayed.at(0).at(1).value<RecognitionResult>());
    }
};

QTEST_GUILESS_MAIN(DeepSeekRecognizerTest)

#include "deepseek-recognizer-test.moc"
