#pragma once

#include "recipes/xiachufang-yaml-parser.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace whilefresh::recipes {

struct ContractQuery {
    RecipeDocument::Operation operation = RecipeDocument::Operation::Search;
    QString keyword;
    QString id;
    int page = 1;
    int pageSize = 20;
    QString provider = QStringLiteral("lexmount");
};

// Turns what the page scrape returned ({Recipe: [...]} or {Content: [...]}, the shapes of the
// submodule's xiachufang-api route) into a document of the same YAML contract the submodule's
// CLI prints, so live and recorded results take one path through parseXiachufangYaml. Nameless
// search rows are skipped with a warning. Empty result and *error set when the scrape cannot be
// normalized.
QByteArray buildContractDocument(const ContractQuery &query, const QJsonObject &scrape, double elapsedMs, QString *error);

}
