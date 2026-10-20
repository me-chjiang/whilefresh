#include "data/sqlite-support.h"
#include "ui/app-info.h"

#ifdef WHILEFRESH_TLS_SMOKE
#include "recognition/tls-smoke.h"
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("WhileFresh"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("趁鲜"));

    whilefresh::ui::AppInfo appInfo;
    qInfo("WhileFresh startup: version=%s platform=%s sqlite=%d recognition-configured=%d",
          qUtf8Printable(appInfo.version()), qUtf8Printable(appInfo.platform()),
          whilefresh::data::isSqliteDriverAvailable(), appInfo.recognitionConfigured());

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("appInfo"), QVariant::fromValue(&appInfo) } });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("WhileFresh", "Main");

#ifdef WHILEFRESH_TLS_SMOKE
    whilefresh::recognition::startTlsSmokeTest(&app);
#endif

    return QGuiApplication::exec();
}
