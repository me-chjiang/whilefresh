#include "platform/platform-info.h"

namespace whilefresh::platform {

QString platformName()
{
#if defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("desktop");
#endif
}

}
