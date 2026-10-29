#include "ui/app-model.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh;

class AppModelTest : public QObject
{
    Q_OBJECT
    std::shared_ptr<const data::BuiltinData> builtin;
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<ui::AppModel> model;

    qint64 add(const QString &name)
    {
        if (!model->addManual(name, QStringLiteral("1份"), QStringLiteral("冷藏")).value("ok").toBool())
            return 0;
        return model->items().last().toMap().value("id").toLongLong();
    }

private slots:
    void initTestCase()
    {
        builtin = data::BuiltinData::builtIn();
        QVERIFY(builtin);
    }

    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        model = std::make_unique<ui::AppModel>(nullptr, builtin, dir->path());
        QVERIFY(model->ok());
    }

    void cleanup()
    {
        model.reset();
        dir.reset();
    }

    void selectionOnlyUpdatesPreviewUntilConfirmed()
    {
        const auto tomato = add(QStringLiteral("西红柿"));
        QVERIFY(tomato > 0);
        QSignalSpy general(model.get(), &ui::AppModel::changed);
        QSignalSpy recipes(model.get(), &ui::AppModel::recipesChanged);
        model->toggleSelected(tomato);
        QCOMPARE(model->clearPreview().value("selectedCount").toInt(), 1);
        QVERIFY(model->clearPreview().value("candidateCount").toInt() > 0);
        QVERIFY(model->clearRecipes().isEmpty());
        QVERIFY(!model->clearResultsReady());
        QCOMPARE(general.size(), 0);
        QCOMPARE(recipes.size(), 0);
        QVERIFY(model->findClearRecipes());
        QVERIFY(model->clearResultsReady());
        QVERIFY(!model->clearRecipes().isEmpty());
        for (const auto &value : model->clearRecipes()) {
            const auto result = value.toMap();
            QVERIFY(result.value("usedSelected").toStringList().contains(QStringLiteral("西红柿")));
            QVERIFY(result.value("unusedSelected").toStringList().isEmpty());
        }
        model->toggleSelected(tomato);
        QVERIFY(!model->clearResultsReady());
        QVERIFY(model->clearRecipes().isEmpty());
        QVERIFY(!model->findClearRecipes());
    }

    void shoppingDoesNotInvalidateConfirmedQuery()
    {
        const auto tomato = add(QStringLiteral("西红柿"));
        model->toggleSelected(tomato);
        QVERIFY(model->findClearRecipes());
        const auto results = model->clearRecipes();
        const auto summary = model->clearResultsSummary();
        QString id;
        for (const auto &value : results) {
            if (value.toMap().value("missingCount").toInt() > 0) {
                id = value.toMap().value("id").toString();
                break;
            }
        }
        QVERIFY(!id.isEmpty());
        QSignalSpy recipes(model.get(), &ui::AppModel::recipesChanged);
        QVERIFY(model->addMissing(id) > 0);
        QVERIFY(model->clearResultsReady());
        QCOMPARE(model->clearResultsSummary(), summary);
        QCOMPARE(model->clearRecipes(), results);
        QCOMPARE(recipes.size(), 0);
        QVERIFY(!model->shopping().isEmpty());
    }

    void inventoryAndModeChangesInvalidateQuery()
    {
        const auto tomato = add(QStringLiteral("西红柿"));
        model->toggleSelected(tomato);
        QVERIFY(model->findClearRecipes());
        model->markAbsent(tomato);
        QVERIFY(model->selectedIds().isEmpty());
        QVERIFY(!model->clearResultsReady());
        model->toggleSelected(tomato);
        QVERIFY(model->selectedIds().isEmpty());
        model->setDemoMode(true);
        QVERIFY(model->demoMode());
        QVERIFY(!model->clearResultsReady());
        QVERIFY(!model->items().isEmpty());
        model->setDemoMode(false);
        QVERIFY(!model->demoMode());
        QVERIFY(model->items().isEmpty());
        QVERIFY(model->selectedIds().isEmpty());
    }

    void seasoningIsNotPartOfCoverageDenominator()
    {
        const auto tomato = add(QStringLiteral("西红柿"));
        const auto salt = add(QStringLiteral("盐"));
        model->toggleSelected(tomato);
        model->toggleSelected(salt);
        QCOMPARE(model->clearPreview().value("selectedCount").toInt(), 1);
        QVERIFY(model->clearPreview().value("completeCount").toInt() > 0);
        QVERIFY(model->findClearRecipes());
        for (const auto &result : model->clearRecipes())
            QVERIFY(!result.toMap().value("unusedSelected").toStringList().contains(QStringLiteral("盐")));
        model->toggleSelected(tomato);
        QCOMPARE(model->clearPreview().value("selectedCount").toInt(), 0);
        QVERIFY(!model->findClearRecipes());
    }

    void detailKeepsOriginalIngredientsAndNotes()
    {
        const auto &recipe = builtin->recipes->recipes().first();
        const auto detail = model->recipeDetail(recipe.id);
        int count = 0;
        for (const auto &ingredient : recipe.ingredients)
            count += ingredient.kind != QLatin1String("tool");
        QCOMPARE(detail.value("ingredients").toList().size(), count);
        QCOMPARE(detail.value("steps").toStringList(), recipe.steps);
        QCOMPARE(detail.value("summary").toString(), recipe.summary);
        QCOMPARE(detail.value("notes").toString(), recipe.notes);
        QCOMPARE(detail.value("quantities").toList().size(), recipe.quantities.size());
        QVERIFY(!detail.value("sourceName").toString().isEmpty());
    }

    void invalidRuleDoesNotReplacePreviousValue()
    {
        QVERIFY(model->saveRule(QStringLiteral("西红柿"), QStringLiteral("冷藏"), QStringLiteral("冷藏"), QStringLiteral("7")));
        const auto rules = model->rules();
        QSignalSpy notice(model.get(), &ui::AppModel::notice);
        QVERIFY(!model->saveRule(QStringLiteral("西红柿"), QStringLiteral("冷藏"), QStringLiteral("冷冻"), QStringLiteral("-1")));
        QCOMPARE(model->rules(), rules);
        QCOMPARE(notice.size(), 1);
    }
};

QTEST_GUILESS_MAIN(AppModelTest)
#include "app-model-test.moc"
