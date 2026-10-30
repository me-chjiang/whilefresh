#include "core/fridge-groups.h"

#include "core-test-support.h"

#include <QTest>

using namespace whilefresh::core;
using namespace whilefresh::core::testing;

Q_DECLARE_METATYPE(Storage)
Q_DECLARE_METATYPE(ItemKind)

namespace {

const QDate today(2026, 10, 30);

FridgeItem item(qint64 id, const QString &name, std::optional<int> daysLeft, Storage storage = Storage::Fridge,
                ItemKind kind = ItemKind::Food)
{
    FridgeItem item;
    item.id = id;
    item.canonicalName = name;
    item.displayName = name;
    item.storage = storage;
    item.kind = kind;
    if (daysLeft) {
        item.shelfLifeDays = 10;
        item.expiryDate = today.addDays(*daysLeft);
    }
    return item;
}

QStringList names(const QList<FridgeItem> &items)
{
    QStringList list;
    for (const auto &item : items)
        list.append(item.displayName);
    return list;
}

}

class FridgeGroupsTest : public QObject
{
    Q_OBJECT

private slots:
    void expiredFirstThenDaysLeftThenUnknown()
    {
        const QList<FridgeItem> items{
            item(1, QStringLiteral("牛奶"), 5),      item(2, QStringLiteral("豆腐"), std::nullopt),
            item(3, QStringLiteral("鸡蛋"), 2),      item(4, QStringLiteral("酸奶"), -3),
            item(5, QStringLiteral("菠菜"), 0),      item(6, QStringLiteral("白菜"), 2),
            item(7, QStringLiteral("芹菜"), std::nullopt),
        };
        // Same days left: by name; unknown ones also by name, after everything dated.
        QCOMPARE(names(sortForFridge(items, today)),
                 (QStringList{ "酸奶", "菠菜", "白菜", "鸡蛋", "牛奶", "芹菜", "豆腐" }));
    }

    void groupsComeInFixedOrder()
    {
        const QList<FridgeItem> items{
            item(1, QStringLiteral("挂面"), std::nullopt, Storage::Room, ItemKind::StapleFood),
            item(2, QStringLiteral("生抽"), 100, Storage::Room, ItemKind::StapleSeasoning),
            item(3, QStringLiteral("土豆"), 20, Storage::Room),
            item(4, QStringLiteral("虾仁"), 60, Storage::Freezer),
            item(5, QStringLiteral("牛奶"), 1, Storage::Fridge),
            item(6, QStringLiteral("豆瓣酱"), -1, Storage::Fridge, ItemKind::StapleSeasoning),
        };
        const auto sorted = sortForFridge(items, today);
        QCOMPARE(names(sorted), (QStringList{ "牛奶", "虾仁", "土豆", "豆瓣酱", "生抽", "挂面" }));
        QStringList groups;
        for (const auto &entry : sorted)
            groups.append(fridgeGroupName(fridgeGroup(entry)));
        QCOMPARE(groups, (QStringList{ "冷藏", "冷冻", "常温", "常备调料", "常备调料", "常备食材" }));
    }

    void sortIsStableForEqualKeys()
    {
        const QList<FridgeItem> items{ item(9, QStringLiteral("鸡蛋"), 3), item(4, QStringLiteral("鸡蛋"), 3) };
        const auto sorted = sortForFridge(items, today);
        QCOMPARE(sorted.at(0).id, qint64(4));
        QCOMPARE(sorted.at(1).id, qint64(9));
    }

    void kindRule_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("seasoning");
        QTest::addColumn<Storage>("storage");
        QTest::addColumn<ItemKind>("expected");

        QTest::newRow("vocabulary seasoning") << "生抽" << false << Storage::Room << ItemKind::StapleSeasoning;
        QTest::newRow("recognized seasoning, unknown name") << "某品牌酱" << true << Storage::Fridge << ItemKind::StapleSeasoning;
        QTest::newRow("pantry staple anywhere") << "食用油" << false << Storage::Fridge << ItemKind::StapleFood;
        QTest::newRow("dry noodles at room temperature") << "挂面" << false << Storage::Room << ItemKind::StapleFood;
        QTest::newRow("staple kept cold") << "鲜面条" << false << Storage::Fridge << ItemKind::Food;
        QTest::newRow("short room life") << "面包" << false << Storage::Room << ItemKind::Food;
        QTest::newRow("ordinary food") << "土豆" << false << Storage::Room << ItemKind::Food;
        QTest::newRow("unknown food") << "未归类" << false << Storage::Room << ItemKind::Food;
    }

    void kindRule()
    {
        QFETCH(QString, name);
        QFETCH(bool, seasoning);
        QFETCH(Storage, storage);
        QFETCH(ItemKind, expected);
        IngredientCatalog catalog = smallCatalog();
        catalog.add({ QStringLiteral("挂面"), QStringLiteral("staple"), {}, {}, false, false });
        catalog.add({ QStringLiteral("鲜面条"), QStringLiteral("staple"), {}, {}, false, false });
        catalog.add({ QStringLiteral("面包"), QStringLiteral("staple"), {}, {}, false, false });
        ShelfLifeTable table = smallShelfLifeTable();
        ShelfLifeReference bread;
        bread.days.room = 7;
        table.insert(QStringLiteral("面包"), bread);
        QCOMPARE(classifyItemKind(name, seasoning, storage, &catalog, &table), expected);
    }

    void staplesWithoutShelfLifeAskForOne()
    {
        auto staple = item(1, QStringLiteral("挂面"), std::nullopt, Storage::Room, ItemKind::StapleFood);
        QVERIFY(staple.needsShelfLife());
        staple.expiryDate = today.addDays(300);
        QVERIFY(!staple.needsShelfLife());
        QVERIFY(!item(2, QStringLiteral("豆腐"), std::nullopt).needsShelfLife());
    }
};

QTEST_GUILESS_MAIN(FridgeGroupsTest)
#include "fridge-groups-test.moc"
