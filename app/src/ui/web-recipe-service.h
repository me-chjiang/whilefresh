#pragma once

#include "recipes/recipe-source.h"

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace whilefresh::ui {

// State behind the web recipe list and the recipe card (T17). One search and one detail request
// can be in flight; a new request of the same kind cancels the previous one, and replies for
// anything but the current request are ignored. Details are kept in memory for this run only:
// recipe bodies are never persisted (PRD). Errors are shown as short labels.
class WebRecipeService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(QString searchState READ searchState NOTIFY changed) // idle | busy | done | error
    Q_PROPERTY(QString searchError READ searchError NOTIFY changed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QString detailState READ detailState NOTIFY changed)
    Q_PROPERTY(QString detailError READ detailError NOTIFY changed)

public:
    explicit WebRecipeService(std::unique_ptr<recipes::RecipeSource> source, QObject *parent = nullptr);

    QVariantList results() const { return m_results; }
    QString searchState() const { return m_searchState; }
    QString searchError() const { return m_searchError; }
    QVariantMap detail() const { return m_detail; }
    QString detailState() const { return m_detailState; }
    QString detailError() const { return m_detailError; }

    Q_INVOKABLE void search(const QString &keyword);
    Q_INVOKABLE void requestDetail(const QString &id);
    // Stops whatever is running; the source still cleans up its browser session.
    Q_INVOKABLE void cancel();
    // Drops results, details and the cache: they belong to the mode and credentials that fetched them.
    void reset();

    static QString errorLabel(recipes::RecipeErrorKind kind);

signals:
    void changed();

private:
    void onSearch(quint64 id, const recipes::RecipeSearchResult &result);
    void onDetail(quint64 id, const recipes::WebRecipe &recipe);
    void onFailed(quint64 id, const recipes::RecipeError &error);
    static QVariantMap attributionMap(const recipes::RecipeAttribution &attribution);

    std::unique_ptr<recipes::RecipeSource> m_source;
    quint64 m_searchRequest = 0;
    quint64 m_detailRequest = 0;
    QVariantList m_results;
    QString m_searchState = QStringLiteral("idle");
    QString m_searchError;
    QVariantMap m_detail;
    QString m_detailState = QStringLiteral("idle");
    QString m_detailError;
    QHash<QString, QVariantMap> m_cache;
};

}
