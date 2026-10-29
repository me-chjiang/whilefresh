#include "core/catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTest>

using namespace whilefresh::core;

namespace {

IngredientCatalog catalogFrom(const QByteArray &json)
{
    QString error;
    auto catalog = IngredientCatalog::fromJson(json, &error);
    if (!catalog)
        qFatal("catalog: %s", qUtf8Printable(error));
    return *catalog;
}

const IngredientCatalog &realCatalog()
{
    static const IngredientCatalog catalog = [] {
        QString error;
        auto loaded = IngredientCatalog::fromFile(QStringLiteral(WHILEFRESH_VOCABULARY_FILE), &error);
        if (!loaded)
            qFatal("real vocabulary: %s", qUtf8Printable(error));
        return *loaded;
    }();
    return catalog;
}

}

class CatalogTest : public QObject
{
    Q_OBJECT

private slots:
    void standardNamesBeatAliasesWhateverTheFileOrder()
    {
        const auto catalog = catalogFrom(R"({"items":[
            {"name":"小白菜","category":"vegetable","aliases":["青菜","菜心"]},
            {"name":"菜心","category":"vegetable"},
            {"name":"西红柿","category":"vegetable","aliases":["番茄"]},
            {"name":"圣女果","category":"vegetable","aliases":["番茄","小番茄"]}]})");
        QCOMPARE(catalog.resolve(QStringLiteral("菜心")), QStringLiteral("菜心"));
        QCOMPARE(catalog.resolve(QStringLiteral(" 青菜 ")), QStringLiteral("小白菜"));
        QCOMPARE(catalog.resolve(QStringLiteral("番茄")), QStringLiteral("西红柿")); // first claim wins
        QCOMPARE(catalog.resolveFinest(QStringLiteral("新鲜菜心")), QStringLiteral("菜心"));
        QCOMPARE(catalog.resolveFinest(QStringLiteral("有机番茄")), QStringLiteral("西红柿"));
    }

    void resolveFinestBasics()
    {
        const auto catalog = catalogFrom(R"({
          "categories": [{"code":"meat","name":"肉类"}, {"code":"staple","name":"主食粮油"}],
          "items": [
            {"name":"猪肉", "category":"meat"},
            {"name":"梅花肉", "category":"meat"},
            {"name":"五花肉", "category":"meat", "aliases":["五花三层"], "parent":"猪肉"},
            {"name":"莲藕", "category":"vegetable", "aliases":["藕"]},
            {"name":"饼", "category":"staple"},
            {"name":"鸡蛋", "category":"egg_dairy"}
          ]})");
        QCOMPARE(catalog.resolveFinest(QStringLiteral("梅花肉")), QStringLiteral("梅花肉"));
        QCOMPARE(catalog.resolveFinest(QStringLiteral("黑猪梅花肉切片")), QStringLiteral("梅花肉"));
        QCOMPARE(catalog.resolveFinest(QStringLiteral("黑猪梅花肉切片300g")), QStringLiteral("梅花肉"));
        QCOMPARE(catalog.resolveFinest(QStringLiteral("土猪肉 500g")), QStringLiteral("猪肉"));
        QCOMPARE(catalog.resolveFinest(QStringLiteral("猪肉五花三层")), QStringLiteral("五花肉")); // child beats parent
        QCOMPARE(catalog.resolveFinest(QStringLiteral("藕粉 500g")), QString()); // 1 char only at the end
        QCOMPARE(catalog.resolveFinest(QStringLiteral("湖北 藕")), QStringLiteral("莲藕"));
        QCOMPARE(catalog.resolveFinest(QStringLiteral("鸡蛋饼")), QStringLiteral("饼"));
        QVERIFY(catalog.resolveFinest(QStringLiteral("完全未知")).isEmpty());
        QVERIFY(catalog.resolveFinest(QStringLiteral("  ")).isEmpty());
    }

    void objectCategoriesAreAccepted()
    {
        const auto catalog = catalogFrom(R"({"categories":{"meat":"肉类"},"items":[{"name":"猪肉","category":"meat"}]})");
        QCOMPARE(catalog.categoryDisplayName(QStringLiteral("meat")), QStringLiteral("肉类"));
    }

    void hierarchyTextGroupsFamiliesByCategory()
    {
        const auto catalog = catalogFrom(R"({
          "categories": [{"code":"vegetable","name":"蔬菜"}, {"code":"meat","name":"肉类"},
                         {"code":"seasoning","name":"调料"}, {"code":"other","name":"其他"}],
          "items": [
            {"name":"猪肉", "category":"meat"},
            {"name":"五花肉", "category":"meat", "parent":"猪肉"},
            {"name":"梅花肉", "category":"meat"},
            {"name":"里脊", "category":"meat", "parent":"猪肉"},
            {"name":"土豆", "category":"vegetable"},
            {"name":"料酒", "category":"seasoning"},
            {"name":"花雕酒", "category":"other", "parent":"料酒"},
            {"name":"孤儿", "category":"other", "parent":"不存在"}
          ]})");
        QCOMPARE(catalog.hierarchyText(),
                 QStringLiteral("【蔬菜】\n土豆\n【肉类】\n猪肉 > 五花肉、里脊\n梅花肉\n【调料】\n料酒 > 花雕酒\n【其他】\n孤儿"));
    }

    void realVocabularyHierarchyListsEveryEntryOnce()
    {
        const QString text = realCatalog().hierarchyText();
        QVERIFY(text.contains(QStringLiteral("\n猪肉 > 五花肉、前腿肉、里脊\n")));
        QVERIFY(text.contains(QStringLiteral("\n酱油 > 生抽、老抽、蒸鱼豉油\n")));
        QString flat = text;
        flat.replace(QRegularExpression(QStringLiteral("【[^】]*】")), QStringLiteral("\n"));
        flat.replace(QStringLiteral(" > "), QStringLiteral("、"));
        QStringList listed = flat.split(QRegularExpression(QStringLiteral("[\n、]")), Qt::SkipEmptyParts);
        QCOMPARE(listed.size(), realCatalog().entries().size());
        listed.removeDuplicates();
        QCOMPARE(listed.size(), realCatalog().entries().size());
    }

    // Reviewed cases (test-data/vocabulary/resolve-cases.json) against the shipped vocabulary.
    void resolveCases_data()
    {
        QTest::addColumn<QString>("raw");
        QTest::addColumn<QString>("expected");
        QFile file(QStringLiteral(WHILEFRESH_RESOLVE_CASES_FILE));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonArray cases = QJsonDocument::fromJson(file.readAll()).array();
        QVERIFY(cases.size() >= 20);
        for (const QJsonValue &value : cases) {
            const QJsonObject object = value.toObject();
            const QString raw = object.value(QStringLiteral("raw")).toString();
            QTest::newRow(qPrintable(raw)) << raw << object.value(QStringLiteral("expected")).toString();
        }
    }

    void resolveCases()
    {
        QFETCH(QString, raw);
        QFETCH(QString, expected);
        if (!expected.isEmpty())
            QVERIFY2(realCatalog().contains(expected), qPrintable(expected));
        QCOMPARE(realCatalog().resolveFinest(raw), expected);
    }
};

QTEST_MAIN(CatalogTest)
#include "catalog-test.moc"
