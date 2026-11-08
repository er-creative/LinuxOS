#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace devai::readiness {
struct Candidate {
    std::string symbol;
    std::string agent_id;
    std::size_t slot{};
    bool causal_observation{false};
    bool forecast_from_past_only{false};
    bool has_realized_forward_return_in_observation{false};
};
struct TrainingDecision {
    std::int64_t decision_time{};
    std::int64_t next_decision_time{};
    std::string manager_id;
    std::vector<Candidate> candidates;
    std::string held_symbol;
    bool held_position_visible{false};
    bool manager_action_valid{false};
    bool supervisor_authorized{false};
    bool terminal{false};
    double initial_equity{};
    double equity_before{};
    double equity_after{};
    double manager_reward{};
};
struct Report {
    bool ready{false};
    std::size_t checked_decisions{};
    std::vector<std::string> errors;
};
Report validate(const std::vector<TrainingDecision>& decisions,
                std::int64_t training_end_exclusive,
                double reward_tolerance = 1e-10);
} // namespace devai::readiness
