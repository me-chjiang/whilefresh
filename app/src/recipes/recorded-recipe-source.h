#pragma once

#include "recipes/recipe-source.h"
#include "recipes/xiachufang-yaml-parser.h"

#include <QHash>
#include <QSet>

namespace whilefresh::recipes {

// Serves recorded source documents (YAML files of the contract) for tests and offline
// development. Each file answers the query it echoes: a search by keyword and page, a detail by
// id; error documents answer with their typed error. Anything not recorded fails with NotFound.
class RecordedRecipeSource : public RecipeSource
{
    Q_OBJECT

public:
    using RecipeSource::RecipeSource;

    // False when the file cannot be read or is not a document of the contract; a document whose
    // status is error is still registered.
    bool addFile(const QString &path);
    // Adds every *.yaml file in the directory; returns how many were registered.
    int addDirectory(const QString &dir);

    quint64 search(const QString &keyword, int page = 1) override;
    quint64 detail(const QString &id) override;
    void cancel(quint64 requestId) override { m_pending.remove(requestId); }
    bool isConfigured() const override { return true; }

private:
    quint64 enqueue(const QString &key);
    void answer(quint64 id, const QString &key);

    QHash<QString, ParsedRecipeDocument> m_documents;
    QSet<quint64> m_pending;
};

}
