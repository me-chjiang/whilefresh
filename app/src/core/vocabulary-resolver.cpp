#include "core/vocabulary-resolver.h"

#include <QSet>

namespace whilefresh::core {

QString enumKey(NameSource source)
{
    switch (source) {
    case NameSource::Unresolved: return QStringLiteral("unresolved");
    case NameSource::UserMapping: return QStringLiteral("user-mapping");
    case NameSource::Recognized: return QStringLiteral("recognized");
    case NameSource::LocalMatch: return QStringLiteral("local-match");
    case NameSource::AiClassified: return QStringLiteral("ai-classified");
    case NameSource::UserEdited: return QStringLiteral("user-edited");
    }
    return {};
}

NameResolution VocabularyResolver::resolve(const QString &rawName, const QString &suggested) const
{
    if (m_mappings) {
        const QString mapped = m_mappings->mapped(rawName);
        if (m_catalog.contains(mapped))
            return { mapped, NameSource::UserMapping };
    }
    const QString finest = m_catalog.resolveFinest(rawName);
    const QString model = m_catalog.resolve(suggested);
    if (!model.isEmpty()) {
        if (!finest.isEmpty() && finest != model && m_catalog.satisfies(finest, model))
            return { finest, NameSource::Recognized };
        return { model, NameSource::Recognized };
    }
    if (!finest.isEmpty())
        return { finest, NameSource::LocalMatch };
    return {};
}

QStringList VocabularyResolver::unresolvedNames(const QStringList &rawNames, const QList<NameResolution> &resolutions)
{
    QStringList names;
    QSet<QString> seen;
    for (qsizetype i = 0; i < rawNames.size() && i < resolutions.size(); ++i) {
        const QString key = NameMappingStore::key(rawNames.at(i));
        if (resolutions.at(i).resolved() || key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        names.append(key);
    }
    return names;
}

}
