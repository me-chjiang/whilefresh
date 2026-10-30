#pragma once

#include <QString>

namespace whilefresh::core {

// An amount read from a quantity / spec text. Mass is normalised to "g", volume to "ml"; count
// units ("盒", "个", "把", ...) are kept as written. value 0 and an empty unit mean unknown.
struct Amount {
    double value = 0.0;
    QString unit;

    bool isKnown() const { return value > 0 && !unit.isEmpty(); }
    bool operator==(const Amount &other) const { return value == other.value && unit == other.unit; }
};

// "500g" -> 500 g, "1.5kg" -> 1500 g, "约350克" -> 350 g, "500ml×2" -> 1000 ml, "2盒" -> 2 盒.
// The first measure (mass or volume) wins over count units; a number joined to it by ×, x or *
// multiplies it. A bare number without a unit is unknown.
Amount parseAmount(const QString &text);

// What one stock row holds in total: the parsed spec times the number of packages.
Amount totalAmount(const QString &quantityText, int count);

}
