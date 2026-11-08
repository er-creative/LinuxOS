#pragma once
#include "devai/strategy/dev_ai_strategy/DevManagerRL_D9F.hpp"
#include <cstdint>
#include <vector>

namespace devai::strategy { struct DevHistoricalExperience; }

namespace devai::d9f4c {
enum class RewardMode { GROSS_MARKET_REWARD, NET_EXECUTION_REWARD };

// These costs must be sourced from executed fills in the SAME transition interval.
// They are diagnostic for NET mode and are added back ONCE for GROSS mode.
struct ActualExecutionCosts {
    double brokerage_and_statutory{0.0};
    double slippage{0.0};
    bool reconciled_to_equity{false}; // true only after reconciliation with the execution ledger
};

struct TransitionInput {
    devai::strategy::d9f::State state{};
    devai::strategy::d9f::Action action{devai::strategy::d9f::Action::WAIT};
    devai::strategy::d9f::State next_state{};
    std::vector<devai::strategy::d9f::Action> legal_actions;
    std::vector<devai::strategy::d9f::Action> next_legal_actions;
    std::int64_t decision_time{0};
    std::int64_t next_decision_time{0}; // 0 allowed only for terminal transitions
    double equity_before{0.0};
    double equity_after{0.0}; // NET ledger equity; never modified by reward mode
    double reference_capital{7000.0};
    bool terminal{false};
    ActualExecutionCosts actual_costs{};
};

struct Transition {
    devai::strategy::d9f::State state{};
    devai::strategy::d9f::Action action{devai::strategy::d9f::Action::WAIT};
    double reward{0.0};
    devai::strategy::d9f::State next_state{};
    std::vector<devai::strategy::d9f::Action> next_legal_actions;
    std::int64_t decision_time{0};
    std::int64_t next_decision_time{0};
    bool terminal{false};
    double net_reward{0.0};
    double gross_reward{0.0};
    double actual_costs{0.0};
};

class DevDualModeTransition final {
public:
    [[nodiscard]] static Transition build(const TransitionInput&, RewardMode);
    // Converts an actual historical replay experience using explicitly supplied
    // causal states, action and reconciled per-interval execution costs.
    [[nodiscard]] static Transition fromHistorical(
        const devai::strategy::DevHistoricalExperience&,
        TransitionInput causal_and_cost_data, RewardMode);
    [[nodiscard]] static std::vector<Transition> buildSequence(
        const std::vector<TransitionInput>&, RewardMode);
};
} // namespace devai::d9f4c
