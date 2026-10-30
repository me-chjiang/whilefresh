#pragma once

#include <QString>

#include <memory>

namespace whilefresh::platform {

// Names of the secrets the app keeps (lowercase letters, digits and '-' only).
inline constexpr char kSecretDeepSeekKey[] = "deepseek-key";
inline constexpr char kSecretLexmountKey[] = "lexmount-key";
inline constexpr char kSecretLexmountProjectId[] = "lexmount-project-id";

// Where the user's API keys are kept on this device (R-ADR-3). Android encrypts with a key in the
// AndroidKeyStore; the desktop build keeps a plain owner-only file for development. Values are
// never logged and never go into the SQLite database. Calls are synchronous and cheap.
class KeyVault
{
public:
    virtual ~KeyVault() = default;

    // Empty when absent or unreadable.
    virtual QString value(const QString &name) const = 0;
    // An empty value removes the secret.
    virtual bool setValue(const QString &name, const QString &value) = 0;
    virtual bool remove(const QString &name) = 0;

    // The implementation for this build.
    static std::unique_ptr<KeyVault> create();
};

}
