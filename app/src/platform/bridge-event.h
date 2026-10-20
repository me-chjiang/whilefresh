#pragma once

#include "platform/import-files.h"
#include "platform/import-payload.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace whilefresh::platform {

// One JSON message from the Java bridge (app/android/src/.../WhileFreshBridge.java). Lives outside
// android/ so the protocol can be unit-tested on the desktop.
struct BridgeEvent
{
    enum class Type { Import, Cancelled, Failed };

    Type type = Type::Failed;
    ImportPayload::Origin origin = ImportPayload::Origin::Share;
    ImportPayload::Kind kind = ImportPayload::Kind::Images;
    QList<StagedImage> images;
    QString text;
    QStringList failures; // per-item problems of an otherwise successful import
    QString message;      // reason of a Failed event
};

std::optional<BridgeEvent> parseBridgeEvent(const QByteArray &json, QString *error);

}
