#include "ui/web-recipe-service.h"

namespace whilefresh::ui {

using namespace recipes;

WebRecipeService::WebRecipeService(std::unique_ptr<RecipeSource> source, QObject *parent)
    : QObject(parent), m_source(std::move(source))
{
    connect(m_source.get(), &RecipeSource::searchSucceeded, this, &WebRecipeService::onSearch);
    connect(m_source.get(), &RecipeSource::detailSucceeded, this, &WebRecipeService::onDetail);
    connect(m_source.get(), &RecipeSource::failed, this, &WebRecipeService::onFailed);
}

QString WebRecipeService::errorLabel(RecipeErrorKind kind)
{
    switch (kind) {
    case RecipeErrorKind::NotConfigured: return QStringLiteral("未配置 Lexmount");
    case RecipeErrorKind::AuthFailed: return QStringLiteral("Lexmount 密钥无效");
    case RecipeErrorKind::Blocked: return QStringLiteral("下厨房拦截");
    case RecipeErrorKind::NetworkError: return QStringLiteral("网络错误");
    case RecipeErrorKind::Timeout: return QStringLiteral("超时");
    case RecipeErrorKind::NotFound: return QStringLiteral("没有找到");
    case RecipeErrorKind::InvalidQuery: return QStringLiteral("输入有误");
    default: return QStringLiteral("菜谱源不可用");
    }
}

void WebRecipeService::search(const QString &keyword)
{
    const QString trimmed = keyword.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (m_searchState == QLatin1String("busy")) {
        m_source->cancel(m_searchRequest);
    }
    m_results.clear();
    m_searchError.clear();
    m_searchState = QStringLiteral("busy");
    m_searchRequest = m_source->search(trimmed);
    emit changed();
}

void WebRecipeService::requestDetail(const QString &id)
{
    if (m_detailState == QLatin1String("busy")) {
        m_source->cancel(m_detailRequest);
    }
    m_detailError.clear();
    const auto cached = m_cache.constFind(id);
    if (cached != m_cache.constEnd()) {
        m_detail = *cached;
        m_detailState = QStringLiteral("done");
        emit changed();
        return;
    }
    m_detail.clear();
    m_detailState = QStringLiteral("busy");
    m_detailRequest = m_source->detail(id);
    emit changed();
}

void WebRecipeService::cancel()
{
    if (m_searchState == QLatin1String("busy")) {
        m_source->cancel(m_searchRequest);
        m_searchState = QStringLiteral("idle");
    }
    if (m_detailState == QLatin1String("busy")) {
        m_source->cancel(m_detailRequest);
        m_detailState = QStringLiteral("idle");
    }
    emit changed();
}

void WebRecipeService::reset()
{
    cancel();
    m_results.clear();
    m_searchError.clear();
    m_searchState = QStringLiteral("idle");
    m_detail.clear();
    m_detailError.clear();
    m_detailState = QStringLiteral("idle");
    m_cache.clear();
    emit changed();
}

QVariantMap WebRecipeService::attributionMap(const RecipeAttribution &attribution)
{
    return { { QStringLiteral("source"), attribution.sourceName == QLatin1String("xiachufang") ? QStringLiteral("下厨房") : attribution.sourceName },
        { QStringLiteral("url"), attribution.sourceUrl }, { QStringLiteral("author"), attribution.author } };
}

void WebRecipeService::onSearch(quint64 id, const RecipeSearchResult &result)
{
    if (id != m_searchRequest || m_searchState != QLatin1String("busy")) {
        return;
    }
    m_results.clear();
    for (const RecipeSummary &item : result.items) {
        m_results.append(QVariantMap { { QStringLiteral("id"), item.id }, { QStringLiteral("name"), item.name },
            { QStringLiteral("imageUrl"), item.imageUrl }, { QStringLiteral("sourceUrl"), item.attribution.sourceUrl } });
    }
    m_searchState = QStringLiteral("done");
    emit changed();
}

void WebRecipeService::onDetail(quint64 id, const WebRecipe &recipe)
{
    if (id != m_detailRequest || m_detailState != QLatin1String("busy")) {
        return;
    }
    QVariantList ingredients;
    for (const RecipeIngredient &ingredient : recipe.ingredients) {
        ingredients.append(QVariantMap { { QStringLiteral("raw"), ingredient.raw } });
    }
    QVariantList steps;
    for (const RecipeStep &step : recipe.steps) {
        steps.append(QVariantMap { { QStringLiteral("order"), step.order }, { QStringLiteral("text"), step.text },
            { QStringLiteral("imageUrl"), step.imageUrl } });
    }
    m_detail = { { QStringLiteral("id"), recipe.id }, { QStringLiteral("name"), recipe.name },
        { QStringLiteral("imageUrl"), recipe.imageUrl }, { QStringLiteral("rating"), recipe.rating.value_or(0) },
        { QStringLiteral("ingredients"), ingredients }, { QStringLiteral("steps"), steps },
        { QStringLiteral("tips"), recipe.tips }, { QStringLiteral("partial"), recipe.partial },
        { QStringLiteral("attribution"), attributionMap(recipe.attribution) } };
    m_cache.insert(recipe.id, m_detail);
    m_detailState = QStringLiteral("done");
    emit changed();
}

void WebRecipeService::onFailed(quint64 id, const RecipeError &error)
{
    if (id == m_searchRequest && m_searchState == QLatin1String("busy")) {
        m_searchState = QStringLiteral("error");
        m_searchError = errorLabel(error.kind);
    } else if (id == m_detailRequest && m_detailState == QLatin1String("busy")) {
        m_detailState = QStringLiteral("error");
        m_detailError = errorLabel(error.kind);
    } else {
        return;
    }
    emit changed();
}

}
