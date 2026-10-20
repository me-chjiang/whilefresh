#include "core/app-version.h"

namespace whilefresh::core {

QString appVersion()
{
    return QStringLiteral(WHILEFRESH_VERSION);
}

}
