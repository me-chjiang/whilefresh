#include "recipes/xiachufang-yaml-parser.h"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace whilefresh::recipes {

namespace {

constexpr int kSupportedSchemaMajor = 1;

struct ContractError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(const std::string &what)
{
    throw ContractError(what);
}

bool present(const YAML::Node &node)
{
    return node.IsDefined() && !node.IsNull();
}

// A missing key yields an invalid node in yaml-cpp that throws on any further use; hand out a
// plain null node instead so absent and null look the same to callers.
const YAML::Node child(const YAML::Node &map, const char *key)
{
    if (!map.IsDefined() || !map.IsMap()) {
        return YAML::Node();
    }
    const YAML::Node node = map[key];
    return node.IsDefined() ? node : YAML::Node();
}

QString text(const YAML::Node &map, const char *key)
{
    const YAML::Node node = child(map, key);
    if (!present(node)) {
        return {};
    }
    if (!node.IsScalar()) {
        fail(std::string(key) + " must be a scalar");
    }
    return QString::fromStdString(node.Scalar());
}

QStringList textList(const YAML::Node &map, const char *key)
{
    const YAML::Node node = child(map, key);
    QStringList result;
    if (!present(node)) {
        return result;
    }
    if (!node.IsSequence()) {
        fail(std::string(key) + " must be a list");
    }
    for (const YAML::Node &item : node) {
        if (!item.IsScalar()) {
            fail(std::string(key) + " must hold scalars");
        }
        result.append(QString::fromStdString(item.Scalar()));
    }
    return result;
}

std::optional<double> optionalDouble(const YAML::Node &map, const char *key)
{
    const YAML::Node node = child(map, key);
    if (!present(node)) {
        return std::nullopt;
    }
    double value = 0;
    if (!node.IsScalar() || !YAML::convert<double>::decode(node, value)) {
        fail(std::string(key) + " must be a number");
    }
    return value;
}

std::optional<int> optionalInt(const YAML::Node &map, const char *key)
{
    const std::optional<double> value = optionalDouble(map, key);
    if (!value) {
        return std::nullopt;
    }
    return static_cast<int>(*value);
}

int intOr(const YAML::Node &map, const char *key, int fallback)
{
    return optionalInt(map, key).value_or(fallback);
}

template<typename Fn>
void eachMap(const YAML::Node &map, const char *key, Fn fn)
{
    const YAML::Node node = child(map, key);
    if (!present(node)) {
        return;
    }
    if (!node.IsSequence()) {
        fail(std::string(key) + " must be a list");
    }
    for (const YAML::Node &item : node) {
        if (!item.IsMap()) {
            fail(std::string(key) + " must hold mappings");
        }
        fn(item);
    }
}

RecipeAttribution attributionOf(const YAML::Node &item, const QString &provider)
{
    RecipeAttribution attribution;
    attribution.sourceName = QStringLiteral("xiachufang");
    attribution.provider = provider;
    attribution.sourceUrl = text(item, "source_url");
    attribution.author = text(item, "author");
    return attribution;
}

RecipeSummary summaryOf(const YAML::Node &item, const QString &sourceName, const QString &provider)
{
    RecipeSummary summary;
    summary.id = text(item, "id");
    summary.name = text(item, "name");
    summary.description = text(item, "description");
    summary.imageUrl = text(item, "image_url");
    summary.attribution = attributionOf(item, provider);
    summary.attribution.sourceName = sourceName;
    if (summary.id.isEmpty() || summary.name.isEmpty()) {
        fail("recipe needs id and name");
    }
    return summary;
}

WebRecipe recipeOf(const YAML::Node &item, const QString &sourceName, const QString &provider)
{
    const RecipeSummary summary = summaryOf(item, sourceName, provider);
    WebRecipe recipe;
    recipe.id = summary.id;
    recipe.name = summary.name;
    recipe.description = summary.description;
    recipe.imageUrl = summary.imageUrl;
    recipe.attribution = summary.attribution;
    recipe.category = text(item, "category");
    recipe.region = text(item, "region");
    recipe.rating = optionalDouble(item, "rating");
    recipe.ratingCount = optionalInt(item, "rating_count");
    recipe.madeCount = optionalInt(item, "made_count");
    recipe.notes = text(item, "notes");
    recipe.tips = textList(item, "tips");
    eachMap(item, "ingredients", [&](const YAML::Node &node) {
        RecipeIngredient ingredient;
        ingredient.raw = text(node, "raw");
        ingredient.name = text(node, "name");
        ingredient.amount = text(node, "amount");
        ingredient.category = text(node, "category");
        ingredient.required = text(node, "required");
        ingredient.evidence = text(node, "evidence");
        ingredient.notes = text(node, "notes");
        ingredient.substitutes = textList(node, "substitutes");
        if (ingredient.name.isEmpty()) {
            fail("ingredient needs a name");
        }
        recipe.ingredients.append(ingredient);
    });
    eachMap(item, "steps", [&](const YAML::Node &node) {
        RecipeStep step;
        step.order = intOr(node, "order", int(recipe.steps.size()) + 1);
        step.text = text(node, "text");
        step.imageUrl = text(node, "image_url");
        recipe.steps.append(step);
    });
    return recipe;
}

RecipeErrorKind errorKindOf(const QString &message, const QStringList &incomplete)
{
    const QString lower = message.toLower();
    if (lower.contains(QLatin1String("timed out")) || lower.contains(QLatin1String("timeout"))) {
        return RecipeErrorKind::Timeout;
    }
    if (lower.contains(QLatin1String("not found")) || lower.contains(QLatin1String("404"))) {
        return RecipeErrorKind::NotFound;
    }
    if (incomplete.contains(QLatin1String("query"))) {
        return RecipeErrorKind::InvalidQuery;
    }
    return RecipeErrorKind::Unavailable;
}

}

ParsedRecipeDocument parseXiachufangYaml(const QByteArray &yaml)
{
    ParsedRecipeDocument parsed;
    try {
        const YAML::Node root = YAML::Load(std::string(yaml.constData(), size_t(yaml.size())));
        if (!root.IsMap()) {
            fail("document is not a mapping");
        }

        RecipeDocument doc;
        doc.schemaVersion = text(root, "schema_version");
        const QList<QStringView> version = QStringView(doc.schemaVersion).split(u'.');
        bool majorOk = false;
        const int major = version.isEmpty() ? 0 : version.first().toInt(&majorOk);
        if (!majorOk) {
            fail("missing or malformed schema_version");
        }
        if (major != kSupportedSchemaMajor) {
            parsed.error = { RecipeErrorKind::BadResponse,
                QStringLiteral("unsupported schema_version %1").arg(doc.schemaVersion) };
            return parsed;
        }

        const QString operation = text(root, "operation");
        if (operation == QLatin1String("search")) {
            doc.operation = RecipeDocument::Operation::Search;
        } else if (operation == QLatin1String("detail")) {
            doc.operation = RecipeDocument::Operation::Detail;
        } else {
            fail("unknown operation");
        }
        parsed.operation = doc.operation;

        const YAML::Node query = child(root, "query");
        parsed.queryKeyword = text(query, "keyword");
        parsed.queryId = text(query, "id");
        parsed.queryPage = intOr(query, "page", 1);

        const YAML::Node status = child(root, "status");
        const QString code = text(status, "code");
        const QString message = text(status, "message");
        QStringList incomplete = textList(status, "incomplete");
        const QStringList warnings = textList(status, "warnings");
        if (code == QLatin1String("error")) {
            parsed.error = { errorKindOf(message, incomplete), message };
            return parsed;
        }
        if (code != QLatin1String("complete") && code != QLatin1String("partial")) {
            fail("unknown status code");
        }
        const bool partial = code == QLatin1String("partial");

        const YAML::Node source = child(root, "source");
        const QString sourceName = text(source, "name");
        const QString provider = text(source, "provider");

        const YAML::Node pagination = child(root, "pagination");
        const YAML::Node timing = child(root, "timing");
        doc.elapsedMs = optionalDouble(timing, "elapsed_ms").value_or(0);

        const YAML::Node results = child(root, "results");
        if (present(results) && !results.IsSequence()) {
            fail("results must be a list");
        }

        if (doc.operation == RecipeDocument::Operation::Search) {
            doc.search.keyword = parsed.queryKeyword;
            doc.search.page = intOr(pagination, "page", parsed.queryPage);
            doc.search.pageSize = intOr(pagination, "page_size", 0);
            doc.search.totalCandidates = intOr(pagination, "total_candidates_after_filter", 0);
            doc.search.partial = partial;
            doc.search.incomplete = incomplete;
            doc.search.warnings = warnings;
            eachMap(root, "results", [&](const YAML::Node &item) {
                doc.search.items.append(summaryOf(item, sourceName, provider));
            });
        } else {
            QList<WebRecipe> recipes;
            eachMap(root, "results", [&](const YAML::Node &item) {
                recipes.append(recipeOf(item, sourceName, provider));
            });
            if (recipes.size() != 1) {
                fail("detail needs exactly one result");
            }
            doc.recipe = recipes.first();
            doc.recipe->partial = partial;
            doc.recipe->incomplete = incomplete;
        }
        parsed.document = doc;
    } catch (const YAML::Exception &e) {
        parsed.error = { RecipeErrorKind::BadResponse, QStringLiteral("malformed YAML: ") + QString::fromUtf8(e.what()) };
    } catch (const std::exception &e) {
        parsed.error = { RecipeErrorKind::BadResponse, QString::fromUtf8(e.what()) };
    }
    return parsed;
}

}
