#pragma once

#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace whilefresh::core {

// What an earlier import contained, for spotting a repeated import of the same purchase.
struct ImportFingerprint {
    qint64 importId = 0;
    QDateTime createdAt;
    QStringList imageSha256;
    QList<QPair<QString, QString>> items; // standard name, quantity text
};

struct DuplicateRules {
    static constexpr double minimumOverlap = 0.8; // share of matching name + quantity pairs
    static constexpr int withinDays = 3;
    static constexpr int minimumItems = 2; // a single shared item says nothing
};

struct DuplicateCheck {
    enum class Kind { None, SameImage, SimilarItems };
    Kind kind = Kind::None;
    qint64 importId = 0;
    QDateTime importedAt;
    double overlap = 0.0;

    bool isDuplicate() const { return kind != Kind::None; }
    QString message() const;
};

DuplicateCheck checkSameImage(const QStringList &imageSha256, const QList<ImportFingerprint> &previous);

// Overlap = matching (name, quantity) pairs / size of the larger set; counted as a possible
// duplicate at >= 80% when the earlier import is at most 3 days old.
DuplicateCheck checkSimilarItems(const QList<QPair<QString, QString>> &items, const QDateTime &now,
                                 const QList<ImportFingerprint> &previous);

}
