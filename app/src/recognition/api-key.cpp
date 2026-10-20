#include "recognition/api-key.h"

#include "generated/api-config.h"

namespace whilefresh::recognition {

QString deepSeekApiKey()
{
    return QString::fromLatin1(generated::kDeepSeekApiKey);
}

bool isDeepSeekApiKeyConfigured()
{
    return generated::kDeepSeekApiKey[0] != '\0';
}

}
