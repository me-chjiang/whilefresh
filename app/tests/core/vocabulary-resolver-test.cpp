#include "core/vocabulary-resolver.h"
#include "core-test-support.h"

#include <QTest>

using namespace whilefresh::core;

class VocabularyResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void userMappingComesFirst()
    {
        const auto catalog = testing::smallCatalog();
        MemoryNameMappingStore mappings;
        QVERIFY(mappings.remember(QStringLiteral(" 农家  土猪肉 "), QStringLiteral("五花肉")));
        const VocabularyResolver resolver(catalog, &mappings);
        const auto resolution = resolver.resolve(QStringLiteral("农家 土猪肉"), QStringLiteral("猪肉"));
        QCOMPARE(resolution.canonical, QStringLiteral("五花肉"));
        QCOMPARE(resolution.source, NameSource::UserMapping);
    }

    void aMappingToANameNoLongerInTheCatalogIsIgnored()
    {
        const auto catalog = testing::smallCatalog();
        MemoryNameMappingStore mappings;
        mappings.remember(QStringLiteral("某商品"), QStringLiteral("已删除的词条"));
        const VocabularyResolver resolver(catalog, &mappings);
        QCOMPARE(resolver.resolve(QStringLiteral("某商品")).source, NameSource::Unresolved);
    }

    void theSuggestionIsRefinedOnlyToADescendant()
    {
        const auto catalog = testing::smallCatalog();
        const VocabularyResolver resolver(catalog);
        auto resolution = resolver.resolve(QStringLiteral("精选五花肉 500g"), QStringLiteral("猪肉"));
        QCOMPARE(resolution.canonical, QStringLiteral("五花肉"));
        QCOMPARE(resolution.source, NameSource::Recognized);
        // 五花肉 is no descendant of 土豆: the model's reading of the whole title stands.
        resolution = resolver.resolve(QStringLiteral("土豆炖五花肉 料理包"), QStringLiteral("土豆"));
        QCOMPARE(resolution.canonical, QStringLiteral("土豆"));
        resolution = resolver.resolve(QStringLiteral("番茄"), QStringLiteral("番茄"));
        QCOMPARE(resolution.canonical, QStringLiteral("西红柿")); // aliases are normalised
    }

    void localMatchWhenTheRecognizerGaveNothingUsable()
    {
        const auto catalog = testing::smallCatalog();
        const VocabularyResolver resolver(catalog);
        auto resolution = resolver.resolve(QStringLiteral("有机小番茄 250g"), QStringLiteral("未归类"));
        QCOMPARE(resolution.canonical, QStringLiteral("圣女果"));
        QCOMPARE(resolution.source, NameSource::LocalMatch);
        resolution = resolver.resolve(QStringLiteral("洗衣液 2kg"));
        QVERIFY(!resolution.resolved());
        QCOMPARE(resolution.source, NameSource::Unresolved);
    }

    void unresolvedNamesAreCollectedOnce()
    {
        const QStringList raw{ QStringLiteral("花胶"), QStringLiteral("土豆"), QStringLiteral(" 花胶 "),
                               QStringLiteral("罗汉果") };
        const QList<NameResolution> resolutions{ {}, { QStringLiteral("土豆"), NameSource::LocalMatch }, {}, {} };
        QCOMPARE(VocabularyResolver::unresolvedNames(raw, resolutions),
                 (QStringList{ QStringLiteral("花胶"), QStringLiteral("罗汉果") }));
    }
};

QTEST_MAIN(VocabularyResolverTest)
#include "vocabulary-resolver-test.moc"
