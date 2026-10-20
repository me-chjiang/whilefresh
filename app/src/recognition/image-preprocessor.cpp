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

    // The scaled size is applied to the stored (unrotated) pixels, while the width limit is
    // about the image as displayed: a 90-degree EXIF rotation swaps the two.
    const QSize stored = reader.size();
    if (stored.isValid()) {
        const bool rotated = reader.transformation().testFlag(QImageIOHandler::TransformationRotate90);
        const int displayedWidth = rotated ? stored.height() : stored.width();
        if (displayedWidth > kMaxImageWidth) {
            const double scale = double(kMaxImageWidth) / displayedWidth;
            reader.setScaledSize(QSize(qMax(1, qRound(stored.width() * scale)), qMax(1, qRound(stored.height() * scale))));
        }
    }

    const QImage image = reader.read();
    if (image.isNull()) {
        if (error)
            *error = QStringLiteral("cannot read image: %1").arg(reader.errorString());
        return {};
    }
    return limitWidth(image); // for formats that ignore the scaled size
}

bool jpegEncodingAvailable()
{
    return QImageWriter::supportedImageFormats().contains("jpeg");
}

QImage limitWidth(const QImage &image)
{
    if (image.width() <= kMaxImageWidth)
        return image;
    const int height = qMax(1, qRound(double(image.height()) * kMaxImageWidth / image.width()));
    return image.scaled(kMaxImageWidth, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QList<int> sliceTops(int height)
{
    QList<int> tops;
    int top = 0;
    while (true) {
        tops.append(top);
        const int bottom = qMin(top + kSliceHeight, height);
        if (bottom >= height)
            break;
        top = bottom - kSliceOverlap;
    }
    return tops;
}

QList<ImageSlice> sliceImage(const QImage &image)
{
    QList<ImageSlice> slices;
    for (const int top : sliceTops(image.height()))
        slices.append({ image.copy(0, top, image.width(), qMin(kSliceHeight, image.height() - top)), top });
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

bool prepareImages(const QList<ImageInput> &inputs, QList<PreparedImage> *out, QString *error,
                   const std::function<bool()> &isCanceled)
{
    out->clear();
    const auto canceled = [&] { return isCanceled && isCanceled(); };

    for (int index = 0; index < inputs.size(); ++index) {
        if (canceled())
            return false;

        QString loadError;
        const QImage image = loadImage(inputs.at(index).path, &loadError);
        if (image.isNull()) {
            // Only the position is reported: paths can reveal personal folder names.
            if (error)
                *error = QStringLiteral("image #%1: %2").arg(index + 1).arg(loadError);
            return false;
        }

        // One slice is cropped and encoded at a time; only the JPEG bytes are kept.
        const QList<int> tops = sliceTops(image.height());
        for (int s = 0; s < tops.size(); ++s) {
            if (canceled())
                return false;

            PreparedImage prepared;
            prepared.imageIndex = index;
            prepared.sliceIndex = s;
            prepared.sliceCount = int(tops.size());
            prepared.captureDate = inputs.at(index).captureDate;
            prepared.jpeg = encodeJpeg(image.copy(0, tops.at(s), image.width(), qMin(kSliceHeight, image.height() - tops.at(s))));
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
