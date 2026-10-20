#include "recognition/image-preprocessor.h"

#include <QBuffer>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>

namespace whilefresh::recognition {

namespace {

qint64 encodedSize(const PreparedImage &image)
{
    // Base64 expansion plus the data-URL prefix and JSON framing.
    return (image.jpeg.size() * 4 + 2) / 3 + 128;
}

}

QImage loadImage(const QString &path, QString *error)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull() && error)
        *error = QStringLiteral("cannot read image: %1").arg(reader.errorString());
    return image;
}

QImage limitWidth(const QImage &image)
{
    if (image.width() <= kMaxImageWidth)
        return image;
    const int height = qMax(1, qRound(double(image.height()) * kMaxImageWidth / image.width()));
    return image.scaled(kMaxImageWidth, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QList<ImageSlice> sliceImage(const QImage &image)
{
    QList<ImageSlice> slices;
    const int height = image.height();
    int top = 0;
    while (true) {
        const int bottom = qMin(top + kSliceHeight, height);
        slices.append({ image.copy(0, top, image.width(), bottom - top), top });
        if (bottom == height)
            break;
        top = bottom - kSliceOverlap;
    }
    return slices;
}

QByteArray encodeJpeg(const QImage &image)
{
    QImage flat(image.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    {
        QPainter painter(&flat);
        painter.drawImage(0, 0, image);
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, "JPEG");
    writer.setQuality(kJpegQuality);
    if (!writer.write(flat))
        return {};
    return bytes;
}

bool prepareImages(const QList<ImageInput> &inputs, QList<PreparedImage> *out, QString *error)
{
    out->clear();
    for (int index = 0; index < inputs.size(); ++index) {
        QString loadError;
        const QImage image = loadImage(inputs.at(index).path, &loadError);
        if (image.isNull()) {
            // Only the position is reported: paths can reveal personal folder names.
            if (error)
                *error = QStringLiteral("image #%1: %2").arg(index + 1).arg(loadError);
            return false;
        }

        const QList<ImageSlice> slices = sliceImage(limitWidth(image));
        for (int s = 0; s < slices.size(); ++s) {
            PreparedImage prepared;
            prepared.imageIndex = index;
            prepared.sliceIndex = s;
            prepared.sliceCount = int(slices.size());
            prepared.captureDate = inputs.at(index).captureDate;
            prepared.jpeg = encodeJpeg(slices.at(s).image);
            if (prepared.jpeg.isEmpty()) {
                if (error)
                    *error = QStringLiteral("image #%1: JPEG encoding failed").arg(index + 1);
                return false;
            }
            out->append(prepared);
        }
    }
    return true;
}

QList<QList<PreparedImage>> planBatches(const QList<PreparedImage> &images, const BatchLimits &limits)
{
    QList<QList<PreparedImage>> batches;
    QList<PreparedImage> current;
    qint64 currentBytes = 0;

    const auto flush = [&] {
        if (!current.isEmpty())
            batches.append(current);
        current.clear();
        currentBytes = 0;
    };
    const auto fits = [&](qsizetype count, qint64 bytes) {
        return current.size() + count <= limits.maxImages && currentBytes + bytes <= limits.maxBytes;
    };
    const auto add = [&](const PreparedImage &image) {
        if (!fits(1, encodedSize(image)))
            flush();
        current.append(image);
        currentBytes += encodedSize(image);
    };

    qsizetype i = 0;
    while (i < images.size()) {
        qsizetype end = i;
        qint64 groupBytes = 0;
        while (end < images.size() && images.at(end).imageIndex == images.at(i).imageIndex) {
            groupBytes += encodedSize(images.at(end));
            ++end;
        }
        const qsizetype groupCount = end - i;

        if (!fits(groupCount, groupBytes))
            flush();
        if (fits(groupCount, groupBytes)) {
            for (qsizetype k = i; k < end; ++k) {
                current.append(images.at(k));
                currentBytes += encodedSize(images.at(k));
            }
        } else {
            // The image alone is over the limits: cut it between slices.
            for (qsizetype k = i; k < end; ++k)
                add(images.at(k));
        }
        i = end;
    }
    flush();
    return batches;
}

}
