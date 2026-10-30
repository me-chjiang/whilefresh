#include "ai/api-keys.h"

#include "generated/api-config.h"

namespace whilefresh::ai {

QString BuiltinCredentialProvider::key(Service service) const
{
    switch (service) {
    case Service::DeepSeek:
        return QString::fromLatin1(generated::kDeepSeekApiKey);
    case Service::Lexmount:
        return QString::fromLatin1(generated::kLexmountApiKey);
    case Service::LexmountProjectId:
        return QString::fromLatin1(generated::kLexmountProjectId);
    }
    return {};
}

}
