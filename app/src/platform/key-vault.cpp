#include "platform/key-vault.h"

#if defined(Q_OS_ANDROID)
#include "platform/android/android-key-vault.h"
#else
#include "platform/desktop/file-key-vault.h"

#include <QStandardPaths>
#endif

namespace whilefresh::platform {

std::unique_ptr<KeyVault> KeyVault::create()
{
#if defined(Q_OS_ANDROID)
    return std::make_unique<AndroidKeyVault>();
#else
    return std::make_unique<FileKeyVault>(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
#endif
}

}
