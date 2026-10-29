#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <optional>

namespace whilefresh::platform {

class ImportedImage
{
    Q_GADGET
    Q_PROPERTY(QString localPath MEMBER localPath)
    Q_PROPERTY(QString mimeType MEMBER mimeType)
    Q_PROPERTY(QDateTime capturedAt MEMBER capturedAt)
    Q_PROPERTY(DateSource capturedAtSource MEMBER capturedAtSource)
    Q_PROPERTY(QString capturedAtSourceName READ capturedAtSourceName)

public:
    // Most to least trustworthy. None: no usable date, the user has to confirm "today" for
    // relative dates such as "今天生产".
    enum class DateSource { None, Camera, Exif, MediaTaken, MediaAdded, FileModified };
    Q_ENUM(DateSource)

    // A file in the app's private imports directory; the app owns it from now on.
    QString localPath;
    QString mimeType;
    QDateTime capturedAt;
    DateSource capturedAtSource = DateSource::None;

    QString capturedAtSourceName() const;
};

class ImportPayload
{
    Q_GADGET
    Q_PROPERTY(Kind kind MEMBER kind)
    Q_PROPERTY(Origin origin MEMBER origin)
    Q_PROPERTY(QList<whilefresh::platform::ImportedImage> images READ imageList)
    Q_PROPERTY(int imageCount READ imageCount)
    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(QString summary READ summary)

public:
    enum class Kind { Images, Text };
    Q_ENUM(Kind)
    enum class Origin { Gallery, Share, Camera, Paste };
    Q_ENUM(Origin)

    Kind kind = Kind::Images;
    Origin origin = Origin::Gallery;
    QList<ImportedImage> images;
    QString text;

    QList<ImportedImage> imageList() const { return images; }
    int imageCount() const { return images.size(); }

    // Multi-line description for the debug panel; includes a short text preview.
    QString summary() const;
    // One log line without any text content or file paths.
    QString logLine() const;
};

QString kindName(ImportPayload::Kind kind);
QString originName(ImportPayload::Origin origin);
QString dateSourceName(ImportedImage::DateSource source);
std::optional<ImportPayload::Origin> originFromName(const QString &name);

}
