#include "core/import-duplicates.h"

#include "core/texts.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cstdlib>

namespace whilefresh::core {

namespace {

QHash<QString, int> countPairs(const QList<QPair<QString, QString>> &items)
{
    QHash<QString, int> counts;
    for (const auto &[name, quantity] : items) {
        if (!name.trimmed().isEmpty())
            ++counts[name.trimmed() + QChar(0x1f) + quantity.trimmed()];
    }
    return counts;
}

int total(const QHash<QString, int> &counts)
{
    int sum = 0;
    for (int count : counts)
        sum += count;
    return sum;
}

}

QString DuplicateCheck::message() const
{
    switch (kind) {
    case Kind::None:
        return {};
    case Kind::SameImage:
        return texts::importDuplicateImage();
    case Kind::SimilarItems:
        return texts::importPossibleDuplicate();
    }
    return {};
}

DuplicateCheck checkSameImage(const QStringList &imageSha256, const QList<ImportFingerprint> &previous)
{
    const QSet<QString> wanted(imageSha256.begin(), imageSha256.end());
    for (const ImportFingerprint &earlier : previous) {
        for (const QString &sha : earlier.imageSha256) {
            if (!sha.isEmpty() && wanted.contains(sha))
                return { DuplicateCheck::Kind::SameImage, earlier.importId, earlier.createdAt, 1.0 };
        }
    }
    return {};
}

DuplicateCheck checkSimilarItems(const QList<QPair<QString, QString>> &items, const QDateTime &now,
                                 const QList<ImportFingerprint> &previous)
{
    const QHash<QString, int> current = countPairs(items);
    const int currentTotal = total(current);
    if (currentTotal < DuplicateRules::minimumItems)
        return {};
    DuplicateCheck best;
    for (const ImportFingerprint &earlier : previous) {
        if (!earlier.createdAt.isValid() || std::abs(earlier.createdAt.daysTo(now)) > DuplicateRules::withinDays)
            continue;
        const QHash<QString, int> other = countPairs(earlier.items);
        int common = 0;
        for (auto it = current.begin(); it != current.end(); ++it)
            common += std::min(it.value(), other.value(it.key()));
        const double overlap = static_cast<double>(common) / std::max(currentTotal, total(other));
        if (overlap >= DuplicateRules::minimumOverlap && overlap > best.overlap)
            best = { DuplicateCheck::Kind::SimilarItems, earlier.importId, earlier.createdAt, overlap };
    }
    return best;
}

}
