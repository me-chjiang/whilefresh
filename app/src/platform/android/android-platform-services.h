#pragma once

#include "platform/platform-services.h"

namespace whilefresh::platform {

// JNI glue for app/android/src/io/github/mechjiang/whilefresh/WhileFreshBridge.java (ADR-8).
// Only one instance may exist: it is the target of the bridge's native callback.
class AndroidPlatformServices : public PlatformServices
{
    Q_OBJECT

public:
    explicit AndroidPlatformServices(QObject *parent = nullptr);
    ~AndroidPlatformServices() override;

    void pickImages() override;
    void captureReceipt() override;
    void shareText(const QString &text) override;

    // Runs on the Qt main thread for each JSON message posted by the bridge.
    void onBridgeMessage(const QString &json);
};

}
