#include "core/pantry.h"
#include "core/shelf-life.h"
#include "data/builtin-data.h"

#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::core;
using namespace whilefresh::data;

// Integration with the real shipped data (app/assets/data, embedded as qrc).
class BuiltinDataTest : public QObject
{
    Q_OBJECT

    std::shared_ptr<const BuiltinData> data;

private slots:
    void initTestCase()
    {
        QString error;
        data = BuiltinData::builtIn(&error);
        QVERIFY2(data, qPrintable(error));
        QCOMPARE(BuiltinData::builtIn().get(), data.get()); // loaded once
    }

    void sizes()
    {
        QCOMPARE(data->catalog->entries().size(), 581);
        QVERIFY(!data->shelfLife->isEmpty());
        QCOMPARE(data->shelfLife->sourceName(), QStringLiteral("USDA FoodKeeper"));
        QCOMPARE(data->catalog->categoryDisplayName(QStringLiteral("vegetable")), QStringLiteral("蔬菜"));
    }

    void realReferenceValues()
    {
        const ShelfLifeContext context{ data->catalog.get(), data->shelfLife.get(), nullptr, nullptr };
        ShelfLifeInput input;
        input.entryDate = QDate(2026, 10, 21);
        input.storage = Storage::Fridge;

        input.name = QStringLiteral("鸡胸肉");
        auto result = resolveShelfLife(input, context);
        QCOMPARE(result.source, ShelfLifeSource::Reference);
        QCOMPARE(result.days, std::optional<int>(2));

        input.name = QStringLiteral("空心菜");
        result = resolveShelfLife(input, context);
        QCOMPARE(result.source, ShelfLifeSource::Reference);
        QCOMPARE(result.days, std::optional<int>(4));

        // 土豆 has no fridge value in the table: the AI's number is used, or nothing is known.
        input.name = QStringLiteral("土豆");
        result = resolveShelfLife(input, context);
        QCOMPARE(result.source, ShelfLifeSource::None);
        input.aiDays.fridge = 14;
        result = resolveShelfLife(input, context);
        QCOMPARE(result.source, ShelfLifeSource::AiEstimate);
        QCOMPARE(result.days, std::optional<int>(14));
        input.storage = Storage::Room;
        QCOMPARE(resolveShelfLife(input, context).source, ShelfLifeSource::Reference);
    }

    void parentFallbackOnRealData()
    {
        // Find a vocabulary entry that is not in the table but has a listed ancestor.
        const IngredientCatalog &catalog = *data->catalog;
        for (const CatalogEntry &entry : catalog.entries()) {
            if (entry.parent.isEmpty() || data->shelfLife->row(entry.name, nullptr))
                continue;
            QString matched;
            const ShelfLifeReference *row = data->shelfLife->row(entry.name, &catalog, &matched);
            if (!row)
                continue;
            QVERIFY(catalog.ancestors(entry.name).contains(matched));
            return;
        }
        QSKIP("every child with a listed ancestor is listed itself");
    }

    void pantryDefaultsOnRealData()
    {
        const PantryRules pantry(data->catalog.get());
        QVERIFY(pantry.isAtHome(QStringLiteral("盐")));
        QVERIFY(pantry.isAtHome(QStringLiteral("食用油")));
    }

    void missingShelfLifeFileIsAnEmptyTable()
    {
        QTemporaryDir dir;
        QVERIFY(QFile::copy(BuiltinData::builtInDirectory() + QStringLiteral("/vocabulary.json"),
                            dir.filePath(QStringLiteral("vocabulary.json"))));
        QString error;
        const auto loaded = BuiltinData::load(dir.path(), &error);
        QVERIFY2(loaded, qPrintable(error));
        QVERIFY(loaded->shelfLife->isEmpty());
        QVERIFY(!BuiltinData::load(dir.filePath(QStringLiteral("missing")), &error));
    }
};

QTEST_GUILESS_MAIN(BuiltinDataTest)
#include "builtin-data-test.moc"
