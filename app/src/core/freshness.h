#pragma once

#include <QDate>
#include <QString>

#include <optional>

namespace whilefresh::core {

enum class Freshness { Unknown, Fresh, NearExpiry, BeyondEstimate };

QString freshnessKey(Freshness freshness);
QString freshnessLabel(Freshness freshness); // texts::freshness*()

// All thresholds in one place (ADR-7); tune here after the trial.
struct FreshnessThresholds {
    // remaining <= beyondAtOrBelow -> beyond the estimate
    static constexpr int beyondAtOrBelow = 0;
    // remaining <= max(nearMinimumDays, total * nearFraction) -> near expiry
    static constexpr double nearFraction = 0.25;
    static constexpr int nearMinimumDays = 1;
};

struct FreshnessState {
    Freshness freshness = Freshness::Unknown;
    std::optional<int> remainingDays; // today -> expiry; negative once past
};

// Never stored; computed from the injected clock's "today" each time.
FreshnessState evaluateFreshness(const QDate &expiryDate, std::optional<int> totalDays, const QDate &today);

}
