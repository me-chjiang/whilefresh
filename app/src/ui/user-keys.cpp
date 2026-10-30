#include "ui/user-keys.h"

namespace whilefresh::ui {

const char *UserCredentialProvider::secretName(ai::Service service)
{
    switch (service) {
    case ai::Service::DeepSeek:
        return platform::kSecretDeepSeekKey;
    case ai::Service::Lexmount:
        return platform::kSecretLexmountKey;
    case ai::Service::LexmountProjectId:
        return platform::kSecretLexmountProjectId;
    }
    return "";
}

QString UserCredentialProvider::key(ai::Service service) const
{
    const auto it = m_cache.constFind(service);
    if (it != m_cache.constEnd())
        return *it;
    const QString value = m_store->value(QString::fromLatin1(secretName(service)));
    m_cache.insert(service, value);
    return value;
}

bool UserCredentialProvider::save(ai::Service service, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return clear(service);
    if (!m_store->setValue(QString::fromLatin1(secretName(service)), trimmed))
        return false;
    m_cache.insert(service, trimmed);
    return true;
}

bool UserCredentialProvider::clear(ai::Service service)
{
    if (!m_store->remove(QString::fromLatin1(secretName(service))))
        return false;
    m_cache.insert(service, QString());
    return true;
}

}
