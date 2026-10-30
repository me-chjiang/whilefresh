#include "platform/android/android-key-vault.h"

#include <QCoreApplication>
#include <QJniObject>

namespace whilefresh::platform {

namespace {

constexpr char vaultClass[] = "io/github/mechjiang/whilefresh/KeyVault";

QJniObject context()
{
    return QNativeInterface::QAndroidApplication::context();
}

}

QString AndroidKeyVault::value(const QString &name) const
{
    const QJniObject javaName = QJniObject::fromString(name);
    const QJniObject result = QJniObject::callStaticObjectMethod(
        vaultClass, "get", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", context().object(),
        javaName.object<jstring>());
    return result.isValid() ? result.toString() : QString();
}

bool AndroidKeyVault::setValue(const QString &name, const QString &value)
{
    if (value.isEmpty())
        return remove(name);
    const QJniObject javaName = QJniObject::fromString(name);
    const QJniObject javaValue = QJniObject::fromString(value);
    return QJniObject::callStaticMethod<jboolean>(vaultClass, "put",
                                                  "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z",
                                                  context().object(), javaName.object<jstring>(),
                                                  javaValue.object<jstring>());
}

bool AndroidKeyVault::remove(const QString &name)
{
    const QJniObject javaName = QJniObject::fromString(name);
    return QJniObject::callStaticMethod<jboolean>(vaultClass, "remove", "(Landroid/content/Context;Ljava/lang/String;)Z",
                                                  context().object(), javaName.object<jstring>());
}

}
