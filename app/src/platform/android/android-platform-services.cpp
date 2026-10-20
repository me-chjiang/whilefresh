#include "platform/android/android-platform-services.h"

#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>

namespace whilefresh::platform {

namespace {

constexpr char bridgeClass[] = "io/github/mechjiang/whilefresh/WhileFreshBridge";

QMutex instanceMutex;
AndroidPlatformServices *instance = nullptr;

// Called on the bridge's worker or UI thread, never on the Qt main thread.
void JNICALL jniOnBridgeMessage(JNIEnv *env, jclass, jstring json)
{
    // GetStringChars keeps UTF-16 intact; GetStringUTFChars would mangle emoji (modified UTF-8).
    if (!json)
        return;
    const jsize length = env->GetStringLength(json);
    const jchar *chars = env->GetStringChars(json, nullptr);
    if (!chars)
        return; // OutOfMemoryError is pending; the JVM reports it when we return
    const QString message(reinterpret_cast<const QChar *>(chars), length);
    env->ReleaseStringChars(json, chars);

    QMutexLocker locker(&instanceMutex);
    if (!instance)
        return;
    AndroidPlatformServices *target = instance;
    QMetaObject::invokeMethod(target, [target, message] { target->onBridgeMessage(message); },
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
            { "nativeOnMessage", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniOnBridgeMessage) },
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

void AndroidPlatformServices::onBridgeMessage(const QString &json)
{
    handleBridgeMessage(json.toUtf8());
}

}
