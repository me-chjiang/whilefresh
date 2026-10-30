#pragma once

#include <QMap>
#include <QString>

namespace whilefresh::ai {

enum class Service { DeepSeek, Lexmount };

// Where API keys come from. App-level code holds the provider and can swap it (built-in key for
// demo mode, the user's stored key otherwise); clients ask for the key on every request.
class CredentialProvider
{
public:
    virtual ~CredentialProvider() = default;

    // Empty when the service is not configured.
    virtual QString key(Service service) const = 0;
    bool isConfigured(Service service) const { return !key(service).isEmpty(); }
};

// The DeepSeek key compiled in by the build script (ADR-5); no Lexmount key.
class BuiltinCredentialProvider : public CredentialProvider
{
public:
    QString key(Service service) const override;
};

// Fixed keys, for tests and tools.
class StaticCredentialProvider : public CredentialProvider
{
public:
    explicit StaticCredentialProvider(const QString &deepSeekKey = {}) { m_keys.insert(Service::DeepSeek, deepSeekKey); }

    QString key(Service service) const override { return m_keys.value(service); }
    void setKey(Service service, const QString &key) { m_keys.insert(service, key); }

private:
    QMap<Service, QString> m_keys;
};

}
