#include "platform/bridge-event.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace whilefresh::platform {

namespace {

qint64 optionalMs(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toInteger(-1) : -1;
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
        event.message = object.value(QLatin1String("message")).toString();
        return event;
    }
    if (type != QLatin1String("import")) {
        *error = QStringLiteral("bridge message has an unknown event type");
        return std::nullopt;
    }

    event.type = BridgeEvent::Type::Import;
    for (const QJsonValue &failure : object.value(QLatin1String("failures")).toArray())
        event.failures << failure.toString();

    const QString kind = object.value(QLatin1String("kind")).toString();
    if (kind == QLatin1String("text")) {
        event.kind = ImportPayload::Kind::Text;
        event.text = object.value(QLatin1String("text")).toString();
        return event;
    }
    if (kind != QLatin1String("images")) {
        *error = QStringLiteral("bridge message has an unknown import kind");
        return std::nullopt;
    }

    event.kind = ImportPayload::Kind::Images;
    for (const QJsonValue &value : object.value(QLatin1String("images")).toArray()) {
        const QJsonObject item = value.toObject();
        StagedImage image;
        image.path = item.value(QLatin1String("path")).toString();
        if (image.path.isEmpty()) {
            *error = QStringLiteral("bridge image without a path");
            return std::nullopt;
        }
        image.mimeType = item.value(QLatin1String("mimeType")).toString();
        image.displayName = item.value(QLatin1String("displayName")).toString();
        image.hints.exifDateTimeOriginal = item.value(QLatin1String("exifDateTimeOriginal")).toString();
        image.hints.exifOffsetTimeOriginal = item.value(QLatin1String("exifOffsetTimeOriginal")).toString();
        image.hints.cameraCaptureMs = optionalMs(item, "cameraCaptureMs");
        image.hints.mediaDateTakenMs = optionalMs(item, "mediaDateTakenMs");
        image.hints.mediaDateAddedMs = optionalMs(item, "mediaDateAddedMs");
        image.hints.fileModifiedMs = optionalMs(item, "fileModifiedMs");
        event.images << image;
    }
    return event;
}

}
