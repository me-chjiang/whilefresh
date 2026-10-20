#include "recognition/vocabulary.h"

#include "recognition-test-support.h"

#include <QRegularExpression>
#include <QTest>

using namespace whilefresh::recognition;
using namespace whilefresh::recognition::testing;

namespace {

QStringList groupLabels(const Vocabulary &vocabulary)
{
    QStringList labels;
    for (const auto &group : vocabulary.namesByCategory())
        labels.append(group.first);
    return labels;
}

}

class VocabularyTest : public QObject
{
    Q_OBJECT

private slots:
    void categoriesKeepFileOrderAndShowChineseNames()
    {
        // Sorted by key this would be aaa, meat, vegetable; the file order must win.
        const auto vocabulary = Vocabulary::fromJson(
            "{\"version\":1,\"note\":\"\\\"categories\\\": {x:1}\","
            "\"categories\":{\"meat\":\"肉类\",\"vegetable\":\"蔬菜\",\"aaa\":\"甲类\"},"
            "\"items\":[{\"name\":\"白菜\",\"category\":\"vegetable\"},"
            "{\"name\":\"五花肉\",\"category\":\"meat\",\"aliases\":[\"猪五花\"],"
            "\"parent\":null,\"pantry_default\":false,\"bulk_default\":true},"
            "{\"name\":\"神秘物\",\"category\":\"aaa\"}]}");
        QVERIFY(vocabulary);
        QCOMPARE(groupLabels(*vocabulary), (QStringList{ "肉类", "蔬菜", "甲类" }));
        QCOMPARE(vocabulary->categories().size(), 3);
        QCOMPARE(vocabulary->categories().first().first, QStringLiteral("meat"));
        QCOMPARE(vocabulary->entries().at(1).category, QStringLiteral("meat")); // the code is kept
        QCOMPARE(vocabulary->categoryDisplayName(QStringLiteral("vegetable")), QStringLiteral("蔬菜"));
        QCOMPARE(vocabulary->resolve(QStringLiteral("猪五花")), QStringLiteral("五花肉"));
    }

    void undeclaredCategoriesFollowDeclaredOnes()
    {
        const auto vocabulary = Vocabulary::fromJson(
            R"({"categories":{"meat":"肉类","vegetable":"蔬菜"},
                "items":[{"name":"x","category":"mystery"},{"name":"白菜","category":"vegetable"},
                         {"name":"y","category":""},{"name":"z","category":"mystery"}]})");
        QVERIFY(vocabulary);
        // "meat" has no entries and is skipped; the empty category has no display name.
        QCOMPARE(groupLabels(*vocabulary), (QStringList{ "蔬菜", "mystery", "" }));
    }

    void withoutCategoriesTheCategoryIsTheDisplayName()
    {
        const auto vocabulary = Vocabulary::fromJson(
            R"({"items":[{"name":"鸡蛋","category":"蛋奶"},{"name":"白菜","category":"蔬菜"}]})");
        QVERIFY(vocabulary);
        QVERIFY(vocabulary->categories().isEmpty());
        QCOMPARE(groupLabels(*vocabulary), (QStringList{ "蛋奶", "蔬菜" }));
    }

    void syntheticFixtureUsesTheOfficialFormat()
    {
        const auto vocabulary = syntheticVocabulary();
        QCOMPARE(vocabulary->categories().size(), 10);
        QCOMPARE(vocabulary->categories().first().first, QStringLiteral("vegetable"));
        QCOMPARE(vocabulary->categories().last().first, QStringLiteral("other"));
        QCOMPARE(groupLabels(*vocabulary).first(), QStringLiteral("蔬菜"));
    }

    void aStandardNameBeatsAnAliasWhateverTheFileOrder()
    {
        VocabularyLoadReport report;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("vocabulary: ignored 0 entries and 1 aliases: 菜心"));
        const auto vocabulary = Vocabulary::fromJson(
            R"({"items":[{"name":"小白菜","category":"v","aliases":["菜心","青菜"]},
                         {"name":"菜心","category":"v"}]})", nullptr, &report);
        QVERIFY(vocabulary);
        QCOMPARE(vocabulary->resolve(QStringLiteral("菜心")), QStringLiteral("菜心"));
        QCOMPARE(vocabulary->resolve(QStringLiteral("青菜")), QStringLiteral("小白菜"));
        QCOMPARE(report.ignoredAliases, 1);
        QCOMPARE(report.ignoredEntries, 0);
        QCOMPARE(report.ignoredNames, QStringList{ "菜心" });
    }

    void aliasesClaimedTwiceGoToTheFirstEntry()
    {
        VocabularyLoadReport report;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("ignored 0 entries and 1 aliases: 番茄"));
        const auto vocabulary = Vocabulary::fromJson(
            R"({"items":[{"name":"西红柿","category":"v","aliases":["番茄"]},
                         {"name":"圣女果","category":"v","aliases":["番茄","小番茄"]}]})", nullptr, &report);
        QVERIFY(vocabulary);
        QCOMPARE(vocabulary->resolve(QStringLiteral("番茄")), QStringLiteral("西红柿"));
        QCOMPARE(vocabulary->resolve(QStringLiteral("小番茄")), QStringLiteral("圣女果"));
        QCOMPARE(report.ignoredAliases, 1);
    }

    void repeatedAndEmptyNamesAreCounted()
    {
        VocabularyLoadReport report;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("ignored 2 entries and 0 aliases"));
        const auto vocabulary = Vocabulary::fromJson(
            R"({"items":[{"name":"鸡蛋","category":"v"},{"name":"鸡蛋","category":"w"},{"name":" ","category":"v"}]})",
            nullptr, &report);
        QVERIFY(vocabulary);
        QCOMPARE(vocabulary->entries().size(), 1);
        QCOMPARE(report.ignoredEntries, 2);
    }

    void cleanFilesReportNothingAndStaySilent()
    {
        VocabularyLoadReport report;
        QVERIFY(Vocabulary::fromJson(R"({"items":[{"name":"鸡蛋","category":"v","aliases":["鸡蛋","土鸡蛋","土鸡蛋"]}]})",
                                     nullptr, &report));
        QCOMPARE(report.ignoredEntries + report.ignoredAliases, 0);
        QVERIFY(report.ignoredNames.isEmpty());
    }

    void legacyFormatStillLoads()
    {
        QString error;
        const auto vocabulary = Vocabulary::fromJson(
            R"({"items":[{"name":"鸡蛋","category":"蛋奶","aliases":["土鸡蛋"],"staple":false},
                         {"name":"牛奶","category":"蛋奶"}]})", &error);
        QVERIFY2(vocabulary, qPrintable(error));
        QCOMPARE(vocabulary->namesByCategory().size(), 1);
        QCOMPARE(groupLabels(*vocabulary).first(), QStringLiteral("蛋奶"));
        QCOMPARE(vocabulary->resolve(QStringLiteral("土鸡蛋")), QStringLiteral("鸡蛋"));
    }
};

QTEST_GUILESS_MAIN(VocabularyTest)

#include "vocabulary-test.moc"
