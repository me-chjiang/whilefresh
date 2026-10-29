#include "ui/app-info.h"

#include "core/app-version.h"
#include "platform/platform-info.h"
#include "ai/api-keys.h"

namespace whilefresh::ui {

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{
}

QString AppInfo::version() const
{
    return core::appVersion();
}

QString AppInfo::platform() const
{
    return platform::platformName();
}

bool AppInfo::recognitionConfigured() const
{
    return ai::BuiltinCredentialProvider().isConfigured(ai::Service::DeepSeek);
}

}
