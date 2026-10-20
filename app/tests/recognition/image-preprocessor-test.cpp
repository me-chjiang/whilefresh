#include "recognition/image-preprocessor.h"

#include <QBuffer>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::recognition;

namespace {

// Row y has a colour that encodes y, so slices can be matched against the source.
QImage gradient(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        const QRgb color = qRgb(y & 0xFF, (y >> 8) & 0xFF, 0x40);
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, color);
    }
    return image;
}

// Inserts an EXIF APP1 segment carrying only the orientation tag right after the SOI marker.
QByteArray withExifOrientation(const QByteArray &jpeg, quint16 orientation)
{
    QByteArray exif;
    exif.append("\xFF\xE1\x00\x22" "Exif\0\0", 10);
    exif.append("MM\x00\x2A\x00\x00\x00\x08", 8);
    exif.append("\x00\x01", 2);
    exif.append("\x01\x12\x00\x03\x00\x00\x00\x01", 8);
    exif.append(char(orientation >> 8)).append(char(orientation & 0xFF)).append("\x00\x00", 2);
    exif.append("\x00\x00\x00\x00", 4);
    return jpeg.left(2) + exif + jpeg.mid(2);
}

QByteArray jpegBytes(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter(&buffer, "JPEG").write(image);
    return bytes;
}

PreparedImage fakeSlice(int imageIndex, int sliceIndex, int sliceCount, int bytes = 10)
{
    PreparedImage image;
    image.imageIndex = imageIndex;
    image.sliceIndex = sliceIndex;
    image.sliceCount = sliceCount;
    image.jpeg = QByteArray(bytes, 'x');
    return image;
}

QList<PreparedImage> fakeImages(const QList<int> &slicesPerImage)
{
    QList<PreparedImage> images;
    for (int i = 0; i < slicesPerImage.size(); ++i) {
        for (int s = 0; s < slicesPerImage.at(i); ++s)
            images.append(fakeSlice(i, s, slicesPerImage.at(i)));
    }
    return images;
}

QList<int> batchSizes(const QList<QList<PreparedImage>> &batches)
{
    QList<int> sizes;
    for (const auto &batch : batches)
        sizes.append(int(batch.size()));
    return sizes;
}

}

class ImagePreprocessorTest : public QObject
{
    Q_OBJECT

private slots:
    void sliceBoundaries_data()
    {
        QTest::addColumn<int>("height");
        QTest::addColumn<QList<int>>("tops");
        QTest::addColumn<QList<int>>("heights");

        QTest::newRow("tiny") << 1 << QList<int>{ 0 } << QList<int>{ 1 };
        QTest::newRow("exactly 1500") << 1500 << QList<int>{ 0 } << QList<int>{ 1500 };
        QTest::newRow("1501") << 1501 << QList<int>{ 0, 1300 } << QList<int>{ 1500, 201 };
        QTest::newRow("two full windows") << 2800 << QList<int>{ 0, 1300 } << QList<int>{ 1500, 1500 };
        QTest::newRow("2801") << 2801 << QList<int>{ 0, 1300, 2600 } << QList<int>{ 1500, 1500, 201 };
        QTest::newRow("long screenshot 6993")
            << 6993 << QList<int>{ 0, 1300, 2600, 3900, 5200, 6500 }
            << QList<int>{ 1500, 1500, 1500, 1500, 1500, 493 };
    }

    void sliceBoundaries()
    {
        QFETCH(int, height);
        QFETCH(QList<int>, tops);
        QFETCH(QList<int>, heights);

        const QImage source = gradient(40, height);
        const QList<ImageSlice> slices = sliceImage(source);
        QCOMPARE(slices.size(), tops.size());
        for (int i = 0; i < slices.size(); ++i) {
            QCOMPARE(slices.at(i).top, tops.at(i));
            QCOMPARE(slices.at(i).image.height(), heights.at(i));
            QCOMPARE(slices.at(i).image.width(), 40);
            // The slice shows exactly the source rows it claims to.
            QCOMPARE(slices.at(i).image.pixel(0, 0), source.pixel(0, tops.at(i)));
            const int last = heights.at(i) - 1;
            QCOMPARE(slices.at(i).image.pixel(39, last), source.pixel(39, tops.at(i) + last));
        }
        QCOMPARE(slices.last().top + slices.last().image.height(), height);
    }

    void consecutiveSlicesOverlapBy200Rows()
    {
        const QList<ImageSlice> slices = sliceImage(gradient(10, 4000));
        QVERIFY(slices.size() >= 3);
        for (int i = 1; i < slices.size(); ++i) {
            const ImageSlice &previous = slices.at(i - 1);
            const int previousBottom = previous.top + previous.image.height();
            QCOMPARE(previousBottom - slices.at(i).top, kSliceOverlap);
            // The overlapping rows are identical in both slices.
            QCOMPARE(slices.at(i).image.pixel(5, 0),
                     previous.image.pixel(5, previous.image.height() - kSliceOverlap));
        }
    }

    void widthIsCappedProportionally()
    {
        QCOMPARE(limitWidth(gradient(1300, 2000)).size(), QSize(1300, 2000));
        QCOMPARE(limitWidth(gradient(1000, 2000)).size(), QSize(1000, 2000));
        QCOMPARE(limitWidth(gradient(1301, 1301)).size(), QSize(1300, 1300));
        QCOMPARE(limitWidth(gradient(2600, 3000)).size(), QSize(1300, 1500));
    }

    void wideAndTallImageIsScaledThenSliced()
    {
        const QList<ImageSlice> slices = sliceImage(limitWidth(gradient(2600, 7000)));
        QCOMPARE(slices.first().image.width(), 1300);
        QCOMPARE(slices.last().top + slices.last().image.height(), 3500);
        QCOMPARE(slices.size(), 3);
    }

    void exifOrientationIsApplied()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QByteArray plain = jpegBytes(gradient(300, 100));

        const QString plainPath = dir.filePath(QStringLiteral("plain.jpg"));
        const QString rotatedPath = dir.filePath(QStringLiteral("rotated.jpg"));
        QFile plainFile(plainPath);
        QVERIFY(plainFile.open(QIODevice::WriteOnly));
        plainFile.write(plain);
        plainFile.close();
        QFile rotatedFile(rotatedPath);
        QVERIFY(rotatedFile.open(QIODevice::WriteOnly));
        rotatedFile.write(withExifOrientation(plain, 6)); // 6 = rotate 90 degrees clockwise
        rotatedFile.close();

        QCOMPARE(loadImage(plainPath).size(), QSize(300, 100));
        QCOMPARE(loadImage(rotatedPath).size(), QSize(100, 300));
    }

    void unreadableFileReportsError()
    {
        QString error;
        QVERIFY(loadImage(QStringLiteral("/nonexistent/none.jpg"), &error).isNull());
        QVERIFY(!error.isEmpty());
    }

    void jpegEncodingFlattensTransparencyOnWhite()
    {
        QImage image(20, 20, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        const QByteArray jpeg = encodeJpeg(image);
        QVERIFY(jpeg.startsWith("\xFF\xD8"));

        QImage decoded;
        QVERIFY(decoded.loadFromData(jpeg, "JPEG"));
        QCOMPARE(decoded.size(), QSize(20, 20));
        const QColor pixel = decoded.pixelColor(10, 10);
        QVERIFY(pixel.red() > 240 && pixel.green() > 240 && pixel.blue() > 240);
    }

    void prepareImagesSlicesEachSourceInOrder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString tall = dir.filePath(QStringLiteral("tall.png"));
        const QString small = dir.filePath(QStringLiteral("small.png"));
        QVERIFY(gradient(1100, 3000).save(tall));
        QVERIFY(gradient(600, 800).save(small));

        QList<PreparedImage> prepared;
        QString error;
        QVERIFY2(prepareImages({ { tall, QDate(2026, 10, 18) }, { small, QDate() } }, &prepared, &error),
                 qPrintable(error));
        QCOMPARE(prepared.size(), 4);
        QCOMPARE(prepared.at(0).imageIndex, 0);
        QCOMPARE(prepared.at(0).sliceIndex, 0);
        QCOMPARE(prepared.at(0).sliceCount, 3);
        QCOMPARE(prepared.at(2).sliceIndex, 2);
        QCOMPARE(prepared.at(0).captureDate, QDate(2026, 10, 18));
        QCOMPARE(prepared.at(3).imageIndex, 1);
        QCOMPARE(prepared.at(3).sliceCount, 1);
        QVERIFY(!prepared.at(3).captureDate.isValid());
        for (const PreparedImage &image : prepared)
            QVERIFY(image.jpeg.startsWith("\xFF\xD8"));
    }

    void prepareImagesFailsOnUnreadableFile()
    {
        QList<PreparedImage> prepared;
        QString error;
        QVERIFY(!prepareImages({ { QStringLiteral("/nonexistent/none.jpg"), {} } }, &prepared, &error));
        QVERIFY(error.contains(QStringLiteral("#1")));
    }

    void batchesSplitAtFortyImages()
    {
        QCOMPARE(batchSizes(planBatches(fakeImages(QList<int>(40, 1)))), QList<int>{ 40 });
        QCOMPARE(batchSizes(planBatches(fakeImages(QList<int>(41, 1)))), (QList<int>{ 40, 1 }));
        QCOMPARE(batchSizes(planBatches(fakeImages(QList<int>(85, 1)))), (QList<int>{ 40, 40, 5 }));
    }

    void batchesKeepOneImagesSlicesTogether()
    {
        BatchLimits limits;
        limits.maxImages = 4;
        QCOMPARE(batchSizes(planBatches(fakeImages({ 3, 3, 1 }), limits)), (QList<int>{ 3, 4 }));
    }

    void batchesSplitAnImageThatAloneIsTooBig()
    {
        BatchLimits limits;
        limits.maxImages = 2;
        QCOMPARE(batchSizes(planBatches(fakeImages({ 1, 5 }), limits)), (QList<int>{ 1, 2, 2, 1 }));
    }

    void batchesRespectByteLimit()
    {
        BatchLimits limits;
        limits.maxBytes = 3 * (10 * 4 / 3 + 128 + 1);
        QCOMPARE(batchSizes(planBatches(fakeImages(QList<int>(7, 1)), limits)), (QList<int>{ 3, 3, 1 }));
    }

    void noImagesNoBatches()
    {
        QVERIFY(planBatches({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(ImagePreprocessorTest)

#include "image-preprocessor-test.moc"
