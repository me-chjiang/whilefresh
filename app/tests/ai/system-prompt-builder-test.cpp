#include "ai/system-prompt-builder.h"

#include <QTest>

using namespace whilefresh::ai;

class SystemPromptBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void baseOnly()
    {
        QCOMPARE(SystemPromptBuilder(QStringLiteral("你是厨房助手。\n")).build(), QStringLiteral("你是厨房助手。"));
    }

    void sectionsKeepTheirOrderAndThePreferenceSummaryComesLast()
    {
        SystemPromptBuilder builder(QStringLiteral("你是厨房助手。"));
        builder.setPreferenceSummary(QStringLiteral("不吃香菜；少油。"))
            .addSection(QStringLiteral("库存"), QStringLiteral("鸡蛋、番茄"))
            .addSection(QStringLiteral("常备调料"), QStringLiteral("盐、生抽"));
        QCOMPARE(builder.build(), QStringLiteral("你是厨房助手。\n\n"
                                                 "【库存】\n鸡蛋、番茄\n\n"
                                                 "【常备调料】\n盐、生抽\n\n"
                                                 "【用户偏好】\n不吃香菜；少油。"));

        builder.addSection(QStringLiteral("补充"), QStringLiteral("两人份"));
        QVERIFY(builder.build().endsWith(QStringLiteral("【用户偏好】\n不吃香菜；少油。")));
        QVERIFY(builder.build().indexOf(QStringLiteral("【补充】")) < builder.build().indexOf(QStringLiteral("【用户偏好】")));
    }

    void emptySectionsAndAnEmptySummaryAreLeftOut()
    {
        SystemPromptBuilder builder(QStringLiteral("基础"));
        builder.addSection(QStringLiteral("空"), QStringLiteral("  \n"))
            .addSection(QStringLiteral("库存"), QStringLiteral(" 鸡蛋 "))
            .setPreferenceSummary(QStringLiteral("  "));
        QCOMPARE(builder.build(), QStringLiteral("基础\n\n【库存】\n鸡蛋"));
        QVERIFY(!builder.build().contains(QStringLiteral("用户偏好")));
    }
};

QTEST_APPLESS_MAIN(SystemPromptBuilderTest)

#include "system-prompt-builder-test.moc"
