#include "core/freshness.h"

#include "core/texts.h"

#include <algorithm>

namespace whilefresh::core {

QString freshnessKey(Freshness freshness)
{
    switch (freshness) {
    case Freshness::Unknown:
        return QStringLiteral("unknown");
    case Freshness::Fresh:
        return QStringLiteral("fresh");
    case Freshness::NearExpiry:
        return QStringLiteral("near_expiry");
    case Freshness::BeyondEstimate:
        return QStringLiteral("beyond_estimate");
    }
    return QStringLiteral("unknown");
}

QString freshnessLabel(Freshness freshness)
{
    switch (freshness) {
    case Freshness::Unknown:
        return texts::freshnessUnknown();
    case Freshness::Fresh:
        return texts::freshnessFresh();
    case Freshness::NearExpiry:
        return texts::freshnessNearExpiry();
    case Freshness::BeyondEstimate:
        return texts::freshnessBeyond();
    }
    return texts::freshnessUnknown();
}

FreshnessState evaluateFreshness(const QDate &expiryDate, std::optional<int> totalDays, const QDate &today)
{
    if (!expiryDate.isValid() || !today.isValid())
        return {};
    const int remaining = static_cast<int>(today.daysTo(expiryDate));
    FreshnessState state{ Freshness::Fresh, remaining };
    const double nearLimit = std::max(static_cast<double>(FreshnessThresholds::nearMinimumDays),
                                      totalDays.value_or(0) * FreshnessThresholds::nearFraction);
    if (remaining <= FreshnessThresholds::beyondAtOrBelow)
        state.freshness = Freshness::BeyondEstimate;
    else if (remaining <= nearLimit)
        state.freshness = Freshness::NearExpiry;
    return state;
}

}
