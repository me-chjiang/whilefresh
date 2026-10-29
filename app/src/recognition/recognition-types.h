#pragma once

#include "ai/chat-types.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>

#include <memory>
#include <optional>

namespace whilefresh::recognition {

class Vocabulary;

// Transport errors and token counts are those of the chat client (ai layer).
using ErrorKind = ai::ErrorKind;
using TokenUsage = ai::TokenUsage;
using ai::enumKey;

// What the user handed in. Decides which prompt paragraph is used.
enum class InputKind { Screenshot, Receipt, OrderText };

// What the model says the material was.
enum class SourceType { Cart, Order, Receipt, Text, Other };

enum class ItemCategory { Food, Seasoning, Irrelevant };

enum class StorageKind { Unknown, Fridge, Freezer, Room };

QString enumKey(InputKind kind);
std::optional<InputKind> inputKindFromString(const QString &text);
QString enumKey(SourceType type);
std::optional<SourceType> sourceTypeFromString(const QString &text);
QString enumKey(ItemCategory category);
std::optional<ItemCategory> itemCategoryFromString(const QString &text);
QString enumKey(StorageKind storage);
std::optional<StorageKind> storageKindFromString(const QString &text);

// Fixed label for names the model could not map into the vocabulary.
QString uncategorizedName();

struct ShelfLifeDays {
    std::optional<int> fridge;
    std::optional<int> freezer;
    std::optional<int> room;
};

struct RecognizedItem {
    QString rawName;
    QString canonicalName; // in the vocabulary, or uncategorizedName()
    QString quantityText;
    int count = 1;
    ItemCategory category = ItemCategory::Food;
    bool selected = true;
    bool isGift = false;
    // Dates are verbatim text as visible in the material (e.g. "今天生产", "10月18日生产").
    // Empty means not visible. Relative-date resolution is done by core.
    QString productionDateText;
    QString expiryDateText;
    QString purchaseDateText;
    StorageKind storage = StorageKind::Unknown;
    ShelfLifeDays shelfLifeDays;
    // 0-based index into RecognitionRequest::images of the image the item was first seen in;
    // -1 when unknown (text input, or the model did not say).
    int imageIndex = -1;
};

// Which original images (0-based indices into RecognitionRequest::images) one request covered;
// -1 for text input.
struct BatchSpan {
    int firstImage = -1;
    int lastImage = -1;
};

struct RecognitionResult {
    SourceType sourceType = SourceType::Other;
    QList<RecognizedItem> items;
    QString ignoredSummary;
    // Raw chat-completion bodies, one per request sent; used for debugging and recording.
    QList<QByteArray> rawResponses;
    QList<BatchSpan> batchSpans; // parallel to rawResponses when known
    TokenUsage usage;
    qint64 elapsedMs = 0;
    bool isDemo = false;
    // The model ran out of output budget and the answer was cut off; the items are those that
    // arrived complete, so the list may be missing some.
    bool truncated = false;
};

struct RecognitionError {
    ErrorKind kind = ErrorKind::BadResponse;
    QString userMessage; // Chinese, no technical detail
    QString detail;      // for logs only
    int httpStatus = 0;
};

QString userMessageFor(ErrorKind kind);
RecognitionError makeError(ErrorKind kind, const QString &detail, int httpStatus = 0);

struct ImageInput {
    QString path;
    QDate captureDate; // optional; the day relative dates like "今天生产" refer to
};

struct RecognitionRequest {
    InputKind kind = InputKind::Screenshot;
    QList<ImageInput> images; // for Screenshot / Receipt
    QString text;             // for OrderText
    std::shared_ptr<const Vocabulary> vocabulary;
    // Resume support (R-ADR-9): skip the batches before this one. Batches are planned from the
    // same images and limits, so their numbering is stable across runs.
    int firstBatch = 0;
};

}
