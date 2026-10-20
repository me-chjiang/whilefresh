#pragma once

#include "platform/import-payload.h"

#include <QDateTime>
#include <QString>

namespace whilefresh::platform {

// Raw date clues collected for one image; -1 means "not available". All times are ms since epoch.
struct ImageDateHints
{
    QString exifDateTimeOriginal;   // "yyyy:MM:dd HH:mm:ss", local time of the camera
    QString exifOffsetTimeOriginal; // "+08:00", optional
    qint64 cameraCaptureMs = -1;    // set when the app itself launched the camera
    qint64 mediaDateTakenMs = -1;   // MediaStore DATE_TAKEN
    qint64 mediaDateAddedMs = -1;   // MediaStore DATE_ADDED
    qint64 fileModifiedMs = -1;     // MediaStore DATE_MODIFIED, document last_modified or file mtime
};

struct CapturedAt
{
    QDateTime when;
    ImportedImage::DateSource source = ImportedImage::DateSource::None;
};

// Invalid QDateTime when the string is empty, blank-filled or malformed.
QDateTime parseExifDateTime(const QString &dateTime, const QString &offset = {});

// Picks the most trustworthy plausible date: camera > EXIF > DATE_TAKEN > DATE_ADDED > modified.
// Dates before 2000 (unset camera clocks) or more than a day after `now` are ignored.
CapturedAt resolveCapturedAt(const ImageDateHints &hints, const QDateTime &now);

}
