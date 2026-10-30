#pragma once

#include "ai/api-keys.h"
#include "platform/key-vault.h"

#include <QHash>

#include <memory>

namespace whilefresh::ui {

// The user's own keys, kept in the platform KeyVault (R-ADR-3). Reads are cached for the
// session; save() and clear() are the only writers, so the cache cannot go stale. An unset key is
// "not configured": nothing falls back to the built-in demo key.
class UserCredentialProvider final : public ai::CredentialProvider
{
public:
    explicit UserCredentialProvider(std::unique_ptr<platform::KeyVault> store) : m_store(std::move(store)) {}

    QString key(ai::Service service) const override;
    // False when the store could not write it; the cache is then left as it was.
    bool save(ai::Service service, const QString &value);
    bool clear(ai::Service service);

    static const char *secretName(ai::Service service);

private:
    std::unique_ptr<platform::KeyVault> m_store;
    mutable QHash<ai::Service, QString> m_cache;
};

}
