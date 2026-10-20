#include "recognition/response-parser.h"

#include "recognition-test-support.h"

#include <QTest>

using namespace whilefresh::recognition;
using namespace whilefresh::recognition::testing;

namespace {

ParseOutcome parseContent(const QString &content, InputKind kind = InputKind::Screenshot)
{
    return parseChatCompletion(chatCompletion(content), syntheticVocabulary().get(), kind);
}

}

class ResponseParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesAFullItem()
    {
        const QString item = QStringLiteral(
            R"({"raw_name":"【爆款】川香麻辣香肠 200g","canonical_name":"香肠","quantity_text":"200g","count":2,)"
            R"("category":"food","selected":true,"is_gift":false,"production_date_text":"今天生产",)"
            R"("expiry_date_text":"2026-11-03到期","purchase_date_text":"2026-10-18 19:42","storage":"freezer",)"
            R"("shelf_life_days":{"fridge":30,"freezer":180,"room":null},"image_index":2})");
        const ParseOutcome outcome = parseChatCompletion(
            chatCompletion(modelJson({ item }, QStringLiteral("\"ignored_summary\":\"换购推荐\"")), 1200, 340),
            syntheticVocabulary().get(), InputKind::Screenshot);

        QVERIFY(outcome.result);
        const RecognitionResult &result = *outcome.result;
        QCOMPARE(result.sourceType, SourceType::Cart);
        QCOMPARE(result.ignoredSummary, QStringLiteral("换购推荐"));
        QCOMPARE(result.usage.promptTokens, 1200);
        QCOMPARE(result.usage.completionTokens, 340);
        QCOMPARE(result.usage.totalTokens, 1540);
        QCOMPARE(result.rawResponses.size(), 1);

        QCOMPARE(result.items.size(), 1);
        const RecognizedItem &it = result.items.first();
        QCOMPARE(it.rawName, QStringLiteral("【爆款】川香麻辣香肠 200g"));
        QCOMPARE(it.canonicalName, QStringLiteral("香肠"));
        QCOMPARE(it.quantityText, QStringLiteral("200g"));
        QCOMPARE(it.count, 2);
        QCOMPARE(it.category, ItemCategory::Food);
        QVERIFY(it.selected);
        QVERIFY(!it.isGift);
        QCOMPARE(it.productionDateText, QStringLiteral("今天生产"));
        QCOMPARE(it.expiryDateText, QStringLiteral("2026-11-03到期"));
        QCOMPARE(it.purchaseDateText, QStringLiteral("2026-10-18 19:42"));
        QCOMPARE(it.storage, StorageKind::Freezer);
        QCOMPARE(it.shelfLifeDays.fridge, std::optional<int>(30));
        QCOMPARE(it.shelfLifeDays.freezer, std::optional<int>(180));
        QVERIFY(!it.shelfLifeDays.room);
        QCOMPARE(it.imageIndex, 1);
    }

    void toleratesCodeFence()
    {
        const QString json = modelJson({ itemJson(QStringLiteral("鸡蛋 10枚"), QStringLiteral("鸡蛋")) });
        for (const QString &wrapped : { QStringLiteral("```json\n%1\n```").arg(json),
                                        QStringLiteral("```\n%1\n```").arg(json),
                                        QStringLiteral("  ```JSON\n%1```  ").arg(json),
                                        QStringLiteral("好的，结果如下：\n%1\n希望有帮助").arg(json) }) {
            const ParseOutcome outcome = parseContent(wrapped);
            QVERIFY2(outcome.result, qPrintable(wrapped));
            QCOMPARE(outcome.result->items.size(), 1);
        }
    }

    void missingFieldsGetDefaults()
    {
        const ParseOutcome outcome = parseContent(modelJson({ QStringLiteral(R"({"raw_name":"鸡蛋"})") }));
        QVERIFY(outcome.result);
        const RecognizedItem &it = outcome.result->items.first();
        QCOMPARE(it.canonicalName, uncategorizedName());
        QCOMPARE(it.count, 1);
        QCOMPARE(it.category, ItemCategory::Food);
        QVERIFY(it.selected);
        QVERIFY(!it.isGift);
        QVERIFY(it.quantityText.isEmpty());
        QVERIFY(it.productionDateText.isEmpty());
        QVERIFY(it.expiryDateText.isEmpty());
        QVERIFY(it.purchaseDateText.isEmpty());
        QCOMPARE(it.storage, StorageKind::Unknown);
        QVERIFY(!it.shelfLifeDays.fridge && !it.shelfLifeDays.freezer && !it.shelfLifeDays.room);
        QCOMPARE(it.imageIndex, -1);
        QVERIFY(outcome.result->ignoredSummary.isEmpty());
    }

    void toleratesWrongTypes()
    {
        const QString item = QStringLiteral(
            R"({"raw_name":"西红柿","canonical_name":"西红柿","count":"3件","selected":"false","is_gift":"是",)"
            R"("quantity_text":500,"production_date_text":"null","expiry_date_text":"无","purchase_date_text":"",)"
            R"("category":"mystery","storage":"cellar","shelf_life_days":{"fridge":"7","freezer":-3,"room":0},)"
            R"("image_index":"第2张"})");
        const ParseOutcome outcome = parseContent(modelJson({ item, QStringLiteral("\"not an object\""), QStringLiteral("42") }));
        QVERIFY(outcome.result);
        QCOMPARE(outcome.result->items.size(), 1);
        const RecognizedItem &it = outcome.result->items.first();
        QCOMPARE(it.count, 3);
        QVERIFY(!it.selected);
        QVERIFY(it.isGift);
        QCOMPARE(it.quantityText, QStringLiteral("500"));
        QVERIFY(it.productionDateText.isEmpty());
        QVERIFY(it.expiryDateText.isEmpty());
        QCOMPARE(it.category, ItemCategory::Food);
        QCOMPARE(it.storage, StorageKind::Unknown);
        QCOMPARE(it.shelfLifeDays.fridge, std::optional<int>(7));
        QVERIFY(!it.shelfLifeDays.freezer);
        QVERIFY(!it.shelfLifeDays.room);
        QCOMPARE(it.imageIndex, 1);
    }

    void countIsClamped()
    {
        const ParseOutcome outcome = parseContent(modelJson({
            QStringLiteral(R"({"raw_name":"a","count":0})"),
            QStringLiteral(R"({"raw_name":"b","count":-4})"),
            QStringLiteral(R"({"raw_name":"c","count":100000})") }));
        QVERIFY(outcome.result);
        QCOMPARE(outcome.result->items.at(0).count, 1);
        QCOMPARE(outcome.result->items.at(1).count, 1);
        QCOMPARE(outcome.result->items.at(2).count, 999);
    }

    void canonicalNamesMustComeFromTheVocabulary()
    {
        const ParseOutcome outcome = parseContent(modelJson({
            itemJson(QStringLiteral("a"), QStringLiteral("五花肉")),
            itemJson(QStringLiteral("b"), QStringLiteral("川味香肠")), // alias -> standard name
            itemJson(QStringLiteral("c"), QStringLiteral("麻婆豆腐料理包")), // not in the vocabulary
            itemJson(QStringLiteral("d"), QStringLiteral("未归类")),
            itemJson(QStringLiteral("e"), QString()),
            QStringLiteral(R"({"raw_name":"f"})") }));
        QVERIFY(outcome.result);
        QStringList canonical;
        for (const RecognizedItem &item : outcome.result->items)
            canonical.append(item.canonicalName);
        QCOMPARE(canonical, (QStringList{ QStringLiteral("五花肉"), QStringLiteral("香肠"), uncategorizedName(),
                                          uncategorizedName(), uncategorizedName(), uncategorizedName() }));
        QCOMPARE(outcome.result->items.at(2).rawName, QStringLiteral("c"));
    }

    void nullVocabularyKeepsModelNames()
    {
        const ParseOutcome outcome = parseChatCompletion(
            chatCompletion(modelJson({ itemJson(QStringLiteral("a"), QStringLiteral("随便")) })), nullptr,
            InputKind::Screenshot);
        QVERIFY(outcome.result);
        QCOMPARE(outcome.result->items.first().canonicalName, QStringLiteral("随便"));
    }

    void itemsWithoutAnyNameAreDropped()
    {
        const ParseOutcome outcome = parseContent(modelJson({ QStringLiteral("{}"), QStringLiteral(R"({"count":2})") }));
        QVERIFY(outcome.result);
        QVERIFY(outcome.result->items.isEmpty());
    }

    void bareArrayIsAccepted()
    {
        const ParseOutcome outcome = parseContent(
            QStringLiteral("[%1]").arg(itemJson(QStringLiteral("鸡蛋"), QStringLiteral("鸡蛋"))));
        QVERIFY(outcome.result);
        QCOMPARE(outcome.result->items.size(), 1);
    }

    void sourceTypeFallsBackToInputKind()
    {
        const QString noType = QStringLiteral(R"({"items":[]})");
        QCOMPARE(parseContent(noType, InputKind::Receipt).result->sourceType, SourceType::Receipt);
        QCOMPARE(parseContent(noType, InputKind::OrderText).result->sourceType, SourceType::Text);
        QCOMPARE(parseContent(noType, InputKind::Screenshot).result->sourceType, SourceType::Other);
        QCOMPARE(parseContent(QStringLiteral(R"({"source_type":"order","items":[]})")).result->sourceType,
                 SourceType::Order);
        QCOMPARE(parseContent(QStringLiteral(R"({"source_type":"bogus","items":[]})"), InputKind::Receipt)
                     .result->sourceType,
                 SourceType::Receipt);
    }

    void unusableResponsesAreBadResponse_data()
    {
        QTest::addColumn<QByteArray>("body");

        QTest::newRow("not JSON at all") << QByteArray("<html>502</html>");
        QTest::newRow("no choices") << QByteArray(R"({"usage":{}})");
        QTest::newRow("model text is prose") << chatCompletion(QStringLiteral("抱歉，我无法识别这张图片。"));
        QTest::newRow("truncated JSON") << chatCompletion(QStringLiteral("{\"items\":[{\"raw_name\":\"鸡"));
        QTest::newRow("empty content") << chatCompletion(QString());
        QTest::newRow("object without items") << chatCompletion(QStringLiteral(R"({"error":"x"})"));
        QTest::newRow("items not an array") << chatCompletion(QStringLiteral(R"({"items":"none"})"));
        QTest::newRow("no choices at all") << QByteArray(R"({"choices":[]})");
        QTest::newRow("empty and cut off") << chatCompletion(QString(), 10, 8000, QStringLiteral("length"));
        QTest::newRow("cut off before the first item completes")
            << chatCompletion(QStringLiteral("{\"source_type\":\"cart\",\"items\":[{\"raw_name\":\"鸡蛋\",\"count"),
                              10, 8000, QStringLiteral("length"));
        QTest::newRow("cut off without items key") << chatCompletion(QStringLiteral("{\"source_ty"), 10, 8000, QStringLiteral("length"));
    }

    void unusableResponsesAreBadResponse()
    {
        QFETCH(QByteArray, body);
        const ParseOutcome outcome = parseChatCompletion(body, syntheticVocabulary().get(), InputKind::Screenshot);
        QVERIFY(!outcome.result);
        QCOMPARE(outcome.error.kind, ErrorKind::BadResponse);
        QVERIFY(!outcome.error.userMessage.isEmpty());
        QVERIFY(!outcome.error.detail.isEmpty());
    }

    void cutOffRepliesKeepTheItemsThatArrivedWhole()
    {
        const QString whole1 = itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋"), QStringLiteral("\"count\":2"));
        const QString whole2 = itemJson(QStringLiteral("a}{b \\\"x\\\""), QStringLiteral("洋葱")); // braces and quotes inside strings
        const QString cut = QStringLiteral("{\"source_type\":\"order\",\"items\":[%1,\n %2,\n {\"raw_name\":\"牛奶\",\"canonical_na").arg(whole1, whole2);
        for (const QString &content : { cut, QStringLiteral("```json\n") + cut }) {
            const ParseOutcome outcome = parseChatCompletion(
                chatCompletion(content, 700, 8000, QStringLiteral("length")), syntheticVocabulary().get(),
                InputKind::Screenshot);
            QVERIFY(outcome.result);
            QVERIFY(outcome.result->truncated);
            QCOMPARE(outcome.result->items.size(), 2);
            QCOMPARE(outcome.result->items.at(0).count, 2);
            QCOMPARE(outcome.result->items.at(1).canonicalName, QStringLiteral("洋葱"));
            QCOMPARE(outcome.result->sourceType, SourceType::Order);
            QCOMPARE(outcome.result->usage.totalTokens, 8700);
            QCOMPARE(outcome.result->rawResponses.size(), 1);
        }
    }

    void aCompleteReplyThatEndedOnTheLimitIsStillFlagged()
    {
        const ParseOutcome outcome = parseChatCompletion(
            chatCompletion(modelJson({ itemJson(QStringLiteral("a"), QStringLiteral("鸡蛋")) }), 10, 4096, QStringLiteral("length")),
            syntheticVocabulary().get(), InputKind::Screenshot);
        QVERIFY(outcome.result);
        QVERIFY(outcome.result->truncated);
        QCOMPARE(outcome.result->items.size(), 1);
    }

    void normalRepliesAreNotFlaggedAndEmptyOnesAreNotZeroItems()
    {
        QVERIFY(!parseContent(modelJson({ itemJson(QStringLiteral("a"), QStringLiteral("鸡蛋")) })).result->truncated);
        QVERIFY(!parseChatCompletion(chatCompletion(QString()), nullptr, InputKind::Screenshot).result);
    }

    void hugeNumbersSaturateInsteadOfBecomingZero()
    {
        const ParseOutcome outcome = parseContent(modelJson({
            QStringLiteral("{\"raw_name\":\"a\",\"count\":\"99999999999999999999\"}"),
            QStringLiteral("{\"raw_name\":\"b\",\"count\":1e300}"),
            QStringLiteral("{\"raw_name\":\"c\",\"count\":3.6,\"shelf_life_days\":{\"fridge\":\"99999999999\"}}") }));
        QVERIFY(outcome.result);
        QCOMPARE(outcome.result->items.at(0).count, 999);
        QCOMPARE(outcome.result->items.at(1).count, 999);
        QCOMPARE(outcome.result->items.at(2).count, 4);
        QVERIFY(!outcome.result->items.at(2).shelfLifeDays.fridge); // beyond any plausible shelf life
    }

    void userMessagesAreChineseAndFreeOfDetail()
    {
        for (ErrorKind kind : { ErrorKind::NotConfigured, ErrorKind::NoNetwork, ErrorKind::Timeout,
                                ErrorKind::AuthFailed, ErrorKind::QuotaOrRateLimit, ErrorKind::ServerError,
                                ErrorKind::RequestRejected, ErrorKind::BadResponse, ErrorKind::InvalidInput,
                                ErrorKind::Internal }) {
            const QString message = userMessageFor(kind);
            QVERIFY(!message.isEmpty());
            QVERIFY(!message.contains(QStringLiteral("HTTP")));
            QVERIFY(!message.contains(QStringLiteral("JSON")));
            QVERIFY(!message.contains(QStringLiteral("401")));
        }
    }

    void mergeOnlyDropsOverlapDuplicatesOfAnImageSplitAcrossRequests()
    {
        const auto part = [](const QStringList &items, int firstImage, int lastImage) {
            RecognitionResult result = *parseChatCompletion(chatCompletion(modelJson(items), 100, 10),
                                                            syntheticVocabulary().get(), InputKind::Screenshot).result;
            result.batchSpans = { { firstImage, lastImage } };
            return result;
        };
        const auto egg = [](const QString &extra = {}) {
            return itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋"),
                            QStringLiteral("\"quantity_text\":\"10枚\"") + extra);
        };
        const QString onion = itemJson(QStringLiteral("洋葱"), QStringLiteral("洋葱"));
        const QString onImage3 = QStringLiteral(",\"image_index\":4"); // 1-based in the reply -> image 3
        const QString onImage5 = QStringLiteral(",\"image_index\":6");

        // Image 3 straddles the two requests: its overlap duplicate goes, a real second egg from
        // image 5 stays, and so does the gift egg (a different item).
        RecognitionResult first = part({ egg(onImage3), onion }, 0, 3);
        first.ignoredSummary = QStringLiteral("热卖推荐");
        RecognitionResult second = part({ egg(onImage3), egg(onImage5), egg(onImage5 + QStringLiteral(",\"is_gift\":true")), onion }, 3, 6);
        second.ignoredSummary = QStringLiteral("热卖推荐");
        RecognitionResult third = part({ egg(onImage5) }, 7, 8); // no shared image with the previous request
        third.ignoredSummary = QStringLiteral("优惠横幅");

        const RecognitionResult merged = mergeResults({ first, second, third });
        // egg(3), onion | egg(5), gift egg(5) | egg(5) again
        QCOMPARE(merged.items.size(), 5);
        QCOMPARE(merged.items.at(2).imageIndex, 5);
        QCOMPARE(merged.items.at(3).isGift, true);
        QCOMPARE(merged.rawResponses.size(), 3);
        QCOMPARE(merged.batchSpans.size(), 3);
        QCOMPARE(merged.usage.totalTokens, 330);
        QCOMPARE(merged.ignoredSummary, QStringLiteral("热卖推荐；优惠横幅"));
        QCOMPARE(merged.sourceType, SourceType::Cart);
    }

    void mergeWithoutSpansDropsNothing()
    {
        const QString egg = itemJson(QStringLiteral("鲜鸡蛋"), QStringLiteral("鸡蛋"));
        const RecognitionResult one = *parseChatCompletion(chatCompletion(modelJson({ egg })), nullptr, InputKind::Screenshot).result;
        QCOMPARE(mergeResults({ one, one }).items.size(), 2);
    }

    void mergeKeepsTruncatedFlag()
    {
        RecognitionResult a;
        RecognitionResult b;
        b.truncated = true;
        QVERIFY(mergeResults({ a, b }).truncated);
        QVERIFY(!mergeResults({ a, a }).truncated);
    }

    void mergeOfNothingIsEmpty()
    {
        const RecognitionResult merged = mergeResults({});
        QVERIFY(merged.items.isEmpty());
        QCOMPARE(merged.sourceType, SourceType::Other);
    }
};

QTEST_GUILESS_MAIN(ResponseParserTest)

#include "response-parser-test.moc"
