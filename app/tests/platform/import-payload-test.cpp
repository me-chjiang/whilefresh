#include "platform/bridge-event.h"
#include "platform/import-files.h"
#include "platform/import-payload.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace whilefresh::platform;

class ImportPayloadTest : public QObject
{
    Q_OBJECT

private:
    static QString writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
    {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(content);
        return file.fileName();
    }

private slots:
    void extensionFromMimeType_data()
    {
        QTest::addColumn<QString>("mimeType");
        QTest::addColumn<QString>("displayName");
        QTest::addColumn<QString>("extension");
        QTest::newRow("jpeg") << "image/jpeg" << "" << "jpg";
        QTest::newRow("mime wins over name") << "image/png" << "shot.jpg" << "png";
        QTest::newRow("upper case mime") << "IMAGE/WEBP" << "" << "webp";
        QTest::newRow("legacy jpg mime") << "image/jpg" << "" << "jpg";
        QTest::newRow("heic") << "image/heic" << "" << "heic";
        QTest::newRow("wildcard falls back to name") << "image/*" << "Screenshot_20261020.PNG" << "png";
        QTest::newRow("jpeg suffix normalized") << "" << "receipt.jpeg" << "jpg";
        QTest::newRow("path-like name only gives suffix") << "" << "../../evil.png" << "png";
        QTest::newRow("odd suffix rejected") << "" << "file.tar.gz-x" << "img";
        QTest::newRow("nothing known") << "" << "" << "img";
    }

    void extensionFromMimeType()
    {
        QFETCH(QString, mimeType);
        QFETCH(QString, displayName);
        QFETCH(QString, extension);
        QCOMPARE(imageExtension(mimeType, displayName), extension);
    }

    void finalizeRenamesAndResolvesDate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        StagedImage staged;
        staged.path = writeFile(dir, QStringLiteral("abc.part"), QByteArray("not empty"));
        staged.mimeType = QStringLiteral("image/*");
        staged.displayName = QStringLiteral("Screenshot.png");
        staged.hints.mediaDateAddedMs = QDateTime(QDate(2026, 10, 20), QTime(9, 0), QTimeZone::UTC).toMSecsSinceEpoch();

        QString error;
        const auto image = finalizeStagedImage(staged, QDateTime::currentDateTimeUtc().addYears(1), &error);
        QVERIFY2(image.has_value(), qPrintable(error));
        QCOMPARE(image->localPath, dir.filePath(QStringLiteral("abc.png")));
        QVERIFY(QFileInfo::exists(image->localPath));
        QVERIFY(!QFileInfo::exists(staged.path));
        QCOMPARE(image->mimeType, QStringLiteral("image/png"));
        QCOMPARE(image->capturedAtSource, ImportedImage::DateSource::MediaAdded);
    }

    void finalizeRejectsEmptyFiles()
    {
        QTemporaryDir dir;
        StagedImage staged;
        staged.path = writeFile(dir, QStringLiteral("empty.part"), QByteArray());
        QString error;
        QVERIFY(!finalizeStagedImage(staged, QDateTime::currentDateTime(), &error).has_value());
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(staged.path));
    }

    void parsesImageImport()
    {
        const QByteArray json = R"({"event":"import","origin":"share","kind":"images",
            "images":[{"path":"/data/files/imports/a.part","mimeType":"image/png","displayName":"a.png",
                       "mediaDateTakenMs":1792051200000,"mediaDateAddedMs":1792051300000},
                      {"path":"/data/files/imports/b.part","mimeType":"image/jpeg",
                       "exifDateTimeOriginal":"2026:10:20 08:30:00","exifOffsetTimeOriginal":"+08:00"}],
            "failures":["不是图片"]})";
        QString error;
        const auto event = parseBridgeEvent(json, &error);
        QVERIFY2(event.has_value(), qPrintable(error));
        QCOMPARE(event->type, BridgeEvent::Type::Import);
        QCOMPARE(event->origin, ImportPayload::Origin::Share);
        QCOMPARE(event->kind, ImportPayload::Kind::Images);
        QCOMPARE(event->images.size(), 2);
        QCOMPARE(event->images[0].displayName, QStringLiteral("a.png"));
        QCOMPARE(event->images[0].hints.mediaDateTakenMs, Q_INT64_C(1792051200000));
        QCOMPARE(event->images[0].hints.fileModifiedMs, Q_INT64_C(-1));
        QCOMPARE(event->images[1].hints.exifOffsetTimeOriginal, QStringLiteral("+08:00"));
        QCOMPARE(event->images[1].hints.cameraCaptureMs, Q_INT64_C(-1));
        QCOMPARE(event->failures, QStringList { QStringLiteral("不是图片") });
    }

    void parsesTextCancelAndFailure()
    {
        QString error;
        auto event = parseBridgeEvent(R"({"event":"import","origin":"share","kind":"text","text":"鸡蛋 🥚"})", &error);
        QVERIFY(event.has_value());
        QCOMPARE(event->kind, ImportPayload::Kind::Text);
        QCOMPARE(event->text, QStringLiteral("鸡蛋 🥚"));

        event = parseBridgeEvent(R"({"event":"cancelled","origin":"camera"})", &error);
        QVERIFY(event.has_value());
        QCOMPARE(event->type, BridgeEvent::Type::Cancelled);
        QCOMPARE(event->origin, ImportPayload::Origin::Camera);

        event = parseBridgeEvent(R"({"event":"failed","origin":"gallery","message":"没有可用的相册"})", &error);
        QVERIFY(event.has_value());
        QCOMPARE(event->type, BridgeEvent::Type::Failed);
        QCOMPARE(event->message, QStringLiteral("没有可用的相册"));
    }

    void rejectsMalformedMessages_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("not json") << QByteArray("{");
        QTest::newRow("array") << QByteArray("[]");
        QTest::newRow("unknown origin") << QByteArray(R"({"event":"cancelled","origin":"usb"})");
        QTest::newRow("unknown event") << QByteArray(R"({"event":"ping","origin":"share"})");
        QTest::newRow("unknown kind") << QByteArray(R"({"event":"import","origin":"share","kind":"video"})");
        QTest::newRow("image without path") << QByteArray(R"({"event":"import","origin":"share","kind":"images","images":[{}]})");
    }

    void rejectsMalformedMessages()
    {
        QFETCH(QByteArray, json);
        QString error;
        QVERIFY(!parseBridgeEvent(json, &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    void logLineOmitsTextAndPaths()
    {
        ImportPayload text;
        text.kind = ImportPayload::Kind::Text;
        text.origin = ImportPayload::Origin::Share;
        text.text = QStringLiteral("收货地址：某某路 1 号");
        QCOMPARE(text.logLine(), QStringLiteral("kind=text origin=share chars=%1").arg(text.text.size()));
        QVERIFY(text.summary().contains(QStringLiteral("收货地址")));

        ImportPayload images;
        images.origin = ImportPayload::Origin::Gallery;
        ImportedImage a;
        a.localPath = QStringLiteral("/secret/a.png");
        a.capturedAtSource = ImportedImage::DateSource::Exif;
        ImportedImage b;
        b.localPath = QStringLiteral("/secret/b.png");
        images.images = { a, b };
        QCOMPARE(images.logLine(), QStringLiteral("kind=images origin=gallery count=2 capturedAtSource=exif,none"));
    }

    void gadgetPropertiesAreReadable()
    {
        ImportPayload payload;
        payload.kind = ImportPayload::Kind::Text;
        payload.text = QStringLiteral("x");
        const QMetaObject &meta = ImportPayload::staticMetaObject;
        const QVariant summary = meta.property(meta.indexOfProperty("summary")).readOnGadget(&payload);
        QVERIFY(summary.toString().startsWith(QStringLiteral("text · gallery")));
    }
};

QTEST_GUILESS_MAIN(ImportPayloadTest)

#include "import-payload-test.moc"
