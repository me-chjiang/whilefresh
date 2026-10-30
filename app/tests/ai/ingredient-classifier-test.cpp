#include "ai/ingredient-classifier.h"
#include "ai/recorded-chat-client.h"

#include "ai-test-support.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace whilefresh::ai;
using namespace whilefresh::ai::testing;
using whilefresh::core::IngredientCatalog;

namespace {

std::shared_ptr<const IngredientCatalog> smallCatalog()
{
    auto catalog = IngredientCatalog::fromJson(R"({
      "categories": [{"code":"meat","name":"肉类"}, {"code":"seafood","name":"水产海鲜"}],
      "items": [{"name":"鸡肉","category":"meat"}, {"name":"鸡腿","category":"meat","parent":"鸡肉"},
                {"name":"牛肉","category":"meat","aliases":["黄牛肉"]}, {"name":"鱼","category":"seafood"}]})");
    if (!catalog)
        qFatal("small catalog");
    return std::make_shared<const IngredientCatalog>(*catalog);
}

std::shared_ptr<const IngredientCatalog> realCatalog()
{
    QString error;
    auto catalog = IngredientCatalog::fromFile(QStringLiteral(WHILEFRESH_DEMO_ASSETS_DIR "/../data/vocabulary.json"), &error);
    if (!catalog)
        qFatal("real vocabulary: %s", qUtf8Printable(error));
    return std::make_shared<const IngredientCatalog>(*catalog);
}

ChatRecording answering(const QString &content)
{
    ChatExchange exchange;
    exchange.response = chatCompletion(content);
    return { { exchange } };
}

// Names of the live recording test-data/recordings/classifier/synthetic-names.json.
const QStringList kRecordedNames{ "冰鲜鸡全腿", "牛霖 500g", "鲜毛肚 200g", "花胶 100g", "罗汉果 1个",
                                  "黑松露酱 80g", "藕粉 500g", "洗衣液 2kg", "电热水壶 1.5L" };

}

class IngredientClassifierTest : public QObject
{
    Q_OBJECT

private slots:
    void requestIsAShortTextOnlyJsonCall()
    {
        const auto catalog = smallCatalog();
        const ChatRequest request = IngredientClassifier::buildRequest(
            { QStringLiteral(" 冰鲜  鸡全腿 "), QStringLiteral("花胶"), QStringLiteral("冰鲜 鸡全腿") }, *catalog);
        QVERIFY(request.jsonMode);
        QVERIFY(!request.thinking);
        QCOMPARE(request.idleTimeoutMs, std::optional<int>(kClassifierIdleTimeoutMs));
        QCOMPARE(request.deadlineMs, std::optional<int>(kClassifierDeadlineMs));
        QCOMPARE(request.messages.size(), 2);
        const QString system = request.messages.at(0).parts.at(0).value;
        QVERIFY(system.contains(QStringLiteral("鸡肉 > 鸡腿")));
        QVERIFY(system.contains(QStringLiteral("none")));
        QVERIFY(!system.contains(QStringLiteral("黄牛肉"))); // aliases stay on our side
        const QString user = request.messages.at(1).parts.at(0).value;
        QCOMPARE(user, QStringLiteral("<商品名>\n1. 冰鲜 鸡全腿\n2. 花胶\n</商品名>"));
        const EncodedChatRequest encoded = encodeChatRequest(request);
        QCOMPARE(encoded.summary.imageCount, 0);
    }

    void tooManyNamesAreCapped()
    {
        QStringList names;
        for (int i = 0; i < kClassifierMaxNames + 5; ++i)
            names.append(QStringLiteral("商品%1").arg(i));
        QCOMPARE(IngredientClassifier::namesToSend(names).size(), kClassifierMaxNames);
    }

    void answersAreCheckedAgainstTheCatalog()
    {
        const auto catalog = smallCatalog();
        const QStringList sent{ QStringLiteral("冰鲜鸡全腿"), QStringLiteral("牛霖"), QStringLiteral("花胶"),
                                QStringLiteral("洗衣液"), QStringLiteral("罗汉果") };
        const QJsonObject answer = QJsonDocument::fromJson(QString::fromUtf8(R"({"items":[
            {"index":1,"raw_name":"冰鲜鸡全腿","canonical_name":"鸡腿"},
            {"index":2,"raw_name":"牛霖","canonical_name":"黄牛肉"},
            {"index":3,"raw_name":"花胶","canonical_name":"鱼胶"},
            {"index":4,"raw_name":"洗衣液","canonical_name":"none"},
            {"index":4,"raw_name":"罗汉果","canonical_name":"鱼"},
            {"index":9,"canonical_name":"鸡肉"},
            {"index":1,"raw_name":"冰鲜鸡全腿","canonical_name":"鸡肉"}
        ]})").toUtf8()).object();
        const Classification result = IngredientClassifier::parseAnswer(answer, sent, *catalog);
        QCOMPARE(result.canonical.value(QStringLiteral("冰鲜鸡全腿")), QStringLiteral("鸡腿")); // first answer kept
        QCOMPARE(result.canonical.value(QStringLiteral("牛霖")), QStringLiteral("牛肉")); // alias normalised
        QVERIFY(!result.canonical.contains(QStringLiteral("花胶"))); // invented name dropped
        QVERIFY(!result.canonical.contains(QStringLiteral("洗衣液")));
        // A raw_name that disagrees with the index wins when it names a sent line.
        QCOMPARE(result.canonical.value(QStringLiteral("罗汉果")), QStringLiteral("鱼"));
        QCOMPARE(result.rejected, 1);
        QCOMPARE(result.canonical.size(), 3);
    }

    void classifyGoesThroughTheChatClient()
    {
        RecordedChatClient client(answering(QStringLiteral(
            R"({"items":[{"index":1,"raw_name":"冰鲜鸡全腿","canonical_name":"鸡腿"}]})")));
        IngredientClassifier classifier(&client, smallCatalog());
        QSignalSpy classified(&classifier, &IngredientClassifier::classified);
        QSignalSpy failed(&classifier, &IngredientClassifier::failed);
        const quint64 id = classifier.classify({ QStringLiteral("冰鲜鸡全腿") });
        QTRY_COMPARE(classified.size(), 1);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(classified.at(0).at(0).toULongLong(), id);
        const auto result = classified.at(0).at(1).value<Classification>();
        QCOMPARE(result.canonical.value(QStringLiteral("冰鲜鸡全腿")), QStringLiteral("鸡腿"));
        QCOMPARE(result.usage.totalTokens, 150);
        QCOMPARE(client.requests().size(), 1);
        QCOMPARE(client.requests().first().deadlineMs, std::optional<int>(kClassifierDeadlineMs));
    }

    void anUnreadableAnswerFails()
    {
        RecordedChatClient client(answering(QStringLiteral("不是 JSON")));
        IngredientClassifier classifier(&client, smallCatalog());
        QSignalSpy failed(&classifier, &IngredientClassifier::failed);
        classifier.classify({ QStringLiteral("花胶") });
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.at(0).at(1).value<ChatError>().kind, ErrorKind::BadResponse);
    }

    void cancelledRequestsStaySilent()
    {
        RecordedChatClient client(answering(QStringLiteral(R"({"items":[]})")));
        IngredientClassifier classifier(&client, smallCatalog());
        QSignalSpy classified(&classifier, &IngredientClassifier::classified);
        classifier.cancel(classifier.classify({ QStringLiteral("花胶") }));
        QTest::qWait(50);
        QCOMPARE(classified.size(), 0);
        QCOMPARE(client.remaining(), 1);
    }

    // A real DeepSeek answer for synthetic names, recorded with recognize-cli --classify. The
    // recording holds the request digest, so a change to the prompt or the vocabulary shows up
    // here and needs a new recording (docs/development.md).
    void liveRecordingReplays()
    {
        const QString path = QStringLiteral(WHILEFRESH_TEST_DATA_DIR "/recordings/classifier/synthetic-names.json");
        if (!QFile::exists(path))
            QSKIP("live classifier recording not made yet (T14: the keychain key was rejected with 401)");
        QString error;
        auto recording = readChatRecording(path, &error);
        QVERIFY2(recording, qPrintable(error));
        QCOMPARE(recording->exchanges.size(), 1);
        QVERIFY(!recording->exchanges.first().requestDigest.isEmpty());
        RecordedChatClient client(*recording);
        const auto catalog = realCatalog();
        IngredientClassifier classifier(&client, catalog);
        QSignalSpy classified(&classifier, &IngredientClassifier::classified);
        QSignalSpy failed(&classifier, &IngredientClassifier::failed);
        classifier.classify(kRecordedNames);
        QTRY_COMPARE(classified.size() + failed.size(), 1);
        QCOMPARE(failed.size(), 0);
        const auto result = classified.at(0).at(1).value<Classification>();
        for (auto it = result.canonical.cbegin(); it != result.canonical.cend(); ++it) {
            QVERIFY(kRecordedNames.contains(it.key()));
            QVERIFY(catalog->contains(it.value()));
        }
        QCOMPARE(result.canonical.value(QStringLiteral("冰鲜鸡全腿")), QStringLiteral("鸡腿"));
        QVERIFY(!result.canonical.contains(QStringLiteral("洗衣液 2kg")));
        QVERIFY(!result.canonical.contains(QStringLiteral("电热水壶 1.5L")));
    }
};

QTEST_MAIN(IngredientClassifierTest)
#include "ingredient-classifier-test.moc"
