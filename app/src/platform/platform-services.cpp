#include "platform/platform-services.h"

#include "platform/bridge-event.h"

#if defined(Q_OS_ANDROID)
#include "platform/android/android-platform-services.h"
#else
#include "platform/desktop/desktop-platform-services.h"
#endif

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>

namespace whilefresh::platform {

PlatformServices *PlatformServices::create(QObject *parent)
{
#if defined(Q_OS_ANDROID)
    return new AndroidPlatformServices(parent);
#else
    return new DesktopPlatformServices(parent);
#endif
}

PlatformServices::PlatformServices(QObject *parent)
    : QObject(parent)
{
    const int removed = removeStalePartFiles(importsDirectory(), QDateTime::currentDateTime());
    if (removed > 0)
        qInfo("WhileFresh import: removed %d stale partial files", removed);
}

void PlatformServices::handleBridgeMessage(const QByteArray &json)
{
    QString error;
    const std::optional<BridgeEvent> event = parseBridgeEvent(json, &error);
    if (!event) {
        qWarning("WhileFresh import: malformed bridge message: %s", qUtf8Printable(error));
        const BridgeLeftovers leftovers = bridgeLeftovers(json);
        const QString importsDir = importsDirectory();
        for (const QString &path : leftovers.stagedPaths)
            discardStagedFile(path, importsDir);
        deliverFailed(leftovers.origin, QStringLiteral("导入数据异常"));
        return;
    }
    switch (event->type) {
    case BridgeEvent::Type::Cancelled:
        deliverCancelled(event->origin);
        break;
    case BridgeEvent::Type::Failed:
        deliverFailed(event->origin, event->message);
        break;
    case BridgeEvent::Type::Import:
        if (event->kind == ImportPayload::Kind::Text)
            deliverText(event->origin, event->text);
        else
            deliverImages(event->origin, event->images, event->failures);
        break;
    }
}

void PlatformServices::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    qInfo("WhileFresh clipboard: copied chars=%lld", static_cast<long long>(text.size()));
}

void PlatformServices::deliverImages(ImportPayload::Origin origin, const QList<StagedImage> &staged,
                                     QStringList failures)
{
    const QDateTime now = QDateTime::currentDateTime();
    ImportPayload payload;
    payload.kind = ImportPayload::Kind::Images;
    payload.origin = origin;
    for (const StagedImage &item : staged) {
        QString error;
        if (const std::optional<ImportedImage> image = finalizeStagedImage(item, now, &error))
            payload.images << *image;
        else
            failures << error;
    }

    if (!payload.images.isEmpty()) {
        qInfo("WhileFresh import: %s", qUtf8Printable(payload.logLine()));
        emit importReceived(payload);
    }
    if (!failures.isEmpty()) {
        const QString message = payload.images.isEmpty()
                ? failures.join(QLatin1Char('\n'))
                : QStringLiteral("%1 张图片导入失败：%2").arg(failures.size()).arg(failures.join(QStringLiteral("；")));
        deliverFailed(origin, message);
    } else if (payload.images.isEmpty()) {
        deliverFailed(origin, QStringLiteral("没有收到图片"));
    }
}

void PlatformServices::deliverText(ImportPayload::Origin origin, const QString &text)
{
    if (text.trimmed().isEmpty()) {
        deliverFailed(origin, QStringLiteral("分享的文字为空"));
        return;
    }
    ImportPayload payload;
    payload.kind = ImportPayload::Kind::Text;
    payload.origin = origin;
    payload.text = text;
    qInfo("WhileFresh import: %s", qUtf8Printable(payload.logLine()));
    emit importReceived(payload);
}

void PlatformServices::deliverCancelled(ImportPayload::Origin origin)
{
    qInfo("WhileFresh import: cancelled origin=%s", qUtf8Printable(originName(origin)));
    emit importCancelled(origin);
}

void PlatformServices::deliverFailed(ImportPayload::Origin origin, const QString &message)
{
    qWarning("WhileFresh import: failed origin=%s message=%s", qUtf8Printable(originName(origin)),
             qUtf8Printable(message));
    emit importFailed(origin, message);
}

}
