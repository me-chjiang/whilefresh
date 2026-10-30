#include "ai/api-keys.h"

#include "generated/api-config.h"

namespace whilefresh::ai {

QString BuiltinCredentialProvider::key(Service service) const
{
    if (service == Service::DeepSeek)
        return QString::fromLatin1(generated::kDeepSeekApiKey);
    return {};
}

}
