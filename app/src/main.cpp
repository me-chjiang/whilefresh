#include "data/sqlite-support.h"
#include "platform/platform-services.h"
#include "ui/app-info.h"

#ifdef WHILEFRESH_TLS_SMOKE
#include "recognition/tls-smoke.h"
#endif

#include <QQmlApplicationEngine>

#ifdef Q_OS_ANDROID
#include <QGuiApplication>
using Application = QGuiApplication;
#else
// The desktop stand-in for the system picker uses QFileDialog, which needs QApplication.
#include <QApplication>
using Application = QApplication;
#endif

int main(int argc, char *argv[])
{
    Application app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("WhileFresh"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("趁鲜"));

    whilefresh::ui::AppInfo appInfo;
    qInfo("WhileFresh startup: version=%s platform=%s sqlite=%d recognition-configured=%d",
          qUtf8Printable(appInfo.version()), qUtf8Printable(appInfo.platform()),
          whilefresh::data::isSqliteDriverAvailable(), appInfo.recognitionConfigured());

#ifdef WHILEFRESH_DEBUG_TOOLS
    constexpr bool debugTools = true;
#else
    constexpr bool debugTools = false;
#endif
    // Created before the engine so a share that cold-started the app is queued for QML.
    auto *platformServices = whilefresh::platform::PlatformServices::create(&app);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("appInfo"), QVariant::fromValue(&appInfo) },
                                  { QStringLiteral("platform"), QVariant::fromValue(platformServices) },
                                  { QStringLiteral("debugTools"), debugTools } });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("WhileFresh", "Main");

#ifdef WHILEFRESH_TLS_SMOKE
    whilefresh::recognition::startTlsSmokeTest(&app);
#endif

    return QGuiApplication::exec();
}
