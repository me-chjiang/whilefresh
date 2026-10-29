#include "data/name-mappings.h"

#include "data/database.h"

#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace whilefresh::data {

Q_LOGGING_CATEGORY(lcNameMappings, "whilefresh.data.names")

QString SqliteNameMappingStore::mapped(const QString &rawName) const
{
    const QString key = NameMappingStore::key(rawName);
    if (key.isEmpty())
        return {};
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT canonical FROM name_mappings WHERE raw_name = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        qCWarning(lcNameMappings, "lookup failed: %s", qUtf8Printable(query.lastError().text()));
        return {};
    }
    return query.next() ? query.value(0).toString() : QString();
}

bool SqliteNameMappingStore::remember(const QString &rawName, const QString &canonical)
{
    const QString key = NameMappingStore::key(rawName);
    if (key.isEmpty() || canonical.isEmpty())
        return false;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO name_mappings (raw_name, canonical, confirmed_at) VALUES (?, ?, ?) "
                                 "ON CONFLICT(raw_name) DO UPDATE SET canonical = excluded.canonical, "
                                 "confirmed_at = excluded.confirmed_at"));
    query.addBindValue(key);
    query.addBindValue(canonical);
    query.addBindValue(m_clock.now().toUTC().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        qCWarning(lcNameMappings, "remember failed: %s", qUtf8Printable(query.lastError().text()));
        return false;
    }
    return true;
}

}
