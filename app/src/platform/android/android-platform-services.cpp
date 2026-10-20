#include "platform/android/android-platform-services.h"

#include "platform/bridge-event.h"

#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>

namespace whilefresh::platform {

namespace {

constexpr char bridgeClass[] = "io/github/mechjiang/whilefresh/WhileFreshBridge";

QMutex instanceMutex;
AndroidPlatformServices *instance = nullptr;

// Called on the bridge's worker or UI thread, never on the Qt main thread.
void JNICALL onBridgeMessage(JNIEnv *env, jclass, jstring json)
{
    // GetStringChars keeps UTF-16 intact; GetStringUTFChars would mangle emoji (modified UTF-8).
    const jsize length = env->GetStringLength(json);
    const jchar *chars = env->GetStringChars(json, nullptr);
    const QString message(reinterpret_cast<const QChar *>(chars), length);
    env->ReleaseStringChars(json, chars);

    QMutexLocker locker(&instanceMutex);
    if (!instance)
        return;
    AndroidPlatformServices *target = instance;
    QMetaObject::invokeMethod(target, [target, message] { target->handleBridgeMessage(message); },
                              Qt::QueuedConnection);
}

void setNativeReady(bool ready)
{
    QJniObject::callStaticMethod<void>(bridgeClass, "setNativeReady", "(Z)V", jboolean(ready));
}

}

AndroidPlatformServices::AndroidPlatformServices(QObject *parent)
    : PlatformServices(parent)
{
    static const bool registered = [] {
        const JNINativeMethod methods[] = {
            { "nativeOnMessage", "(Ljava/lang/String;)V", reinterpret_cast<void *>(onBridgeMessage) },
        };
        QJniEnvironment env;
        return env.registerNativeMethods(bridgeClass, methods, 1);
    }();
    if (!registered) {
        qWarning("WhileFresh import: could not register the Java bridge, imports are disabled");
        return;
    }

    {
        QMutexLocker locker(&instanceMutex);
        Q_ASSERT(!instance);
        instance = this;
    }
    // Flushes anything that arrived before Qt was ready, e.g. the share that cold-started the app.
    setNativeReady(true);
}

AndroidPlatformServices::~AndroidPlatformServices()
{
    setNativeReady(false);
    QMutexLocker locker(&instanceMutex);
    if (instance == this)
        instance = nullptr;
}

void AndroidPlatformServices::pickImages()
{
    QJniObject::callStaticMethod<void>(bridgeClass, "pickImages");
}

void AndroidPlatformServices::captureReceipt()
{
    QJniObject::callStaticMethod<void>(bridgeClass, "captureReceipt");
}

void AndroidPlatformServices::shareText(const QString &text)
{
    const QJniObject javaText = QJniObject::fromString(text);
    const QJniObject title = QJniObject::fromString(QStringLiteral("分享到"));
    QJniObject::callStaticMethod<void>(bridgeClass, "shareText", "(Ljava/lang/String;Ljava/lang/String;)V",
                                       javaText.object<jstring>(), title.object<jstring>());
    qInfo("WhileFresh share: opened the share sheet chars=%lld", static_cast<long long>(text.size()));
}

void AndroidPlatformServices::handleBridgeMessage(const QString &json)
{
    QString error;
    const std::optional<BridgeEvent> event = parseBridgeEvent(json.toUtf8(), &error);
    if (!event) {
        qWarning("WhileFresh import: dropped a malformed bridge message: %s", qUtf8Printable(error));
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

}
