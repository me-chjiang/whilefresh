#include "platform/import-files.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace whilefresh::platform {

namespace {

struct KnownImageType
{
    const char *mimeType;
    const char *extension;
};

constexpr KnownImageType knownImageTypes[] = {
    { "image/jpeg", "jpg" }, { "image/png", "png" },   { "image/webp", "webp" },
    { "image/heic", "heic" }, { "image/heif", "heif" }, { "image/gif", "gif" },
    { "image/bmp", "bmp" },
};

}

QString imageExtension(const QString &mimeType, const QString &displayName)
{
    const QString mime = mimeType.trimmed().toLower();
    for (const KnownImageType &type : knownImageTypes) {
        if (mime == QLatin1String(type.mimeType))
            return QString::fromLatin1(type.extension);
    }
    if (mime == QLatin1String("image/jpg") || mime == QLatin1String("image/pjpeg"))
        return QStringLiteral("jpg");

    static const QRegularExpression suffixPattern(QStringLiteral("\\.([A-Za-z0-9]{1,5})$"));
    const QRegularExpressionMatch match = suffixPattern.match(displayName.trimmed());
    if (match.hasMatch()) {
        const QString suffix = match.captured(1).toLower();
        return suffix == QLatin1String("jpeg") ? QStringLiteral("jpg") : suffix;
    }
    return QStringLiteral("img");
}

QString imageMimeType(const QString &extension)
{
    for (const KnownImageType &type : knownImageTypes) {
        if (extension == QLatin1String(type.extension))
            return QString::fromLatin1(type.mimeType);
    }
    return {};
}

QString importsDirectory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/imports");
    QDir().mkpath(dir);
    return dir;
}

bool discardStagedFile(const QString &path, const QString &importsDir)
{
    const QFileInfo file(path);
    const QString dir = QFileInfo(importsDir).canonicalFilePath();
    if (dir.isEmpty() || !file.isFile() || file.canonicalPath() != dir)
        return false;
    return QFile::remove(file.filePath());
}

int removeStalePartFiles(const QString &dir, const QDateTime &now)
{
    int removed = 0;
    const QDateTime cutoff = now.addSecs(-24 * 3600);
    const QFileInfoList parts = QDir(dir).entryInfoList({ QStringLiteral("*.part") }, QDir::Files);
    for (const QFileInfo &part : parts) {
        if (part.lastModified() < cutoff && QFile::remove(part.filePath()))
            ++removed;
    }
    return removed;
}

std::optional<ImportedImage> finalizeStagedImage(const StagedImage &staged, const QDateTime &now,
                                                 QString *error)
{
    const QFileInfo info(staged.path);
    if (!info.isFile() || info.size() == 0) {
        *error = QStringLiteral("导入的文件不存在或为空");
        QFile::remove(staged.path);
        return std::nullopt;
    }

    const QString extension = imageExtension(staged.mimeType, staged.displayName);
    const QString target = info.dir().filePath(info.completeBaseName() + QLatin1Char('.') + extension);
    if (target != info.filePath() && !QFile::rename(info.filePath(), target)) {
        *error = QStringLiteral("无法保存导入的文件");
        QFile::remove(staged.path);
        return std::nullopt;
    }

    QString mime = staged.mimeType.trimmed().toLower();
    if (mime.isEmpty() || mime.endsWith(QLatin1String("/*")))
        mime = imageMimeType(extension);

    const CapturedAt capturedAt = resolveCapturedAt(staged.hints, now);
    ImportedImage image;
    image.localPath = target;
    image.mimeType = mime;
    image.capturedAt = capturedAt.when;
    image.capturedAtSource = capturedAt.source;
    return image;
}

}
