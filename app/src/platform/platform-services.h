#pragma once

#include "platform/import-files.h"
#include "platform/import-payload.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace whilefresh::platform {

// Native import / export entry points for the UI. Imports are asynchronous; every request ends in
// at least one terminal signal (importReceived, importCancelled or importFailed). A partially
// failed import emits importReceived for the good items first, then importFailed. A request made
// while another gallery / camera request is still open fails with "正在处理上一次导入". Shares
// from other apps arrive unsolicited through the same signals with origin Share.
class PlatformServices : public QObject
{
    Q_OBJECT

public:
    // Android or desktop implementation, depending on the build.
    static PlatformServices *create(QObject *parent = nullptr);

    Q_INVOKABLE virtual void pickImages() = 0;
    Q_INVOKABLE virtual void captureReceipt() = 0;
    Q_INVOKABLE virtual void shareText(const QString &text) = 0;
    Q_INVOKABLE void copyText(const QString &text);

signals:
    void importReceived(const whilefresh::platform::ImportPayload &payload);
    // The user backed out of the picker or camera; not an error.
    void importCancelled(whilefresh::platform::ImportPayload::Origin origin);
    void importFailed(whilefresh::platform::ImportPayload::Origin origin, const QString &message);

protected:
    // Also removes stale *.part files from the imports directory.
    explicit PlatformServices(QObject *parent = nullptr);

    // One JSON message from the Java bridge (format: bridge-event.cpp). A malformed message still
    // yields importFailed, and the files it names are deleted.
    void handleBridgeMessage(const QByteArray &json);

    // Finalizes staged files, then emits importReceived and/or importFailed.
    void deliverImages(ImportPayload::Origin origin, const QList<StagedImage> &staged,
                       QStringList failures);
    void deliverText(ImportPayload::Origin origin, const QString &text);
    void deliverCancelled(ImportPayload::Origin origin);
    void deliverFailed(ImportPayload::Origin origin, const QString &message);
};

}
