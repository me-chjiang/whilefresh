#include "recognition/recognition-types.h"

#include "recognition/recognition-log.h"

namespace whilefresh::recognition {

Q_LOGGING_CATEGORY(lcRecognition, "whilefresh.recognition")

namespace {

template<typename E>
struct Name {
    E value;
    const char *text;
};

constexpr Name<InputKind> kInputKinds[] = {
    { InputKind::Screenshot, "screenshot" },
    { InputKind::Receipt, "receipt" },
    { InputKind::OrderText, "order-text" },
};
constexpr Name<SourceType> kSourceTypes[] = {
    { SourceType::Cart, "cart" },
    { SourceType::Order, "order" },
    { SourceType::Receipt, "receipt" },
    { SourceType::Text, "text" },
    { SourceType::Other, "other" },
};
constexpr Name<ItemCategory> kCategories[] = {
    { ItemCategory::Food, "food" },
    { ItemCategory::Seasoning, "seasoning" },
    { ItemCategory::Irrelevant, "irrelevant" },
};
constexpr Name<StorageKind> kStorages[] = {
    { StorageKind::Unknown, "unknown" },
    { StorageKind::Fridge, "fridge" },
    { StorageKind::Freezer, "freezer" },
    { StorageKind::Room, "room" },
};
constexpr Name<ErrorKind> kErrorKinds[] = {
    { ErrorKind::NotConfigured, "not-configured" },
    { ErrorKind::NoNetwork, "no-network" },
    { ErrorKind::Timeout, "timeout" },
    { ErrorKind::AuthFailed, "auth-failed" },
    { ErrorKind::QuotaOrRateLimit, "quota-or-rate-limit" },
    { ErrorKind::ServerError, "server-error" },
    { ErrorKind::RequestRejected, "request-rejected" },
    { ErrorKind::BadResponse, "bad-response" },
    { ErrorKind::InvalidInput, "invalid-input" },
    { ErrorKind::SecureConnectionFailed, "secure-connection-failed" },
    { ErrorKind::Internal, "internal" },
};

template<typename E, size_t N>
QString nameOf(const Name<E> (&table)[N], E value)
{
    for (const auto &entry : table) {
        if (entry.value == value)
            return QString::fromLatin1(entry.text);
    }
    return {};
}

template<typename E, size_t N>
std::optional<E> valueOf(const Name<E> (&table)[N], const QString &text)
{
    const QString key = text.trimmed().toLower();
    for (const auto &entry : table) {
        if (key == QLatin1String(entry.text))
            return entry.value;
    }
    return std::nullopt;
}

}

QString enumKey(InputKind kind) { return nameOf(kInputKinds, kind); }
std::optional<InputKind> inputKindFromString(const QString &text) { return valueOf(kInputKinds, text); }
QString enumKey(SourceType type) { return nameOf(kSourceTypes, type); }
std::optional<SourceType> sourceTypeFromString(const QString &text) { return valueOf(kSourceTypes, text); }
QString enumKey(ItemCategory category) { return nameOf(kCategories, category); }
std::optional<ItemCategory> itemCategoryFromString(const QString &text) { return valueOf(kCategories, text); }
QString enumKey(StorageKind storage) { return nameOf(kStorages, storage); }
std::optional<StorageKind> storageKindFromString(const QString &text) { return valueOf(kStorages, text); }
QString enumKey(ErrorKind kind) { return nameOf(kErrorKinds, kind); }

QString uncategorizedName()
{
    return QStringLiteral("未归类");
}

QString userMessageFor(ErrorKind kind)
{
    switch (kind) {
    case ErrorKind::NotConfigured:
        return QStringLiteral("识别服务尚未配置，暂时无法识别。你仍可以手动添加食材。");
    case ErrorKind::NoNetwork:
        return QStringLiteral("网络不可用，请检查网络连接后重试。");
    case ErrorKind::Timeout:
        return QStringLiteral("识别超时了，请稍后重试。");
    case ErrorKind::AuthFailed:
        return QStringLiteral("识别服务拒绝了请求，可能是服务凭据失效。请稍后再试或联系开发者。");
    case ErrorKind::QuotaOrRateLimit:
        return QStringLiteral("识别服务额度已用完或请求太频繁，请稍后再试。");
    case ErrorKind::ServerError:
        return QStringLiteral("识别服务暂时不可用，请稍后重试。");
    case ErrorKind::RequestRejected:
        return QStringLiteral("识别服务无法处理这次请求，请换一张图片或稍后重试。");
    case ErrorKind::BadResponse:
        return QStringLiteral("没能看懂识别结果，请重试一次。");
    case ErrorKind::InvalidInput:
        return QStringLiteral("没有可识别的内容，请重新选择图片或文字。");
    case ErrorKind::SecureConnectionFailed:
        return QStringLiteral("安全连接失败，请检查系统时间或网络环境。");
    case ErrorKind::Internal:
        return QStringLiteral("图片处理组件缺失，请更新应用。");
    }
    return {};
}

RecognitionError makeError(ErrorKind kind, const QString &detail, int httpStatus)
{
    return { kind, userMessageFor(kind), detail, httpStatus };
}

}
