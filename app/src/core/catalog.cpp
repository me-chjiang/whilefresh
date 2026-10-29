#include "core/catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace whilefresh::core {

std::optional<IngredientCatalog> IngredientCatalog::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull()) {
        if (error)
            *error = QStringLiteral("vocabulary: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    const QJsonValue categories = root.value(QStringLiteral("categories"));

    IngredientCatalog catalog;
    if (categories.isArray()) {
        for (const QJsonValue &value : categories.toArray()) {
            const QJsonObject object = value.toObject();
            catalog.addCategory(object.value(QStringLiteral("code")).toString(),
                                object.value(QStringLiteral("name")).toString());
        }
    } else if (categories.isObject()) {
        const QJsonObject object = categories.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            catalog.addCategory(it.key(), it.value().toString());
    }

    const QJsonArray items = doc.isArray() ? doc.array() : root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        CatalogEntry entry;
        entry.name = object.value(QStringLiteral("name")).toString().trimmed();
        entry.category = object.value(QStringLiteral("category")).toString().trimmed();
        for (const QJsonValue &alias : object.value(QStringLiteral("aliases")).toArray())
            entry.aliases.append(alias.toString().trimmed());
        entry.parent = object.value(QStringLiteral("parent")).toString().trimmed();
        entry.pantryDefault = object.value(QStringLiteral("pantry_default")).toBool();
        entry.bulkDefault = object.value(QStringLiteral("bulk_default")).toBool();
        catalog.add(entry);
    }
    if (catalog.m_entries.isEmpty()) {
        if (error)
            *error = QStringLiteral("vocabulary: no items");
        return std::nullopt;
    }
    return catalog;
}

std::optional<IngredientCatalog> IngredientCatalog::fromFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("vocabulary: cannot read %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    return fromJson(file.readAll(), error);
}

void IngredientCatalog::addCategory(const QString &code, const QString &displayName)
{
    if (code.isEmpty())
        return;
    for (const auto &category : std::as_const(m_categories)) {
        if (category.first == code)
            return;
    }
    m_categories.append({ code, displayName.isEmpty() ? code : displayName });
}

bool IngredientCatalog::add(const CatalogEntry &entry)
{
    if (entry.name.isEmpty() || m_index.contains(entry.name))
        return false;
    m_index.insert(entry.name, m_entries.size());
    m_entries.append(entry);
    for (const QString &alias : entry.aliases) {
        if (!alias.isEmpty() && !m_aliasToName.contains(alias))
            m_aliasToName.insert(alias, entry.name);
    }
    return true;
}

const CatalogEntry *IngredientCatalog::entry(const QString &name) const
{
    const auto it = m_index.constFind(name);
    return it == m_index.constEnd() ? nullptr : &m_entries.at(*it);
}

QString IngredientCatalog::resolve(const QString &nameOrAlias) const
{
    const QString trimmed = nameOrAlias.trimmed();
    if (m_index.contains(trimmed))
        return trimmed;
    return m_aliasToName.value(trimmed);
}

QString IngredientCatalog::resolveFinest(const QString &title) const
{
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (const QString exact = resolve(trimmed); !exact.isEmpty())
        return exact;

    static const QRegularExpression separators(QStringLiteral(R"([\s\p{P}\p{S}]+|\d+(?:\.\d+)?[A-Za-z]*)"),
                                               QRegularExpression::UseUnicodePropertiesOption);
    const QStringList tokens = trimmed.split(separators, Qt::SkipEmptyParts);

    struct Candidate {
        QString name;
        qsizetype length = 0;
        qsizetype end = 0;
        qsizetype token = 0;
    };
    QList<Candidate> candidates;
    const auto consider = [&](const QString &surface, const QString &name) {
        for (qsizetype t = 0; t < tokens.size(); ++t) {
            const QString &token = tokens.at(t);
            const qsizetype at = token.lastIndexOf(surface);
            if (at < 0)
                continue;
            const qsizetype end = at + surface.size();
            if (surface.size() == 1 && end != token.size())
                continue;
            candidates.append({ name, surface.size(), end, t });
        }
    };
    for (const CatalogEntry &candidate : m_entries) {
        consider(candidate.name, candidate.name);
        for (const QString &alias : candidate.aliases) {
            // Only the owner of an alias may match through it; a standard name is never an alias.
            if (!alias.isEmpty() && !m_index.contains(alias) && m_aliasToName.value(alias) == candidate.name)
                consider(alias, candidate.name);
        }
    }

    QSet<QString> broader;
    for (const Candidate &candidate : std::as_const(candidates)) {
        for (const QString &ancestor : ancestors(candidate.name))
            broader.insert(ancestor);
    }
    QHash<qsizetype, Candidate> bestInToken;
    for (const Candidate &candidate : std::as_const(candidates)) {
        if (broader.contains(candidate.name))
            continue;
        const auto it = bestInToken.constFind(candidate.token);
        if (it == bestInToken.constEnd() || candidate.end > it->end
            || (candidate.end == it->end && candidate.length > it->length))
            bestInToken.insert(candidate.token, candidate);
    }
    std::optional<Candidate> best;
    for (qsizetype t = 0; t < tokens.size(); ++t) {
        const auto it = bestInToken.constFind(t);
        if (it != bestInToken.constEnd() && (!best || it->length > best->length))
            best = *it;
    }
    return best ? best->name : QString();
}

QString IngredientCatalog::categoryOf(const QString &name) const
{
    const CatalogEntry *found = entry(name);
    return found && !found->category.isEmpty() ? found->category : QStringLiteral("other");
}

QString IngredientCatalog::categoryDisplayName(const QString &code) const
{
    for (const auto &category : m_categories) {
        if (category.first == code)
            return category.second;
    }
    return code == QStringLiteral("other") ? QStringLiteral("其他") : code;
}

QStringList IngredientCatalog::categoryOrder() const
{
    QStringList codes;
    for (const auto &category : m_categories)
        codes.append(category.first);
    return codes;
}

int IngredientCatalog::categoryRank(const QString &code) const
{
    for (int i = 0; i < m_categories.size(); ++i) {
        if (m_categories.at(i).first == code)
            return i;
    }
    return m_categories.size();
}

QStringList IngredientCatalog::ancestors(const QString &name) const
{
    QStringList chain;
    QSet<QString> seen{ name };
    const CatalogEntry *current = entry(name);
    while (current && !current->parent.isEmpty() && !seen.contains(current->parent)) {
        chain.append(current->parent);
        seen.insert(current->parent);
        current = entry(current->parent);
    }
    return chain;
}

bool IngredientCatalog::satisfies(const QString &name, const QString &requirement) const
{
    return name == requirement || ancestors(name).contains(requirement);
}

QString IngredientCatalog::hierarchyText() const
{
    // Deeper descendants are listed with their topmost ancestor.
    const auto rootOf = [this](const QString &name) {
        QString root;
        for (const QString &ancestor : ancestors(name)) {
            if (contains(ancestor))
                root = ancestor;
        }
        return root;
    };
    QHash<QString, QStringList> children;
    QSet<QString> nested;
    for (const CatalogEntry &item : m_entries) {
        if (const QString root = rootOf(item.name); !root.isEmpty()) {
            children[root].append(item.name);
            nested.insert(item.name);
        }
    }

    struct Block {
        QString code;
        QStringList families;
        QStringList standalone;
    };
    QList<Block> blocks;
    const auto blockFor = [&](const QString &code) -> Block & {
        for (Block &block : blocks) {
            if (block.code == code)
                return block;
        }
        blocks.append({ code, {}, {} });
        return blocks.last();
    };
    for (const auto &category : std::as_const(m_categories))
        blockFor(category.first);
    for (const CatalogEntry &item : m_entries) {
        if (nested.contains(item.name))
            continue;
        const QStringList kids = children.value(item.name);
        if (kids.isEmpty())
            blockFor(item.category).standalone.append(item.name);
        else
            blockFor(item.category).families.append(
                QStringLiteral("%1 > %2").arg(item.name, kids.join(QStringLiteral("、"))));
    }

    QStringList lines;
    for (const Block &block : std::as_const(blocks)) {
        if (block.families.isEmpty() && block.standalone.isEmpty())
            continue;
        const QString label = block.code.isEmpty() ? QStringLiteral("其他") : categoryDisplayName(block.code);
        lines.append(QStringLiteral("【%1】").arg(label));
        lines.append(block.families);
        if (!block.standalone.isEmpty())
            lines.append(block.standalone.join(QStringLiteral("、")));
    }
    return lines.join(QLatin1Char('\n'));
}

}
