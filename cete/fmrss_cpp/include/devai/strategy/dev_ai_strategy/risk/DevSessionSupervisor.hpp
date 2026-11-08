#pragma once
#include <cstdint>
#include <string>

namespace devai::strategy::session {

enum class Priority { NORMAL, REVIEW, ELEVATED, URGENT, MANDATORY };
enum class Direction { UP, DOWN, SIDEWAYS, UNKNOWN };
enum class Recommendation { HOLD_REVIEW, EXIT_RECOMMENDED, EXIT_REQUIRED };

struct Config {
    int entry_cutoff_minute_ist = 14 * 60 + 45;
    int liquidation_deadline_minute_ist = 15 * 60 + 15;
    int liquidation_buffer_minutes = 5; // start urgent exit handling before deadline
    int stagnation_review_minutes = 10;
    double sideways_range_fraction = 0.0015; // configurable; 0.15% range
    double minimum_expected_net_advantage_fraction = 0.0002;
};

struct PositionSnapshot {
    std::string symbol;
    std::int64_t decision_time_utc = 0;
    std::int64_t entry_time_utc = 0;
    bool invested = false;
    double entry_price = 0;
    double last_price = 0;
    double highest_price_since_entry = 0;
    double lowest_price_since_entry = 0;
    double stop_loss_price = 0; // >0 if configured
    // Causal forecasts produced from observations available at decision_time_utc.
    double expected_hold_net_advantage_fraction = 0;
    double estimated_downside_risk_fraction = 0;
    bool forecast_available = false;
};

struct Assessment {
    Priority priority = Priority::NORMAL;
    Direction direction = Direction::UNKNOWN;
    Recommendation recommendation = Recommendation::HOLD_REVIEW;
    int age_minutes = 0;
    int minutes_to_deadline = 0;
    bool stagnant = false;
    bool stop_triggered = false;
    std::string message;
};

int minuteIST(std::int64_t unix_seconds) noexcept;
bool entryAllowed(std::int64_t unix_seconds, const Config& cfg = {});
bool isNewPositionOrSwitch(const std::string& held, const std::string& desired);
bool targetAllowed(const std::string& held, const std::string& desired,
                   std::int64_t unix_seconds, const Config& cfg = {});
Assessment assess(const PositionSnapshot& p, const Config& cfg = {});
const char* toString(Priority p) noexcept;
const char* toString(Recommendation r) noexcept;

} // namespace devai::strategy::session
