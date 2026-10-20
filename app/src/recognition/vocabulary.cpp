#include "recognition/vocabulary.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace whilefresh::recognition {

namespace {

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

}

bool Vocabulary::add(const VocabularyEntry &entry)
{
    const QString name = entry.name.trimmed();
    if (name.isEmpty() || m_byName.contains(name))
        return false;

    VocabularyEntry stored{ name, entry.category.trimmed(), {} };
    m_byName.insert(name, name);
    for (const QString &alias : entry.aliases) {
        const QString trimmed = alias.trimmed();
        if (trimmed.isEmpty() || m_byName.contains(trimmed))
            continue;
        m_byName.insert(trimmed, name);
        stored.aliases.append(trimmed);
    }
    m_entries.append(stored);
    return true;
}

QString Vocabulary::resolve(const QString &nameOrAlias) const
{
    return m_byName.value(nameOrAlias.trimmed());
}

QList<std::pair<QString, QStringList>> Vocabulary::namesByCategory() const
{
    QList<std::pair<QString, QStringList>> groups;
    for (const VocabularyEntry &entry : m_entries) {
        auto it = std::find_if(groups.begin(), groups.end(),
                               [&](const auto &group) { return group.first == entry.category; });
        if (it == groups.end())
            groups.append({ entry.category, { entry.name } });
        else
            it->second.append(entry.name);
    }
    return groups;
}

std::optional<Vocabulary> Vocabulary::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull()) {
        setError(error, QStringLiteral("vocabulary is not valid JSON: %1").arg(parseError.errorString()));
        return std::nullopt;
    }

    QJsonArray items;
    if (doc.isArray())
        items = doc.array();
    else if (doc.object().value(QStringLiteral("items")).isArray())
        items = doc.object().value(QStringLiteral("items")).toArray();
    else {
        setError(error, QStringLiteral("vocabulary has no \"items\" array"));
        return std::nullopt;
    }

    Vocabulary vocabulary;
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        VocabularyEntry entry;
        entry.name = object.value(QStringLiteral("name")).toString();
        entry.category = object.value(QStringLiteral("category")).toString();
        for (const QJsonValue &alias : object.value(QStringLiteral("aliases")).toArray())
            entry.aliases.append(alias.toString());
        vocabulary.add(entry);
    }
    if (vocabulary.isEmpty()) {
        setError(error, QStringLiteral("vocabulary has no usable entries"));
        return std::nullopt;
    }
    return vocabulary;
}

std::optional<Vocabulary> Vocabulary::fromFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot open vocabulary %1: %2").arg(path, file.errorString()));
        return std::nullopt;
    }
    return fromJson(file.readAll(), error);
}

}
