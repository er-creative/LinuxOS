#pragma once
// F3E-F1: OFFLINE forward-return labels. NEVER expose realized labels to live policy.
// Standalone adapter: supply timestamped 1-minute CLOSE prices for one symbol.
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace devai::strategy::f3ef1 {
struct MinuteClose {
    std::int64_t epoch_seconds{}; // absolute Unix seconds, candle START timestamp
    double close{};
    int session_id{}; // YYYYMMDD in Asia/Kolkata, computed by upstream loader
};
struct HorizonLabel {
    int minutes{};
    bool valid{};
    double simple_return{}; // fractional, e.g. 0.01 == +1%; NOT percent
};
struct LabelRow {
    std::int64_t epoch_seconds{};
    int session_id{};
    std::array<HorizonLabel,5> horizons{};
};
inline constexpr std::array<int,5> kHorizons{1,3,5,10,15};

// Sorted, unique, exactly spaced minute bars required. Missing minutes invalidate
// the horizon, rather than silently interpreting "10 bars" as "10 minutes".
// No crossing sessions; no extrapolation; no future candle -> invalid label.
inline std::vector<LabelRow> make_forward_return_labels(
    const std::vector<MinuteClose>& bars) {
    for (std::size_t i=0; i<bars.size(); ++i) {
        if (!std::isfinite(bars[i].close) || bars[i].close <= 0.0)
            throw std::invalid_argument("F3E-F1: close must be finite and positive");
        if (i && bars[i].epoch_seconds <= bars[i-1].epoch_seconds)
            throw std::invalid_argument("F3E-F1: timestamps must be strictly increasing");
    }
    std::vector<LabelRow> rows;
    rows.reserve(bars.size());
    for (std::size_t i=0; i<bars.size(); ++i) {
        LabelRow row{};
        row.epoch_seconds=bars[i].epoch_seconds;
        row.session_id=bars[i].session_id;
        for (std::size_t h=0; h<kHorizons.size(); ++h) {
            const int minutes=kHorizons[h];
            auto& out=row.horizons[h];
            out.minutes=minutes;
            const auto j=i+static_cast<std::size_t>(minutes);
            if (j >= bars.size() || bars[j].session_id != bars[i].session_id) continue;
            bool contiguous=true;
            for (std::size_t k=i+1; k<=j; ++k) {
                if (bars[k].session_id != bars[i].session_id ||
                    bars[k].epoch_seconds-bars[k-1].epoch_seconds != 60) {
                    contiguous=false; break;
                }
            }
            if (!contiguous) continue;
            out.valid=true;
            out.simple_return=bars[j].close/bars[i].close-1.0;
        }
        rows.push_back(row);
    }
    return rows;
}
} // namespace devai::strategy::f3ef1
