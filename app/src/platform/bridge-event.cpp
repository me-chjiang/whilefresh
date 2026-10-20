#include "platform/bridge-event.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace whilefresh::platform {

namespace {

// Optional string field: absent is fine, any other type is a protocol error.
bool readString(const QJsonObject &object, const char *key, QString *out)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isUndefined())
        return true;
    if (!value.isString())
        return false;
    *out = value.toString();
    return true;
}

bool readMs(const QJsonObject &object, const char *key, qint64 *out)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isUndefined())
        return true;
    if (!value.isDouble())
        return false;
    *out = value.toInteger(-1);
    return true;
}

bool readImage(const QJsonValue &value, StagedImage *image)
{
    if (!value.isObject())
        return false;
    const QJsonObject item = value.toObject();
    const QJsonValue path = item.value(QLatin1String("path"));
    if (!path.isString() || path.toString().isEmpty())
        return false;
    image->path = path.toString();
    return readString(item, "mimeType", &image->mimeType)
            && readString(item, "displayName", &image->displayName)
            && readString(item, "exifDateTimeOriginal", &image->hints.exifDateTimeOriginal)
            && readString(item, "exifOffsetTimeOriginal", &image->hints.exifOffsetTimeOriginal)
            && readMs(item, "cameraCaptureMs", &image->hints.cameraCaptureMs)
            && readMs(item, "mediaDateTakenMs", &image->hints.mediaDateTakenMs)
            && readMs(item, "mediaDateAddedMs", &image->hints.mediaDateAddedMs)
            && readMs(item, "fileModifiedMs", &image->hints.fileModifiedMs);
}

}

std::optional<BridgeEvent> parseBridgeEvent(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (!document.isObject()) {
        *error = QStringLiteral("bridge message is not a JSON object: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    BridgeEvent event;
    const std::optional<ImportPayload::Origin> origin =
            originFromName(object.value(QLatin1String("origin")).toString());
    if (!origin) {
        *error = QStringLiteral("bridge message has an unknown origin");
        return std::nullopt;
    }
    event.origin = *origin;

    const QString type = object.value(QLatin1String("event")).toString();
    if (type == QLatin1String("cancelled")) {
        event.type = BridgeEvent::Type::Cancelled;
        return event;
    }
    if (type == QLatin1String("failed")) {
        event.type = BridgeEvent::Type::Failed;
        if (!readString(object, "message", &event.message)) {
            *error = QStringLiteral("bridge failure message is not a string");
            return std::nullopt;
        }
        return event;
    }
    if (type != QLatin1String("import")) {
        *error = QStringLiteral("bridge message has an unknown event type");
        return std::nullopt;
    }

    event.type = BridgeEvent::Type::Import;
    const QJsonValue failures = object.value(QLatin1String("failures"));
    if (!failures.isUndefined()) {
        if (!failures.isArray()) {
            *error = QStringLiteral("bridge failures is not an array");
            return std::nullopt;
        }
        for (const QJsonValue &failure : failures.toArray()) {
            if (!failure.isString()) {
                *error = QStringLiteral("bridge failure entry is not a string");
                return std::nullopt;
            }
            event.failures << failure.toString();
        }
    }

    const QString kind = object.value(QLatin1String("kind")).toString();
    if (kind == QLatin1String("text")) {
        event.kind = ImportPayload::Kind::Text;
        const QJsonValue text = object.value(QLatin1String("text"));
        if (!text.isString()) {
            *error = QStringLiteral("bridge text is not a string");
            return std::nullopt;
        }
        event.text = text.toString();
        return event;
    }
    if (kind != QLatin1String("images")) {
        *error = QStringLiteral("bridge message has an unknown import kind");
        return std::nullopt;
    }

    event.kind = ImportPayload::Kind::Images;
    const QJsonValue images = object.value(QLatin1String("images"));
    if (!images.isArray()) {
        *error = QStringLiteral("bridge images is not an array");
        return std::nullopt;
    }
    for (const QJsonValue &value : images.toArray()) {
        StagedImage image;
        if (!readImage(value, &image)) {
            *error = QStringLiteral("bridge image entry is malformed");
            return std::nullopt;
        }
        event.images << image;
    }
    return event;
}

BridgeLeftovers bridgeLeftovers(const QByteArray &json)
{
    BridgeLeftovers leftovers;
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    if (const auto origin = originFromName(object.value(QLatin1String("origin")).toString()))
        leftovers.origin = *origin;
    for (const QJsonValue &value : object.value(QLatin1String("images")).toArray()) {
        const QString path = value.toObject().value(QLatin1String("path")).toString();
        if (!path.isEmpty())
            leftovers.stagedPaths << path;
    }
    return leftovers;
}

}
