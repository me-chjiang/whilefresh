#include "data/sqlite-support.h"
#include "platform/platform-services.h"
#include "data/builtin-data.h"
#include "ui/app-info.h"
#include "ui/app-model.h"

#ifdef WHILEFRESH_TLS_SMOKE
#include "recognition/tls-smoke.h"
#endif

#include <QQmlApplicationEngine>
#include <QtQml>

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

    QString dataError;
    const auto builtin = whilefresh::data::BuiltinData::builtIn(&dataError);
    if (!builtin) {
        qCritical("built-in data failed to load: %s", qUtf8Printable(dataError));
        return EXIT_FAILURE;
    }
    // Opens the database, retries pending file cleanup and restores demo mode.
    whilefresh::ui::AppModel appModel(platformServices, builtin);
    qmlRegisterSingletonInstance("WhileFresh", 1, 0, "App", &appModel);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("appInfo"), QVariant::fromValue(&appInfo) },
                                  { QStringLiteral("platform"), QVariant::fromValue(platformServices) },
                                  { QStringLiteral("debugTools"), debugTools } });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("WhileFresh", "Main");

#ifdef WHILEFRESH_TLS_SMOKE
    if (qEnvironmentVariableIntValue("WHILEFRESH_TLS_SMOKE") == 1)
        whilefresh::recognition::startTlsSmokeTest(&app);
#endif

    return QGuiApplication::exec();
}
