#include "platform/import-files.h"
#include "platform/platform-services.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::platform;

namespace {

// Exposes the bridge entry point; the request methods are irrelevant here.
class TestServices : public PlatformServices
{
public:
    using PlatformServices::handleBridgeMessage;

    void pickImages() override { }
    void captureReceipt() override { }
    void shareText(const QString &) override { }
};

QString stage(const QString &name)
{
    QFile file(QDir(importsDirectory()).filePath(name));
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write("not empty");
    return file.fileName();
}

QByteArray imagesMessage(const QStringList &paths, const QStringList &failures)
{
    QStringList images;
    for (const QString &path : paths)
        images << QStringLiteral(R"({"path":"%1","mimeType":"image/png"})").arg(path);
    QStringList quoted;
    for (const QString &failure : failures)
        quoted << QLatin1Char('"') + failure + QLatin1Char('"');
    return QStringLiteral(R"({"event":"import","origin":"gallery","kind":"images","images":[%1],"failures":[%2]})")
            .arg(images.join(QLatin1Char(',')), quoted.join(QLatin1Char(',')))
            .toUtf8();
}

}

class PlatformServicesTest : public QObject
{
    Q_OBJECT

private:
    TestServices *services = nullptr;
    QStringList order;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(QStringLiteral("WhileFreshPlatformTest"));
    }

    void init()
    {
        QDir(importsDirectory()).removeRecursively();
        services = new TestServices;
        order.clear();
        connect(services, &PlatformServices::importReceived, this, [this] { order << QStringLiteral("received"); });
        connect(services, &PlatformServices::importCancelled, this, [this] { order << QStringLiteral("cancelled"); });
        connect(services, &PlatformServices::importFailed, this, [this] { order << QStringLiteral("failed"); });
    }

    void cleanup()
    {
        delete services;
        services = nullptr;
        QDir(importsDirectory()).removeRecursively();
    }

    void allImagesSucceed()
    {
        QSignalSpy received(services, &PlatformServices::importReceived);
        services->handleBridgeMessage(imagesMessage({ stage(QStringLiteral("a.part")), stage(QStringLiteral("b.part")) }, {}));
        QCOMPARE(order, QStringList { QStringLiteral("received") });
        const auto payload = received.at(0).at(0).value<ImportPayload>();
        QCOMPARE(payload.origin, ImportPayload::Origin::Gallery);
        QCOMPARE(payload.images.size(), 2);
        QVERIFY(payload.images[0].localPath.endsWith(QStringLiteral("/a.png")));
        QVERIFY(QFileInfo::exists(payload.images[1].localPath));
    }

    void partialFailureReceivesThenFails()
    {
        QSignalSpy failed(services, &PlatformServices::importFailed);
        const QString missing = QDir(importsDirectory()).filePath(QStringLiteral("missing.part"));
        services->handleBridgeMessage(imagesMessage({ stage(QStringLiteral("a.part")), missing }, { QStringLiteral("不是图片") }));
        QCOMPARE(order, (QStringList { QStringLiteral("received"), QStringLiteral("failed") }));
        QVERIFY(failed.at(0).at(1).toString().startsWith(QStringLiteral("2 张图片导入失败")));
    }

    void allFailedOnlyFails()
    {
        QSignalSpy failed(services, &PlatformServices::importFailed);
        services->handleBridgeMessage(imagesMessage({}, { QStringLiteral("不是图片"), QStringLiteral("图片超过 50 MB") }));
        QCOMPARE(order, QStringList { QStringLiteral("failed") });
        QCOMPARE(failed.at(0).at(0).value<ImportPayload::Origin>(), ImportPayload::Origin::Gallery);
    }

    void emptyListFails()
    {
        QSignalSpy failed(services, &PlatformServices::importFailed);
        services->handleBridgeMessage(imagesMessage({}, {}));
        QCOMPARE(order, QStringList { QStringLiteral("failed") });
        QCOMPARE(failed.at(0).at(1).toString(), QStringLiteral("没有收到图片"));
    }

    void malformedMessageFailsAndDeletesStagedFiles()
    {
        QTemporaryDir elsewhere;
        QFile outside(elsewhere.filePath(QStringLiteral("keep.part")));
        QVERIFY(outside.open(QIODevice::WriteOnly));
        outside.close();
        const QString staged = stage(QStringLiteral("a.part"));
        const QByteArray json = QStringLiteral(
                R"({"event":"import","origin":"camera","kind":"images","images":[{"path":"%1","mimeType":5},{"path":"%2"}]})")
                .arg(staged, outside.fileName()).toUtf8();

        QSignalSpy failed(services, &PlatformServices::importFailed);
        services->handleBridgeMessage(json);
        QCOMPARE(order, QStringList { QStringLiteral("failed") });
        QCOMPARE(failed.at(0).at(0).value<ImportPayload::Origin>(), ImportPayload::Origin::Camera);
        QVERIFY(!QFileInfo::exists(staged));
        QVERIFY(QFileInfo::exists(outside.fileName()));
    }

    void unparsableMessageStillFails()
    {
        services->handleBridgeMessage("{not json");
        QCOMPARE(order, QStringList { QStringLiteral("failed") });
    }

    void cancelAndText()
    {
        QSignalSpy cancelled(services, &PlatformServices::importCancelled);
        services->handleBridgeMessage(R"({"event":"cancelled","origin":"camera"})");
        QCOMPARE(cancelled.size(), 1);

        QSignalSpy received(services, &PlatformServices::importReceived);
        services->handleBridgeMessage(R"({"event":"import","origin":"share","kind":"text","text":"鸡蛋"})");
        QCOMPARE(received.at(0).at(0).value<ImportPayload>().text, QStringLiteral("鸡蛋"));
    }

    void constructionRemovesStalePartFiles()
    {
        const QString stale = stage(QStringLiteral("stale.part"));
        {
            QFile file(stale);
            QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.setFileTime(QDateTime::currentDateTime().addDays(-2), QFileDevice::FileModificationTime));
        }
        const QString fresh = stage(QStringLiteral("fresh.part"));
        TestServices another;
        QVERIFY(!QFileInfo::exists(stale));
        QVERIFY(QFileInfo::exists(fresh));
    }
};

QTEST_GUILESS_MAIN(PlatformServicesTest)

#include "platform-services-test.moc"
