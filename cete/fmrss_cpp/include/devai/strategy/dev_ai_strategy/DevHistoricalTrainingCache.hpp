#pragma once

#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace devai::strategy
{

inline constexpr std::uint32_t DEV_HISTORICAL_CACHE_FORMAT_VERSION = 4U;

// Offline-only realized target. This type is deliberately separate from
// DevAIObservation so future information cannot enter live inference state.
struct DevHistoricalOutcome
{
    std::size_t horizon_bars{0};
    std::int64_t outcome_time{0};
    double forward_return{0.0};
    bool available{false};
    bool numerical_integrity{false};
};

struct DevHistoricalCandidateRecord
{
    std::size_t slot_index{0};
    std::string agent_name;
    std::string symbol;
    double candidate_score{0.0};
    std::size_t candidate_rank{0};
    DevAIObservation observation;
    std::vector<DevHistoricalOutcome> outcomes;
    bool numerical_integrity{false};
};

struct DevHistoricalDecisionRecord
{
    std::int64_t decision_time{0};
    std::string benchmark_symbol;
    std::size_t universe_stock_count{0};
    std::vector<DevHistoricalCandidateRecord> candidates; // 0..5, contiguous slots; slot 4 maps to rank 5
    bool numerical_integrity{false};
};

struct DevHistoricalTrainingDataset
{
    std::uint32_t format_version{DEV_HISTORICAL_CACHE_FORMAT_VERSION};
    std::vector<DevHistoricalDecisionRecord> records;
    bool numerical_integrity{false};
};

class DevHistoricalTrainingDatasetValidator
{
public:
    static void validate(const DevHistoricalTrainingDataset& dataset);
    static void validateRecord(const DevHistoricalDecisionRecord& record);
    static void validateCandidate(
        const DevHistoricalCandidateRecord& candidate,
        std::int64_t expected_decision_time,
        const std::string& expected_benchmark,
        std::size_t expected_slot);
};

class DevHistoricalTrainingCache
{
public:
    // Binary cache. Atomic publish uses <path>.tmp then rename.
    static void save(
        const std::filesystem::path& path,
        const DevHistoricalTrainingDataset& dataset);

    [[nodiscard]]
    static DevHistoricalTrainingDataset load(
        const std::filesystem::path& path);
};

} // namespace devai::strategy
