#pragma once
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace devai::strategy {
struct DevHistoricalMinuteInput {
    std::string symbol;
    std::string file_path;
};
struct DevHistoricalMinuteValidationConfig {
    std::string benchmark_symbol{"NIFTY 50"};
    int session_start_minute{9 * 60 + 15};
    int session_end_minute{15 * 60 + 30}; // exclusive
    bool reject_out_of_session{true};
    // Explicit YYYY-MM-DD -> [start minute, exclusive end minute].
    // No automatic inference of special sessions from incomplete files.
    std::map<std::string, std::pair<int,int>> session_overrides{
        {"2025-10-21", {13 * 60 + 45, 14 * 60 + 45}}
    };
};
struct DevHistoricalSymbolValidation {
    std::string symbol;
    std::size_t rows{0};
    std::size_t duplicate_minutes{0};
    std::size_t invalid_rows{0};
    std::size_t missing_session_minutes{0};
    std::size_t partial_sessions{0};
    std::size_t complete_sessions{0};
    std::size_t complete_5m_windows{0};
    std::size_t complete_15m_windows{0};
};
struct DevHistoricalMinuteValidationReport {
    bool passed{false}; // integrity + at least one synchronized completed window of each timeframe
    bool all_sessions_complete{false};
    std::vector<DevHistoricalSymbolValidation> symbols;
    std::size_t synchronized_5m_windows{0};
    std::size_t synchronized_15m_windows{0};
    std::vector<std::string> issues;
};
class DevHistoricalMinuteValidator final {
public:
    explicit DevHistoricalMinuteValidator(DevHistoricalMinuteValidationConfig config = {});
    [[nodiscard]] DevHistoricalMinuteValidationReport validate(
        const std::vector<DevHistoricalMinuteInput>& inputs) const;
private:
    DevHistoricalMinuteValidationConfig config_;
};
} // namespace devai::strategy
