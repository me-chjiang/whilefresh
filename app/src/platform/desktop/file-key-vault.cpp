#include "platform/desktop/file-key-vault.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace whilefresh::platform {

namespace {

bool validName(const QString &name)
{
    if (name.isEmpty())
        return false;
    for (const QChar c : name) {
        if (!((c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
              || c == QLatin1Char('-')))
            return false;
    }
    return true;
}

}

FileKeyVault::FileKeyVault(const QString &directory)
    : m_directory(directory), m_path(QDir(directory).filePath(QStringLiteral("secrets.json")))
{
}

QJsonObject FileKeyVault::read() const
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool FileKeyVault::write(const QJsonObject &secrets) const
{
    if (!QDir().mkpath(m_directory))
        return false;
    // With the directory closed to others, the moment before the file's own permissions are set
    // exposes nothing.
    QFile::setPermissions(m_directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(secrets).toJson(QJsonDocument::Compact));
    if (!file.commit())
        return false;
    return QFile::setPermissions(m_path, QFile::ReadOwner | QFile::WriteOwner);
}

QString FileKeyVault::value(const QString &name) const
{
    return validName(name) ? read().value(name).toString() : QString();
}

bool FileKeyVault::setValue(const QString &name, const QString &value)
{
    if (!validName(name))
        return false;
    if (value.isEmpty())
        return remove(name);
    QJsonObject secrets = read();
    secrets.insert(name, value);
    return write(secrets);
}

bool FileKeyVault::remove(const QString &name)
{
    if (!validName(name))
        return false;
    QJsonObject secrets = read();
    if (!secrets.contains(name))
        return true;
    secrets.remove(name);
    return write(secrets);
}

}
