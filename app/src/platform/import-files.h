#pragma once

#include "platform/captured-at.h"
#include "platform/import-payload.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace whilefresh::platform {

// A copied image waiting for its final name, e.g. imports/<uuid>.part written by the Java bridge.
struct StagedImage
{
    QString path;
    QString mimeType;    // may be empty or a wildcard such as "image/*"
    QString displayName; // only used to guess the extension, never as a path
    ImageDateHints hints;
};

// Lower-case extension without the dot. MIME type wins over the display name; "img" if neither helps.
QString imageExtension(const QString &mimeType, const QString &displayName);
// Concrete MIME type for a known image extension, otherwise empty.
QString imageMimeType(const QString &extension);

// <AppDataLocation>/imports, created on demand. On Android this is files/imports.
QString importsDirectory();

// Renames the staged file to <same dir>/<base name>.<ext> and resolves its date.
std::optional<ImportedImage> finalizeStagedImage(const StagedImage &staged, const QDateTime &now,
                                                 QString *error);

}
