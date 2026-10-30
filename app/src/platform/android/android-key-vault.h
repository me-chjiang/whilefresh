#pragma once

#include "platform/key-vault.h"

namespace whilefresh::platform {

// JNI glue for app/android/src/io/github/mechjiang/whilefresh/KeyVault.java: AES-GCM with a key
// held in the AndroidKeyStore, ciphertext files in app-private storage.
class AndroidKeyVault final : public KeyVault
{
public:
    QString value(const QString &name) const override;
    bool setValue(const QString &name, const QString &value) override;
    bool remove(const QString &name) override;
};

}
