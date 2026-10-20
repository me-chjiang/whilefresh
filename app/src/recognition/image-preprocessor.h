#pragma once

#include "recognition/recognition-types.h"

#include <QImage>

namespace whilefresh::recognition {

inline constexpr int kMaxImageWidth = 1300;
inline constexpr int kSliceHeight = 1500;
inline constexpr int kSliceOverlap = 200;
inline constexpr int kJpegQuality = 85;

// Reads an image applying its EXIF orientation. Null image (and *error set) on failure.
QImage loadImage(const QString &path, QString *error = nullptr);

// Scales down proportionally when wider than kMaxImageWidth; never scales up.
QImage limitWidth(const QImage &image);

struct ImageSlice {
    QImage image;
    int top = 0; // y offset in the source image
};

// Images up to kSliceHeight tall are returned whole. Taller ones are cut into windows of
// kSliceHeight rows, each starting kSliceOverlap rows above the end of the previous one.
QList<ImageSlice> sliceImage(const QImage &image);

// JPEG at kJpegQuality; transparency is flattened onto white.
QByteArray encodeJpeg(const QImage &image);

struct PreparedImage {
    int imageIndex = 0;  // 0-based index of the source image in the request
    int sliceIndex = 0;  // 0-based
    int sliceCount = 1;
    QDate captureDate;
    QByteArray jpeg;
};

// load + limitWidth + slice + encode for every input. All slices of one source image are
// adjacent and in top-to-bottom order. Fails on the first unreadable image.
bool prepareImages(const QList<ImageInput> &inputs, QList<PreparedImage> *out, QString *error);

struct BatchLimits {
    int maxImages = 40;
    qint64 maxBytes = 40LL * 1024 * 1024; // of the request body (base64 inflates JPEG by 4/3)
};

// Splits the prepared slices into requests. Slices of one source image stay together unless
// the image alone exceeds the limits.
QList<QList<PreparedImage>> planBatches(const QList<PreparedImage> &images,
                                        const BatchLimits &limits = {});

}
