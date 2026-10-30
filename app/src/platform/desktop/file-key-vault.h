#pragma once

#include "platform/key-vault.h"

#include <QJsonObject>

namespace whilefresh::platform {

// Development-only KeyVault: a JSON file `secrets.json` in `directory`, readable by the owner
// only. It is NOT encrypted; a desktop build is not meant for distribution (the shipped app is
// Android, which encrypts with the AndroidKeyStore).
class FileKeyVault final : public KeyVault
{
public:
    explicit FileKeyVault(const QString &directory);

    QString value(const QString &name) const override;
    bool setValue(const QString &name, const QString &value) override;
    bool remove(const QString &name) override;

    QString filePath() const { return m_path; }

private:
    bool write(const QJsonObject &secrets) const;
    QJsonObject read() const;

    QString m_directory;
    QString m_path;
};

}
