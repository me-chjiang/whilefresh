#include "recognition/request-builder.h"

#include "recognition-test-support.h"

#include <QTest>

using namespace whilefresh::recognition;
using namespace whilefresh::recognition::testing;

namespace {

PreparedImage slice(int imageIndex, int sliceIndex, int sliceCount, QDate date = {})
{
    PreparedImage image;
    image.imageIndex = imageIndex;
    image.sliceIndex = sliceIndex;
    image.sliceCount = sliceCount;
    image.captureDate = date;
    image.jpeg = QByteArray("\xFF\xD8jpeg-bytes\xFF\xD9");
    return image;
}

}

class RequestBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void promptListsVocabularyByCategory()
    {
        const auto vocabulary = syntheticVocabulary();
        const QString prompt = buildPrompt(InputKind::Screenshot, *vocabulary);

        QVERIFY(prompt.contains(QStringLiteral("【蔬菜】")));
        QVERIFY(prompt.contains(QStringLiteral("【肉类】")));
        QVERIFY(prompt.contains(QStringLiteral("五花肉、")));
        QVERIFY(prompt.contains(QStringLiteral("未归类")));
        // Aliases are resolved on our side and kept out of the prompt to save tokens.
        QVERIFY(!prompt.contains(QStringLiteral("川味香肠")));
        QVERIFY(!prompt.contains(QStringLiteral("{{")));
    }

    void promptStatesTheKeyRules()
    {
        const QString prompt = buildPrompt(InputKind::Screenshot, *syntheticVocabulary());
        for (const char *phrase : { "只输出一个 JSON", "勾选", "赠品", "换购", "凑单", "ignored_summary",
                                    "不要推测", "逐字取自下方词表" })
            QVERIFY2(prompt.contains(QString::fromUtf8(phrase)), phrase);
    }

    void promptDiffersPerInputKind()
    {
        const auto vocabulary = syntheticVocabulary();
        QVERIFY(buildPrompt(InputKind::Screenshot, *vocabulary).contains(QStringLiteral("切片")));
        QVERIFY(buildPrompt(InputKind::Receipt, *vocabulary).contains(QStringLiteral("小票")));
        QVERIFY(buildPrompt(InputKind::OrderText, *vocabulary).contains(QStringLiteral("订单文字")));
        QVERIFY(!buildPrompt(InputKind::OrderText, *vocabulary).contains(QStringLiteral("切片")));
    }

    void imageRequestHasExpectedStructure()
    {
        const auto vocabulary = syntheticVocabulary();
        const QJsonObject body = buildChatRequest(
            InputKind::Screenshot, *vocabulary,
            { slice(0, 0, 2, QDate(2026, 10, 18)), slice(0, 1, 2, QDate(2026, 10, 18)), slice(1, 0, 1) }, {});

        QCOMPARE(body.value("model").toString(), QStringLiteral("deepseek-flash"));
        QCOMPARE(body.value("response_format").toObject().value("type").toString(), QStringLiteral("json_object"));

        const QJsonArray messages = body.value("messages").toArray();
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.at(0).toObject().value("role").toString(), QStringLiteral("user"));

        const QJsonArray content = messages.at(0).toObject().value("content").toArray();
        QCOMPARE(content.size(), 1 + 3 * 2);
        QCOMPARE(content.at(0).toObject().value("type").toString(), QStringLiteral("text"));
        QCOMPARE(content.at(0).toObject().value("text").toString(), buildPrompt(InputKind::Screenshot, *vocabulary));

        const QStringList labels = { QStringLiteral("[第 1 张图，切片 1/2，拍摄日期 2026-10-18]"),
                                     QStringLiteral("[第 1 张图，切片 2/2，拍摄日期 2026-10-18]"),
                                     QStringLiteral("[第 2 张图]") };
        for (int i = 0; i < 3; ++i) {
            const QJsonObject label = content.at(1 + 2 * i).toObject();
            QCOMPARE(label.value("type").toString(), QStringLiteral("text"));
            QCOMPARE(label.value("text").toString(), labels.at(i));

            const QJsonObject image = content.at(2 + 2 * i).toObject();
            QCOMPARE(image.value("type").toString(), QStringLiteral("image_url"));
            const QString url = image.value("image_url").toObject().value("url").toString();
            QVERIFY(url.startsWith(QStringLiteral("data:image/jpeg;base64,")));
            QCOMPARE(QByteArray::fromBase64(url.mid(23).toLatin1()), slice(0, 0, 1).jpeg);
        }
    }

    void requestCarriesTokenBudgetAndTemperature()
    {
        const QJsonObject body = buildChatRequest(InputKind::OrderText, *syntheticVocabulary(), {}, QStringLiteral("x"));
        QCOMPARE(body.value("max_tokens").toInt(), 131072);
        QVERIFY(body.value("max_tokens").toInt() <= 393216); // documented maximum
        QCOMPARE(body.value("temperature").toInt(), 0);
        QVERIFY(!body.contains("thinking")); // thinking stays at the API default (on)
    }

    void thinkingCanBeSwitchedOffAndBudgetChanged()
    {
        RequestOptions options;
        options.thinking = false;
        options.maxTokens = 4000;
        const QJsonObject body = buildChatRequest(InputKind::OrderText, *syntheticVocabulary(), {}, QStringLiteral("x"), options);
        QCOMPARE(body.value("thinking").toObject().value("type").toString(), QStringLiteral("disabled"));
        QCOMPARE(body.value("max_tokens").toInt(), 4000);
    }

    void orderTextRequestDelimitsTheText()
    {
        const QJsonObject body = buildChatRequest(InputKind::OrderText, *syntheticVocabulary(), {},
                                                  QStringLiteral("猪五花肉 500g x1"));
        const QJsonArray content = body.value("messages").toArray().at(0).toObject().value("content").toArray();
        QCOMPARE(content.size(), 2);
        const QString text = content.at(1).toObject().value("text").toString();
        QVERIFY(text.startsWith(QStringLiteral("<订单文字>\n")));
        QVERIFY(text.contains(QStringLiteral("猪五花肉 500g x1")));
        QVERIFY(text.endsWith(QStringLiteral("\n</订单文字>")));
    }

    void bodyContainsNoSecrets()
    {
        // The key is added as an HTTP header by the recognizer, never to the body.
        const QByteArray json = QJsonDocument(buildChatRequest(InputKind::Receipt, *syntheticVocabulary(),
                                                               { slice(0, 0, 1) }, {}))
                                    .toJson(QJsonDocument::Compact);
        QVERIFY(!json.contains("Bearer"));
        QVERIFY(!json.contains("api_key"));
    }

    void promptVersionIsExposed()
    {
        QVERIFY(!promptVersion().isEmpty());
    }
};

QTEST_GUILESS_MAIN(RequestBuilderTest)

#include "request-builder-test.moc"
