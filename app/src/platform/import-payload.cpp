#include "platform/import-payload.h"

#include <QFileInfo>
#include <QStringList>

namespace whilefresh::platform {

QString ImportedImage::capturedAtSourceName() const
{
    return dateSourceName(capturedAtSource);
}

QString ImportPayload::summary() const
{
    QStringList lines;
    lines << QStringLiteral("%1 · %2").arg(kindName(kind), originName(origin));
    if (kind == Kind::Text) {
        constexpr int previewLength = 30;
        QString preview = text.left(previewLength).simplified();
        if (text.size() > previewLength)
            preview += QStringLiteral("…");
        lines << QStringLiteral("%1 字：%2").arg(text.size()).arg(preview);
        return lines.join(QLatin1Char('\n'));
    }
    lines << QStringLiteral("%1 张图片").arg(images.size());
    for (const ImportedImage &image : images) {
        const QString when = image.capturedAt.isValid()
                ? image.capturedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                : QStringLiteral("日期未知");
        lines << QStringLiteral("%1 %2 %3 (%4)")
                         .arg(QFileInfo(image.localPath).fileName(), image.mimeType, when,
                              image.capturedAtSourceName());
    }
    return lines.join(QLatin1Char('\n'));
}

QString ImportPayload::logLine() const
{
    QString line = QStringLiteral("kind=%1 origin=%2").arg(kindName(kind), originName(origin));
    if (kind == Kind::Text)
        return line + QStringLiteral(" chars=%1").arg(text.size());
    QStringList sources;
    for (const ImportedImage &image : images)
        sources << image.capturedAtSourceName();
    return line + QStringLiteral(" count=%1 capturedAtSource=%2")
                          .arg(images.size())
                          .arg(sources.isEmpty() ? QStringLiteral("-") : sources.join(QLatin1Char(',')));
}

QString kindName(ImportPayload::Kind kind)
{
    switch (kind) {
    case ImportPayload::Kind::Images:
        return QStringLiteral("images");
    case ImportPayload::Kind::Text:
        return QStringLiteral("text");
    }
    return {};
}

QString originName(ImportPayload::Origin origin)
{
    switch (origin) {
    case ImportPayload::Origin::Gallery:
        return QStringLiteral("gallery");
    case ImportPayload::Origin::Share:
        return QStringLiteral("share");
    case ImportPayload::Origin::Camera:
        return QStringLiteral("camera");
    }
    return {};
}

QString dateSourceName(ImportedImage::DateSource source)
{
    switch (source) {
    case ImportedImage::DateSource::None:
        return QStringLiteral("none");
    case ImportedImage::DateSource::Camera:
        return QStringLiteral("camera");
    case ImportedImage::DateSource::Exif:
        return QStringLiteral("exif");
    case ImportedImage::DateSource::MediaTaken:
        return QStringLiteral("media-taken");
    case ImportedImage::DateSource::MediaAdded:
        return QStringLiteral("media-added");
    case ImportedImage::DateSource::FileModified:
        return QStringLiteral("file-modified");
    }
    return {};
}

std::optional<ImportPayload::Origin> originFromName(const QString &name)
{
    for (auto origin : { ImportPayload::Origin::Gallery, ImportPayload::Origin::Share,
                         ImportPayload::Origin::Camera }) {
        if (originName(origin) == name)
            return origin;
    }
    return std::nullopt;
}

}
