#include "recipes/contract-document.h"

#include <yaml-cpp/yaml.h>

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <cmath>

namespace whilefresh::recipes {

namespace {

const QUrl kSite(QStringLiteral("https://www.xiachufang.com"));

QString trimmed(const QJsonValue &value)
{
    return value.toString().trimmed();
}

QString absoluteHttpUrl(const QJsonValue &value)
{
    const QString raw = trimmed(value);
    if (raw.isEmpty()) {
        return {};
    }
    const QUrl url = kSite.resolved(QUrl(raw));
    return (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) ? url.toString() : QString();
}

std::string utf8(const QString &text)
{
    return text.toStdString();
}

void emitText(YAML::Emitter &out, const QString &value)
{
    if (value.isEmpty()) {
        out << YAML::Null;
    } else {
        out << utf8(value);
    }
}

void emitId(YAML::Emitter &out, const QString &id)
{
    out << YAML::SingleQuoted << utf8(id);
}

void emitStringList(YAML::Emitter &out, const QStringList &list)
{
    out << YAML::BeginSeq;
    for (const QString &item : list) {
        out << utf8(item);
    }
    out << YAML::EndSeq;
}

struct Row {
    QString id;
    QString name;
    QString imageUrl;
};

void emitRecipeHead(YAML::Emitter &out, const Row &row)
{
    out << YAML::Key << "id" << YAML::Value;
    emitId(out, row.id);
    out << YAML::Key << "name" << YAML::Value << utf8(row.name);
    out << YAML::Key << "description" << YAML::Value << YAML::Null;
    out << YAML::Key << "author" << YAML::Value << YAML::Null;
    out << YAML::Key << "source_url" << YAML::Value << utf8(kSite.resolved(QUrl(QStringLiteral("/recipe/%1/").arg(row.id))).toString());
    out << YAML::Key << "image_url" << YAML::Value;
    emitText(out, row.imageUrl);
}

void emitRecipeTail(YAML::Emitter &out)
{
    out << YAML::Key << "notes" << YAML::Value << YAML::Null;
}

}

QByteArray buildContractDocument(const ContractQuery &query, const QJsonObject &scrape, double elapsedMs, QString *error)
{
    const bool search = query.operation == RecipeDocument::Operation::Search;
    QStringList incomplete;
    QStringList warnings;

    YAML::Emitter out;
    out.SetIndent(2);

    // Build the results first so status can describe them; the emitter writes in one pass
    // afterwards from these plain values.
    QList<Row> rows;
    QJsonObject content;
    int candidates = 0;
    if (search) {
        static const QRegularExpression idPattern(QStringLiteral("/recipe/(\\d{1,20})/?"));
        QSet<QString> seen;
        const QJsonArray found = scrape.value(QLatin1String("Recipe")).toArray();
        candidates = int(found.size());
        int skipped = 0;
        for (const QJsonValue &value : found) {
            const QJsonObject item = value.toObject();
            const QRegularExpressionMatch match = idPattern.match(trimmed(item.value(QLatin1String("url"))));
            const QString name = trimmed(item.value(QLatin1String("name")));
            if (!match.hasMatch()) {
                ++skipped;
                continue;
            }
            if (name.isEmpty()) {
                ++skipped;
                continue;
            }
            const QString id = match.captured(1);
            if (seen.contains(id)) {
                continue;
            }
            seen.insert(id);
            rows.append({ id, name, absoluteHttpUrl(item.value(QLatin1String("cover"))) });
        }
        if (skipped > 0) {
            warnings.append(QStringLiteral("skipped_unusable_rows"));
        }
        if (rows.size() > query.pageSize) {
            rows = rows.mid(0, query.pageSize);
            incomplete.append(QStringLiteral("upstream_page_truncated"));
        }
        incomplete.prepend(QStringLiteral("full_details"));
    } else {
        const QJsonArray found = scrape.value(QLatin1String("Content")).toArray();
        if (found.size() != 1) {
            if (error) {
                *error = QStringLiteral("response requires one Content recipe");
            }
            return {};
        }
        content = found.first().toObject();
        if (trimmed(content.value(QLatin1String("name"))).isEmpty()) {
            if (error) {
                *error = QStringLiteral("source recipe must have a name");
            }
            return {};
        }
        candidates = 1;
    }

    struct Ingredient {
        QString name;
        QString amount;
    };
    struct Step {
        QString text;
        QString imageUrl;
    };
    QList<Ingredient> ingredients;
    QList<Step> steps;
    QString tip;
    std::optional<double> rating;
    if (!search) {
        for (const QJsonValue &value : content.value(QLatin1String("materials")).toArray()) {
            const QString name = trimmed(value.toObject().value(QLatin1String("name")));
            if (!name.isEmpty()) {
                ingredients.append({ name, trimmed(value.toObject().value(QLatin1String("unit"))) });
            }
        }
        for (const QJsonValue &value : content.value(QLatin1String("steps")).toArray()) {
            const QJsonObject step = value.toObject();
            const QString text = step.value(QLatin1String("desc")).toString();
            const QString image = absoluteHttpUrl(step.value(QLatin1String("img")));
            if (!text.trimmed().isEmpty() || !image.isEmpty()) {
                steps.append({ text, image });
            }
        }
        tip = trimmed(content.value(QLatin1String("tip")));
        const QJsonValue grade = content.value(QLatin1String("grade"));
        bool ok = grade.isDouble();
        double number = grade.toDouble();
        if (grade.isString()) {
            number = grade.toString().toDouble(&ok);
        }
        if (ok && std::isfinite(number) && number >= 0 && number <= 10) {
            rating = number;
        }
        if (ingredients.isEmpty()) {
            incomplete.append(QStringLiteral("ingredients"));
        }
        if (steps.isEmpty()) {
            incomplete.append(QStringLiteral("steps"));
        }
        rows.append({ query.id, trimmed(content.value(QLatin1String("name"))), absoluteHttpUrl(content.value(QLatin1String("cover"))) });
    }

    out << YAML::BeginMap;
    out << YAML::Key << "schema_version" << YAML::Value << YAML::SingleQuoted << "1.0";
    out << YAML::Key << "query" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "source" << YAML::Value << "xiachufang";
    out << YAML::Key << "operation" << YAML::Value << (search ? "search" : "detail");
    out << YAML::Key << "keyword" << YAML::Value;
    emitText(out, search ? query.keyword : QString());
    out << YAML::Key << "id" << YAML::Value;
    if (search) {
        out << YAML::Null;
    } else {
        emitId(out, query.id);
    }
    out << YAML::Key << "page" << YAML::Value << query.page;
    out << YAML::Key << "page_size" << YAML::Value << query.pageSize;
    out << YAML::Key << "provider" << YAML::Value << utf8(query.provider);
    out << YAML::Key << "search_type" << YAML::Value << "browser";
    out << YAML::EndMap;
    out << YAML::Key << "operation" << YAML::Value << (search ? "search" : "detail");
    out << YAML::Key << "source" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << "xiachufang";
    out << YAML::Key << "capabilities" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "search" << YAML::Value << true << YAML::Key << "detail" << YAML::Value << true
        << YAML::Key << "recommend" << YAML::Value << false;
    out << YAML::EndMap;
    out << YAML::Key << "provider" << YAML::Value << utf8(query.provider);
    out << YAML::EndMap;
    out << YAML::Key << "status" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "code" << YAML::Value << (incomplete.isEmpty() ? "complete" : "partial");
    out << YAML::Key << "message" << YAML::Value << "source data returned";
    out << YAML::Key << "incomplete" << YAML::Value;
    emitStringList(out, incomplete);
    out << YAML::Key << "warnings" << YAML::Value;
    emitStringList(out, warnings);
    out << YAML::EndMap;

    out << YAML::Key << "results" << YAML::Value << YAML::BeginSeq;
    for (const Row &row : rows) {
        out << YAML::BeginMap;
        emitRecipeHead(out, row);
        out << YAML::Key << "category" << YAML::Value << YAML::Null;
        out << YAML::Key << "region" << YAML::Value << YAML::Null;
        out << YAML::Key << "rating" << YAML::Value;
        if (rating) {
            out << *rating;
        } else {
            out << YAML::Null;
        }
        out << YAML::Key << "rating_count" << YAML::Value << YAML::Null;
        out << YAML::Key << "made_count" << YAML::Value << YAML::Null;
        out << YAML::Key << "steps" << YAML::Value << YAML::BeginSeq;
        int order = 1;
        for (const Step &step : steps) {
            out << YAML::BeginMap << YAML::Key << "order" << YAML::Value << order++ << YAML::Key << "text" << YAML::Value
                << utf8(step.text) << YAML::Key << "image_url" << YAML::Value;
            emitText(out, step.imageUrl);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::Key << "ingredients" << YAML::Value << YAML::BeginSeq;
        for (const Ingredient &ingredient : ingredients) {
            out << YAML::BeginMap << YAML::Key << "raw" << YAML::Value
                << utf8(ingredient.amount.isEmpty() ? ingredient.name : ingredient.name + QLatin1Char(' ') + ingredient.amount)
                << YAML::Key << "name" << YAML::Value << utf8(ingredient.name) << YAML::Key << "amount" << YAML::Value;
            emitText(out, ingredient.amount);
            out << YAML::Key << "category" << YAML::Value << "ingredient" << YAML::Key << "required" << YAML::Value << "unknown"
                << YAML::Key << "evidence" << YAML::Value << "source does not specify necessity" << YAML::Key << "notes"
                << YAML::Value << YAML::Null << YAML::Key << "substitutes" << YAML::Value << YAML::Flow << YAML::BeginSeq
                << YAML::EndSeq << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::Key << "tips" << YAML::Value;
        emitStringList(out, tip.isEmpty() ? QStringList() : QStringList { tip });
        emitRecipeTail(out);
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "filter" << YAML::Value << YAML::BeginMap << YAML::Key << "applied" << YAML::Value << YAML::BeginSeq;
    if (search) {
        out << YAML::BeginMap << YAML::Key << "keyword" << YAML::Value << utf8(query.keyword) << YAML::EndMap;
    }
    out << YAML::EndSeq << YAML::Key << "candidate_scope" << YAML::Value << "one upstream page" << YAML::EndMap;
    out << YAML::Key << "sort" << YAML::Value << YAML::BeginMap << YAML::Key << "requested" << YAML::Value << YAML::Null
        << YAML::Key << "applied" << YAML::Value << YAML::Null << YAML::EndMap;
    out << YAML::Key << "pagination" << YAML::Value << YAML::BeginMap << YAML::Key << "page" << YAML::Value << query.page
        << YAML::Key << "page_size" << YAML::Value << query.pageSize << YAML::Key << "returned" << YAML::Value << int(rows.size())
        << YAML::Key << "total_candidates_after_filter" << YAML::Value << candidates << YAML::EndMap;
    out << YAML::Key << "timing" << YAML::Value << YAML::BeginMap << YAML::Key << "elapsed_ms" << YAML::Value
        << std::round(elapsedMs * 100) / 100 << YAML::EndMap;
    out << YAML::EndMap;

    if (!out.good()) {
        if (error) {
            *error = QStringLiteral("could not write the contract document");
        }
        return {};
    }
    return QByteArray(out.c_str()) + '\n';
}

}
