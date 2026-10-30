#pragma once

#include "recipes/recipe-types.h"

#include <QObject>

namespace whilefresh::recipes {

// A web recipe source. Calls are asynchronous like ChatClient: search() / detail() return a
// request id and never complete synchronously; exactly one of the success signals or failed()
// is emitted later from the event loop with that id, unless the request was cancelled.
class RecipeSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual quint64 search(const QString &keyword, int page = 1) = 0;
    virtual quint64 detail(const QString &id) = 0;
    virtual void cancel(quint64 requestId) = 0;
    virtual bool isConfigured() const = 0;

signals:
    void searchSucceeded(quint64 requestId, const whilefresh::recipes::RecipeSearchResult &result);
    void detailSucceeded(quint64 requestId, const whilefresh::recipes::WebRecipe &recipe);
    void failed(quint64 requestId, const whilefresh::recipes::RecipeError &error);

protected:
    quint64 nextRequestId() { return ++m_lastRequestId; }

private:
    quint64 m_lastRequestId = 0;
};

}
