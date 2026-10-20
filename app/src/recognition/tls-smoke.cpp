#include "recognition/tls-smoke.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QUrl>

namespace whilefresh::recognition {

namespace {
constexpr auto kPrefix = "WhileFresh tls-smoke:";
constexpr int kTimeoutMs = 20000;
}

void startTlsSmokeTest(QObject *parent)
{
    qInfo("%s backend=%s supportsSsl=%d library=%s", kPrefix,
          qUtf8Printable(QSslSocket::activeBackend()), QSslSocket::supportsSsl(),
          qUtf8Printable(QSslSocket::sslLibraryVersionString()));

    auto *manager = new QNetworkAccessManager(parent);
    QNetworkRequest request(QUrl(QStringLiteral("https://api.deepseek.com/models")));
    request.setTransferTimeout(kTimeoutMs);
    QNetworkReply *reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, manager, [manager, reply] {
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (status.isValid())
            qInfo("%s ok http-status=%d", kPrefix, status.toInt());
        else
            qWarning("%s failed error=%d %s", kPrefix, int(reply->error()),
                     qUtf8Printable(reply->errorString()));
        reply->deleteLater();
        manager->deleteLater();
    });
}

}
