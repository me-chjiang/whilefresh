#include "ai/preference-summarizer.h"
#include "ai/recorded-chat-client.h"

#include "ai-test-support.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace whilefresh::ai;
using namespace whilefresh::ai::testing;

namespace {

ChatRecording answering(const QString &content)
{
    ChatExchange exchange;
    exchange.response = chatCompletion(content);
    return { { exchange } };
}

// Preferences of the live recording test-data/recordings/preferences/synthetic-preferences.json.
const QStringList kRecordedPreferences{ "不吃香菜", "少油", "周末才做复杂菜", "不吃香菜" };

}

class PreferenceSummarizerTest : public QObject
{
    Q_OBJECT

private slots:
    void requestIsAShortTextOnlyJsonCall()
    {
        const ChatRequest request = PreferenceSummarizer::buildRequest(
            { QStringLiteral(" 不吃  香菜 "), QStringLiteral(""), QStringLiteral("少油") });
        QVERIFY(request.jsonMode);
        QVERIFY(!request.thinking);
        QCOMPARE(request.idleTimeoutMs, std::optional<int>(kSummarizerIdleTimeoutMs));
        QCOMPARE(request.deadlineMs, std::optional<int>(kSummarizerDeadlineMs));
        QCOMPARE(request.messages.size(), 2);
        QCOMPARE(request.messages.at(0).role, ChatRole::System);
        QVERIFY(request.messages.at(0).parts.at(0).value.contains(QStringLiteral("summary")));
        QCOMPARE(request.messages.at(1).parts.at(0).value,
                 QStringLiteral("<偏好记录>\n1. 不吃 香菜\n2. 少油\n</偏好记录>"));
        QCOMPARE(encodeChatRequest(request).summary.imageCount, 0);
    }

    void inputIsCapped()
    {
        QStringList many;
        for (int i = 0; i < 80; ++i)
            many.append(QStringLiteral("偏好%1").arg(i));
        many.append(QString(300, QLatin1Char('a')));
        const QString user = PreferenceSummarizer::buildRequest(many).messages.at(1).parts.at(0).value;
        QVERIFY(user.contains(QStringLiteral("50. 偏好49")));
        QVERIFY(!user.contains(QStringLiteral("51.")));
        const QString longOne = PreferenceSummarizer::buildRequest({ QString(300, QLatin1Char('a')) })
                                    .messages.at(1).parts.at(0).value;
        QVERIFY(longOne.contains(QString(100, QLatin1Char('a'))));
        QVERIFY(!longOne.contains(QString(101, QLatin1Char('a'))));
    }

    void answerIsOneLineAndTruncated()
    {
        QCOMPARE(PreferenceSummarizer::parseAnswer({ { "summary", " 不吃香菜；\n少油。 " } }),
                 QStringLiteral("不吃香菜； 少油。"));
        QCOMPARE(PreferenceSummarizer::parseAnswer({ { "summary", QString(500, QChar(0x6CB9)) } }).size(),
                 kSummaryMaxChars);
        QVERIFY(PreferenceSummarizer::parseAnswer({ { "summary", 3 } }).isEmpty());
        QVERIFY(PreferenceSummarizer::parseAnswer({ { "other", "x" } }).isEmpty());
        QVERIFY(PreferenceSummarizer::parseAnswer({ { "summary", "  " } }).isEmpty());
    }

    void summarizeGoesThroughTheChatClient()
    {
        RecordedChatClient client(answering(QStringLiteral(R"({"summary":"不吃香菜；少油。"})")));
        PreferenceSummarizer summarizer(&client);
        QSignalSpy done(&summarizer, &PreferenceSummarizer::summarized);
        QSignalSpy failed(&summarizer, &PreferenceSummarizer::failed);
        const quint64 id = summarizer.summarize({ QStringLiteral("不吃香菜"), QStringLiteral("少油") });
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(done.at(0).at(0).toULongLong(), id);
        const auto summary = done.at(0).at(1).value<PreferenceSummary>();
        QCOMPARE(summary.text, QStringLiteral("不吃香菜；少油。"));
        QCOMPARE(summary.usage.totalTokens, 150);
        QCOMPARE(client.requests().size(), 1);
        QCOMPARE(client.requests().first().deadlineMs, std::optional<int>(kSummarizerDeadlineMs));
    }

    void unusableAnswersFail()
    {
        for (const QString &content : { QStringLiteral("不是 JSON"), QStringLiteral(R"({"summary":""})"),
                                        QStringLiteral(R"({"text":"x"})") }) {
            RecordedChatClient client(answering(content));
            PreferenceSummarizer summarizer(&client);
            QSignalSpy failed(&summarizer, &PreferenceSummarizer::failed);
            summarizer.summarize({ QStringLiteral("少油") });
            QTRY_COMPARE(failed.size(), 1);
            QCOMPARE(failed.at(0).at(1).value<ChatError>().kind, ErrorKind::BadResponse);
        }
    }

    void cancelledRequestsStaySilent()
    {
        RecordedChatClient client(answering(QStringLiteral(R"({"summary":"x"})")));
        PreferenceSummarizer summarizer(&client);
        QSignalSpy done(&summarizer, &PreferenceSummarizer::summarized);
        summarizer.cancel(summarizer.summarize({ QStringLiteral("少油") }));
        QTest::qWait(50);
        QCOMPARE(done.size(), 0);
    }

    // A real DeepSeek answer for synthetic preferences, recorded with recognize-cli --summarize.
    // The recording holds the request digest, so a change to the prompt shows up here and needs a
    // new recording (docs/done/development.md).
    void liveRecordingReplays()
    {
        const QString path = QStringLiteral(WHILEFRESH_TEST_DATA_DIR "/recordings/preferences/synthetic-preferences.json");
        if (!QFile::exists(path))
            QSKIP("live preference recording not made yet");
        QString error;
        auto recording = readChatRecording(path, &error);
        QVERIFY2(recording, qPrintable(error));
        QCOMPARE(recording->exchanges.size(), 1);
        QVERIFY(!recording->exchanges.first().requestDigest.isEmpty());
        RecordedChatClient client(*recording);
        PreferenceSummarizer summarizer(&client);
        QSignalSpy done(&summarizer, &PreferenceSummarizer::summarized);
        QSignalSpy failed(&summarizer, &PreferenceSummarizer::failed);
        summarizer.summarize(kRecordedPreferences);
        QTRY_COMPARE(done.size() + failed.size(), 1);
        QCOMPARE(failed.size(), 0);
        const auto summary = done.at(0).at(1).value<PreferenceSummary>();
        QVERIFY(!summary.text.isEmpty());
        QVERIFY(summary.text.size() <= kSummaryMaxChars);
        QVERIFY(!summary.text.contains(QLatin1Char('\n')));
        QVERIFY(summary.text.contains(QStringLiteral("香菜")));
        QVERIFY(summary.text.contains(QStringLiteral("油")));
    }
};

QTEST_MAIN(PreferenceSummarizerTest)

#include "preference-summarizer-test.moc"
