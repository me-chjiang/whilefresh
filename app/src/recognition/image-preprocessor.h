#pragma once

#include "recognition/recognition-types.h"

#include <QImage>

#include <functional>

namespace whilefresh::recognition {

inline constexpr int kMaxImageWidth = 1300;
inline constexpr int kSliceHeight = 1500;
inline constexpr int kSliceOverlap = 200;
inline constexpr int kJpegQuality = 85;

// Reads an image applying its EXIF orientation, shrinking it already while decoding so that
// the result is at most kMaxImageWidth wide (a 12000x9000 photo is never held at full size).
// Null image (and *error set) on failure.
QImage loadImage(const QString &path, QString *error = nullptr);

// Whether JPEG encoding is available (the qjpeg image plugin is deployed).
bool jpegEncodingAvailable();

// Scales down proportionally when wider than kMaxImageWidth; never scales up.
QImage limitWidth(const QImage &image);

struct ImageSlice {
    QImage image;
    int top = 0; // y offset in the source image
};

// Top offsets of the slices of an image `height` rows tall.
QList<int> sliceTops(int height);

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

// load + slice + encode for every input, one slice at a time. All slices of one source image
// are adjacent and in top-to-bottom order. Fails on the first unreadable image. `isCanceled`
// is polled between slices; when it returns true this stops and returns false with an empty
// error.
bool prepareImages(const QList<ImageInput> &inputs, QList<PreparedImage> *out, QString *error,
                   const std::function<bool()> &isCanceled = {});

struct BatchLimits {
    int maxImages = 40;
    qint64 maxBytes = 40LL * 1024 * 1024; // of the request body (base64 inflates JPEG by 4/3)
};

// Splits the prepared slices into requests. Slices of one source image stay together unless
// the image alone exceeds the limits.
QList<QList<PreparedImage>> planBatches(const QList<PreparedImage> &images,
                                        const BatchLimits &limits = {});

}
