#include "recognition/demo-recognizer.h"
#include "recognition/recording.h"
#include "recognition/replay-recognizer.h"
#include "recognition/vocabulary.h"

#include "recognition-test-support.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::recognition;
using namespace whilefresh::recognition::testing;

namespace {

RecognitionRequest requestFor(InputKind kind)
{
    RecognitionRequest request;
    request.kind = kind;
    request.vocabulary = syntheticVocabulary();
    return request;
}

}

class RecordedRecognizerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<RecognitionResult>();
        qRegisterMetaType<RecognitionError>();
    }

    void vocabularyLoadsFromJson()
    {
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("vocabulary: ignored 2 entries"));
        const auto vocabulary = Vocabulary::fromJson(
            R"({"items":[{"name":"鸡蛋","category":"蛋奶","aliases":["土鸡蛋"],"staple":false},
                         {"name":"鸡蛋","category":"x"},
                         {"name":"","category":"x"},
                         {"name":"牛奶","category":"蛋奶"}]})", &error);
        QVERIFY2(vocabulary, qPrintable(error));
        QCOMPARE(vocabulary->entries().size(), 2);
        QCOMPARE(vocabulary->resolve(QStringLiteral("土鸡蛋")), QStringLiteral("鸡蛋"));
        QCOMPARE(vocabulary->resolve(QStringLiteral(" 牛奶 ")), QStringLiteral("牛奶"));
        QVERIFY(vocabulary->resolve(QStringLiteral("豆浆")).isEmpty());
        QVERIFY(vocabulary->contains(QStringLiteral("牛奶")));
        QVERIFY(!vocabulary->contains(QStringLiteral("土鸡蛋")));
        QCOMPARE(vocabulary->namesByCategory().size(), 1);
    }

    void vocabularyAcceptsBareArrayAndRejectsGarbage()
    {
        QVERIFY(Vocabulary::fromJson(R"([{"name":"鸡蛋","category":"蛋奶"}])"));
        QString error;
        QVERIFY(!Vocabulary::fromJson("not json", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!Vocabulary::fromJson(R"({"items":[]})", &error));
        QVERIFY(!Vocabulary::fromJson(R"({"other":1})", &error));
        QVERIFY(!Vocabulary::fromFile(QStringLiteral("/nonexistent/vocabulary.json"), &error));
    }

    void syntheticVocabularyFixtureIsUsable()
    {
        const auto vocabulary = syntheticVocabulary();
        QVERIFY(vocabulary->entries().size() > 50);
        QCOMPARE(vocabulary->resolve(QStringLiteral("麻辣香肠")), QStringLiteral("香肠"));
    }

    void replayReturnsTheRecordedResult()
    {
        ReplayRecognizer recognizer(testDataPath(QStringLiteral("recordings/synthetic-cart.json")));
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        QSignalSpy failed(&recognizer, &RecognitionService::failed);

        const quint64 id = recognizer.recognize(requestFor(InputKind::Screenshot));
        QCOMPARE(succeeded.count(), 0); // never synchronous
        QTRY_COMPARE(succeeded.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(succeeded.at(0).at(0).toULongLong(), id);

        const auto result = succeeded.at(0).at(1).value<RecognitionResult>();
        QVERIFY(!result.isDemo);
        QCOMPARE(result.sourceType, SourceType::Cart);
        QCOMPARE(result.items.size(), 8);
        QCOMPARE(result.rawResponses.size(), 1);
        QCOMPARE(result.usage.totalTokens, 4100);
        QVERIFY(result.ignoredSummary.contains(QStringLiteral("换购")));

        const RecognizedItem &sausage = result.items.at(0);
        QCOMPARE(sausage.canonicalName, QStringLiteral("香肠")); // alias in the recording
        QCOMPARE(sausage.productionDateText, QStringLiteral("今天生产"));
        QCOMPARE(sausage.shelfLifeDays.fridge, std::optional<int>(30));
        QCOMPARE(result.items.at(1).count, 2);
        QCOMPARE(result.items.at(3).isGift, true);
        QCOMPARE(result.items.at(4).expiryDateText, QStringLiteral("2026-11-03到期"));
        QCOMPARE(result.items.at(5).canonicalName, uncategorizedName());
        QCOMPARE(result.items.at(6).category, ItemCategory::Irrelevant);
        QVERIFY(!result.items.at(7).selected);

        for (const RecognizedItem &item : result.items) {
            QVERIFY(item.canonicalName == uncategorizedName()
                    || requestFor(InputKind::Screenshot).vocabulary->contains(item.canonicalName));
        }
    }

    void replayOfOrderText()
    {
        ReplayRecognizer recognizer(testDataPath(QStringLiteral("recordings/synthetic-order-text.json")));
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        recognizer.recognize(requestFor(InputKind::OrderText));
        QTRY_COMPARE(succeeded.count(), 1);
        const auto result = succeeded.at(0).at(1).value<RecognitionResult>();
        QCOMPARE(result.sourceType, SourceType::Text);
        QCOMPARE(result.items.size(), 2);
        QCOMPARE(result.items.at(0).purchaseDateText, QStringLiteral("2026-10-18 19:42"));
        QCOMPARE(result.items.at(0).imageIndex, -1);
    }

    void replayOfSeveralResponsesMergesLikeBatches()
    {
        ReplayRecognizer recognizer(testDataPath(QStringLiteral("recordings/synthetic-two-batches.json")));
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        recognizer.recognize(requestFor(InputKind::Screenshot));
        QTRY_COMPARE(succeeded.count(), 1);
        const auto result = succeeded.at(0).at(1).value<RecognitionResult>();
        // The egg of image 23 repeated in the second batch (that image straddles both) is dropped;
        // the egg from image 40 is a different purchase.
        QCOMPARE(result.items.size(), 4);
        QCOMPARE(result.rawResponses.size(), 2);
        QCOMPARE(result.usage.totalTokens, 5400 + 4350);
    }

    void replayWithoutBatchSpansDropsNothing()
    {
        QString error;
        auto recording = readRecording(testDataPath(QStringLiteral("recordings/synthetic-two-batches.json")), &error);
        QVERIFY2(recording, qPrintable(error));
        recording->batches.clear();
        RecognitionRequest request;
        request.vocabulary = syntheticVocabulary();
        RecognitionError failure;
        const auto result = resultFromRecording(*recording, request, &failure);
        QVERIFY(result);
        QCOMPARE(result->items.size(), 5);
    }

    void recordingsAreWrittenOwnerOnly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("rec.json"));
        QFile existing(path); // a pre-existing world-readable file is tightened too
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.close();
        QVERIFY(existing.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup
                                        | QFileDevice::ReadOther));

        Recording recording;
        recording.responses = { QByteArray("{}") };
        QString error;
        QVERIFY2(writeRecording(path, recording, &error), qPrintable(error));
        const auto permissions = QFile::permissions(path);
        QCOMPARE(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
                                | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther),
                 QFileDevice::Permissions());
        QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    }

    void recordingPathsInsideTheRepositoryMustBeUnderPrivate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("private/recordings")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("test-data/recordings")));
        const QStringList roots{ dir.path() };

        QVERIFY(isRecordingPathAllowed(dir.filePath(QStringLiteral("private/recordings/a.json")), roots));
        QVERIFY(!isRecordingPathAllowed(dir.filePath(QStringLiteral("test-data/recordings/a.json")), roots));
        QVERIFY(!isRecordingPathAllowed(dir.filePath(QStringLiteral("a.json")), roots));
        QVERIFY(!isRecordingPathAllowed(dir.filePath(QStringLiteral("private/../a.json")), roots));
        QVERIFY(!isRecordingPathAllowed(dir.filePath(QStringLiteral("private-x/a.json")), roots)); // directory missing
        QVERIFY(!isRecordingPathAllowed(dir.filePath(QStringLiteral("private/recordings/missing-dir/a.json")), roots));

        QTemporaryDir outside;
        QVERIFY(outside.isValid());
        QVERIFY(isRecordingPathAllowed(outside.filePath(QStringLiteral("a.json")), roots));

        // A symlink pointing into the repository does not get around the rule.
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("test-data")), outside.filePath(QStringLiteral("link"))));
        QVERIFY(!isRecordingPathAllowed(outside.filePath(QStringLiteral("link/a.json")), roots));
    }

    void replayAppliesTheRequestsVocabulary()
    {
        RecognitionRequest request = requestFor(InputKind::Screenshot);
        request.vocabulary = std::make_shared<const Vocabulary>(*Vocabulary::fromJson(
            R"({"items":[{"name":"鸡蛋","category":"蛋奶"}]})"));
        ReplayRecognizer recognizer(testDataPath(QStringLiteral("recordings/synthetic-cart.json")));
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        recognizer.recognize(request);
        QTRY_COMPARE(succeeded.count(), 1);
        const auto result = succeeded.at(0).at(1).value<RecognitionResult>();
        int known = 0;
        for (const RecognizedItem &item : result.items)
            known += item.canonicalName != uncategorizedName();
        QCOMPARE(known, 1);
    }

    void replayOfMissingFileFails()
    {
        ReplayRecognizer recognizer(QStringLiteral("/nonexistent/recording.json"));
        QSignalSpy failed(&recognizer, &RecognitionService::failed);
        recognizer.recognize(requestFor(InputKind::Screenshot));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(1).value<RecognitionError>().kind, ErrorKind::BadResponse);
    }

    void cancelledReplayEmitsNothing()
    {
        ReplayRecognizer recognizer(testDataPath(QStringLiteral("recordings/synthetic-cart.json")));
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        QSignalSpy failed(&recognizer, &RecognitionService::failed);
        recognizer.cancel(recognizer.recognize(requestFor(InputKind::Screenshot)));
        QTest::qWait(100);
        QCOMPARE(succeeded.count() + failed.count(), 0);
    }

    void demoFromDirectoryIsFlaggedAsDemo()
    {
        DemoRecognizer recognizer(QStringLiteral(WHILEFRESH_DEMO_ASSETS_DIR));
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        recognizer.recognize(requestFor(InputKind::Screenshot));
        QTRY_COMPARE(succeeded.count(), 1);
        const auto result = succeeded.at(0).at(1).value<RecognitionResult>();
        QVERIFY(result.isDemo);
        QVERIFY(!result.items.isEmpty());
    }

    void demoBuiltInPresetsAreEmbedded()
    {
        DemoRecognizer recognizer;
        QSignalSpy succeeded(&recognizer, &RecognitionService::succeeded);
        recognizer.recognize(requestFor(InputKind::Screenshot));
        QTRY_COMPARE(succeeded.count(), 1);
        QVERIFY(succeeded.at(0).at(1).value<RecognitionResult>().isDemo);
    }

    void demoWithoutPresetForTheInputKindFails()
    {
        DemoRecognizer recognizer(QStringLiteral(WHILEFRESH_DEMO_ASSETS_DIR));
        QSignalSpy failed(&recognizer, &RecognitionService::failed);
        recognizer.recognize(requestFor(InputKind::Receipt));
        QTRY_COMPARE(failed.count(), 1);
    }

    void recordingRoundTrips()
    {
        QString error;
        const auto original = readRecording(testDataPath(QStringLiteral("recordings/synthetic-two-batches.json")), &error);
        QVERIFY2(original, qPrintable(error));
        QCOMPARE(original->inputKind, InputKind::Screenshot);
        QCOMPARE(original->imageCount, 45);
        QCOMPARE(original->promptVersion, QStringLiteral("v2"));
        QCOMPARE(original->responses.size(), 2);
        QCOMPARE(original->batches.size(), 2);
        QCOMPARE(original->batches.at(1).firstImage, 22);

        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("copy.json"));
        QVERIFY2(writeRecording(path, *original, &error), qPrintable(error));
        const auto copy = readRecording(path, &error);
        QVERIFY2(copy, qPrintable(error));
        QCOMPARE(copy->responses, original->responses);
        QCOMPARE(copy->imageCount, 45);
        QCOMPARE(copy->batches.size(), 2);
        QCOMPARE(copy->batches.at(0).lastImage, 22);
    }

    void makeRecordingSummarisesTheRequest()
    {
        RecognitionRequest request = requestFor(InputKind::OrderText);
        request.text = QStringLiteral("猪五花肉 500g");
        RecognitionResult result;
        result.rawResponses = { QByteArray("{}") };
        const Recording recording = makeRecording(request, result);
        QCOMPARE(recording.inputKind, InputKind::OrderText);
        QCOMPARE(recording.textLength, 9);
        QCOMPARE(recording.imageCount, 0);
        QVERIFY(!recording.promptVersion.isEmpty());
        QCOMPARE(recording.responses.size(), 1);
    }

    void readingGarbageFails()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("bad.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"format":99})");
        file.close();
        QString error;
        QVERIFY(!readRecording(path, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(RecordedRecognizerTest)

#include "recorded-recognizer-test.moc"
