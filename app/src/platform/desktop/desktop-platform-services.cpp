#include "platform/desktop/desktop-platform-services.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUuid>

namespace whilefresh::platform {

DesktopPlatformServices::DesktopPlatformServices(QObject *parent)
    : PlatformServices(parent)
{
}

void DesktopPlatformServices::pickImages()
{
    importFromFileDialog(ImportPayload::Origin::Gallery);
}

void DesktopPlatformServices::captureReceipt()
{
    importFromFileDialog(ImportPayload::Origin::Camera);
}

void DesktopPlatformServices::shareText(const QString &text)
{
    copyText(text);
    qInfo("WhileFresh share: desktop has no share sheet, copied the text to the clipboard instead");
}

void DesktopPlatformServices::importFromFileDialog(ImportPayload::Origin origin)
{
    const bool camera = origin == ImportPayload::Origin::Camera;
    const QString filter = QStringLiteral("图片 (*.png *.jpg *.jpeg *.webp *.heic *.heif *.gif *.bmp)");
    QStringList paths;
    if (camera) {
        const QString path = QFileDialog::getOpenFileName(nullptr, QStringLiteral("选择一张小票照片"),
                                                          QDir::homePath(), filter);
        if (!path.isEmpty())
            paths << path;
    } else {
        paths = QFileDialog::getOpenFileNames(nullptr, QStringLiteral("选择图片"), QDir::homePath(), filter);
    }
    if (paths.isEmpty()) {
        deliverCancelled(origin);
        return;
    }

    const QString importsDir = importsDirectory();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QMimeDatabase mimeDatabase;
    QList<StagedImage> staged;
    QStringList failures;
    for (const QString &path : paths) {
        const QFileInfo source(path);
        StagedImage image;
        image.path = QDir(importsDir).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)
                                               + QStringLiteral(".part"));
        if (!QFile::copy(path, image.path)) {
            failures << QStringLiteral("无法读取所选文件");
            continue;
        }
        image.mimeType = mimeDatabase.mimeTypeForFile(source).name();
        image.displayName = source.fileName();
        // Mirror Android: a camera photo is dated when it was taken, i.e. now.
        if (camera)
            image.hints.cameraCaptureMs = nowMs;
        else
            image.hints.fileModifiedMs = source.lastModified().toMSecsSinceEpoch();
        staged << image;
    }
    deliverImages(origin, staged, failures);
}

}
