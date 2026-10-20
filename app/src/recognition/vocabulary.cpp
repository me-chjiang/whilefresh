#include "recognition/vocabulary.h"

#include "recognition/recognition-log.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace whilefresh::recognition {

namespace {

constexpr int kLoggedNames = 10;

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

// QJsonObject sorts its keys, but the order of "categories" is the order of the prompt.
// This reads the member keys of a top-level object member in file order.
class MemberKeyScanner
{
public:
    explicit MemberKeyScanner(const QByteArray &json) : m_s(json) {}

    QStringList keysOf(const QString &member)
    {
        QStringList keys;
        skipSpace();
        if (!consume('{'))
            return {};
        while (true) {
            skipSpace();
            if (peek() == '}' || atEnd())
                return {};
            QString key;
            if (!readString(&key))
                return {};
            skipSpace();
            if (!consume(':'))
                return {};
            skipSpace();
            if (key == member && peek() == '{') {
                ++m_i;
                while (true) {
                    skipSpace();
                    if (consume('}'))
                        return keys;
                    QString inner;
                    if (!readString(&inner))
                        return {};
                    skipSpace();
                    if (!consume(':'))
                        return {};
                    skipSpace();
                    if (!skipValue())
                        return {};
                    keys.append(inner);
                    skipSpace();
                    consume(',');
                }
            }
            if (!skipValue())
                return {};
            skipSpace();
            consume(',');
        }
    }

private:
    bool atEnd() const { return m_i >= m_s.size(); }
    char peek() const { return atEnd() ? '\0' : m_s.at(m_i); }
    bool consume(char c)
    {
        if (peek() != c)
            return false;
        ++m_i;
        return true;
    }
    void skipSpace()
    {
        while (!atEnd() && (peek() == ' ' || peek() == '\n' || peek() == '\r' || peek() == '\t'))
            ++m_i;
    }
    bool readString(QString *out)
    {
        const qsizetype start = m_i;
        if (!consume('"'))
            return false;
        while (!atEnd() && peek() != '"') {
            m_i += peek() == '\\' ? 2 : 1;
        }
        if (!consume('"'))
            return false;
        const QJsonArray wrapper = QJsonDocument::fromJson("[" + m_s.mid(start, m_i - start) + "]").array();
        if (wrapper.isEmpty())
            return false;
        *out = wrapper.at(0).toString();
        return true;
    }
    bool skipValue()
    {
        if (peek() == '"') {
            QString ignored;
            return readString(&ignored);
        }
        if (peek() == '{' || peek() == '[') {
            int depth = 0;
            while (!atEnd()) {
                const char c = peek();
                if (c == '"') {
                    QString ignored;
                    if (!readString(&ignored))
                        return false;
                    continue;
                }
                ++m_i;
                if (c == '{' || c == '[')
                    ++depth;
                else if ((c == '}' || c == ']') && --depth == 0)
                    return true;
            }
            return false;
        }
        while (!atEnd() && peek() != ',' && peek() != '}' && peek() != ']' && peek() != ' ' && peek() != '\n'
               && peek() != '\r' && peek() != '\t')
            ++m_i;
        return true;
    }

    const QByteArray &m_s;
    qsizetype m_i = 0;
};

}

void Vocabulary::addCategory(const QString &code, const QString &displayName)
{
    const QString trimmed = code.trimmed();
    if (trimmed.isEmpty())
        return;
    for (const auto &category : std::as_const(m_categories)) {
        if (category.first == trimmed)
            return;
    }
    m_categories.append({ trimmed, displayName.trimmed().isEmpty() ? trimmed : displayName.trimmed() });
}

bool Vocabulary::addName(const VocabularyEntry &entry)
{
    const QString name = entry.name.trimmed();
    if (name.isEmpty() || m_byName.contains(name))
        return false;
    m_byName.insert(name, name);
    m_entries.append({ name, entry.category.trimmed(), {} });
    return true;
}

bool Vocabulary::addAlias(const QString &alias, const QString &standardName)
{
    const QString trimmed = alias.trimmed();
    if (trimmed.isEmpty() || trimmed == standardName)
        return true; // nothing to register, nothing wrong
    if (m_byName.contains(trimmed))
        return m_byName.value(trimmed) == standardName; // the same alias twice is harmless
    m_byName.insert(trimmed, standardName);
    for (VocabularyEntry &entry : m_entries) {
        if (entry.name == standardName) {
            entry.aliases.append(trimmed);
            break;
        }
    }
    return true;
}

bool Vocabulary::add(const VocabularyEntry &entry)
{
    if (!addName(entry))
        return false;
    const QString name = entry.name.trimmed();
    for (const QString &alias : entry.aliases)
        addAlias(alias, name);
    return true;
}

QString Vocabulary::resolve(const QString &nameOrAlias) const
{
    return m_byName.value(nameOrAlias.trimmed());
}

QString Vocabulary::categoryDisplayName(const QString &category) const
{
    for (const auto &declared : m_categories) {
        if (declared.first == category)
            return declared.second;
    }
    return category;
}

QList<std::pair<QString, QStringList>> Vocabulary::namesByCategory() const
{
    QList<std::pair<QString, QStringList>> groups; // keyed by category code, display name applied below
    const auto groupFor = [&](const QString &code) -> QStringList & {
        auto it = std::find_if(groups.begin(), groups.end(), [&](const auto &g) { return g.first == code; });
        if (it == groups.end()) {
            groups.append({ code, {} });
            return groups.last().second;
        }
        return it->second;
    };
    for (const auto &declared : m_categories)
        groupFor(declared.first);
    for (const VocabularyEntry &entry : m_entries)
        groupFor(entry.category).append(entry.name);

    QList<std::pair<QString, QStringList>> result;
    for (const auto &group : std::as_const(groups)) {
        if (!group.second.isEmpty())
            result.append({ categoryDisplayName(group.first), group.second });
    }
    return result;
}

std::optional<Vocabulary> Vocabulary::fromJson(const QByteArray &json, QString *error,
                                               VocabularyLoadReport *report)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull()) {
        setError(error, QStringLiteral("vocabulary is not valid JSON: %1").arg(parseError.errorString()));
        return std::nullopt;
    }

    QJsonArray items;
    Vocabulary vocabulary;
    if (doc.isArray()) {
        items = doc.array();
    } else if (doc.object().value(QStringLiteral("items")).isArray()) {
        items = doc.object().value(QStringLiteral("items")).toArray();

        const QJsonObject categories = doc.object().value(QStringLiteral("categories")).toObject();
        QStringList order = MemberKeyScanner(json).keysOf(QStringLiteral("categories"));
        order.removeIf([&](const QString &code) { return !categories.contains(code); });
        for (const QString &code : categories.keys()) { // anything the scan missed, sorted
            if (!order.contains(code))
                order.append(code);
        }
        for (const QString &code : std::as_const(order))
            vocabulary.addCategory(code, categories.value(code).toString());
    } else {
        setError(error, QStringLiteral("vocabulary has no \"items\" array"));
        return std::nullopt;
    }

    // Two passes so that a standard name always wins over an alias, whatever the file order.
    VocabularyLoadReport local;
    QList<VocabularyEntry> accepted;
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        VocabularyEntry entry;
        entry.name = object.value(QStringLiteral("name")).toString();
        entry.category = object.value(QStringLiteral("category")).toString();
        for (const QJsonValue &alias : object.value(QStringLiteral("aliases")).toArray())
            entry.aliases.append(alias.toString());
        if (vocabulary.addName(entry)) {
            entry.name = entry.name.trimmed();
            accepted.append(entry);
        } else {
            ++local.ignoredEntries;
            local.ignoredNames.append(entry.name.trimmed());
        }
    }
    for (const VocabularyEntry &entry : std::as_const(accepted)) {
        for (const QString &alias : entry.aliases) {
            if (!vocabulary.addAlias(alias, entry.name)) {
                ++local.ignoredAliases;
                local.ignoredNames.append(alias.trimmed());
            }
        }
    }

    if (local.ignoredEntries || local.ignoredAliases) {
        qCWarning(lcRecognition, "vocabulary: ignored %d entries and %d aliases: %s", local.ignoredEntries,
                  local.ignoredAliases,
                  qUtf8Printable(local.ignoredNames.mid(0, kLoggedNames).join(QStringLiteral("、"))));
    }
    if (report)
        *report = local;

    if (vocabulary.isEmpty()) {
        setError(error, QStringLiteral("vocabulary has no usable entries"));
        return std::nullopt;
    }
    return vocabulary;
}

std::optional<Vocabulary> Vocabulary::fromFile(const QString &path, QString *error,
                                               VocabularyLoadReport *report)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot open vocabulary %1: %2").arg(path, file.errorString()));
        return std::nullopt;
    }
    return fromJson(file.readAll(), error, report);
}

}
