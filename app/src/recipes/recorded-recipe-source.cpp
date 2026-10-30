#include "recipes/recorded-recipe-source.h"

#include "recipes/recipes-log.h"

#include <QDir>
#include <QFile>
#include <QTimer>

namespace whilefresh::recipes {

namespace {

QString searchKey(const QString &keyword, int page)
{
    return QStringLiteral("search\n%1\n%2").arg(keyword).arg(page);
}

QString detailKey(const QString &id)
{
    return QStringLiteral("detail\n%1").arg(id);
}

}

bool RecordedRecipeSource::addFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const ParsedRecipeDocument parsed = parseXiachufangYaml(file.readAll());
    if (!parsed.operation) {
        return false;
    }
    const QString key = *parsed.operation == RecipeDocument::Operation::Search
        ? searchKey(parsed.queryKeyword, parsed.queryPage)
        : detailKey(parsed.queryId);
    m_documents.insert(key, parsed);
    return true;
}

int RecordedRecipeSource::addDirectory(const QString &dir)
{
    int added = 0;
    const QStringList names = QDir(dir).entryList({ QStringLiteral("*.yaml") }, QDir::Files, QDir::Name);
    for (const QString &name : names) {
        if (addFile(QDir(dir).filePath(name))) {
            ++added;
        }
    }
    return added;
}

quint64 RecordedRecipeSource::search(const QString &keyword, int page)
{
    return enqueue(searchKey(keyword, page));
}

quint64 RecordedRecipeSource::detail(const QString &id)
{
    return enqueue(detailKey(id));
}

quint64 RecordedRecipeSource::enqueue(const QString &key)
{
    const quint64 id = nextRequestId();
    m_pending.insert(id);
    QTimer::singleShot(0, this, [this, id, key] { answer(id, key); });
    return id;
}

void RecordedRecipeSource::answer(quint64 id, const QString &key)
{
    if (!m_pending.remove(id)) {
        return;
    }
    const auto it = m_documents.constFind(key);
    if (it == m_documents.constEnd()) {
        emit failed(id, { RecipeErrorKind::NotFound, QStringLiteral("no recording for this request") });
    } else if (!it->ok()) {
        emit failed(id, it->error);
    } else if (it->document->operation == RecipeDocument::Operation::Search) {
        emit searchSucceeded(id, it->document->search);
    } else {
        emit detailSucceeded(id, *it->document->recipe);
    }
}

}
