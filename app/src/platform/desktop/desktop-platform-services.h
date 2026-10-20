#pragma once

#include "platform/platform-services.h"

namespace whilefresh::platform {

// Development stand-in (ADR-9): file dialog for the gallery and the camera, clipboard for sharing.
class DesktopPlatformServices : public PlatformServices
{
    Q_OBJECT

public:
    explicit DesktopPlatformServices(QObject *parent = nullptr);

    void pickImages() override;
    void captureReceipt() override;
    void shareText(const QString &text) override;

private:
    void importFromFileDialog(ImportPayload::Origin origin);
};

}
