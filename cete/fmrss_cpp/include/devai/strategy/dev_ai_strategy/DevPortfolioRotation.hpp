#pragma once

#include "devai/strategy/dev_ai_strategy/DevManager.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy
{

inline constexpr double DEV_DEFAULT_DEPLOYED_CAPITAL_FRACTION = 0.75;
inline constexpr double DEV_DEFAULT_RESERVE_CAPITAL_FRACTION  = 0.25;

enum class DevPortfolioRotationType : std::size_t
{
    STAY_FLAT = 0,
    EXIT_TO_FLAT = 1,
    KEEP_CURRENT = 2,
    ENTER_SELECTED = 3,
    ROTATE_TO_SELECTED = 4,
    REVERSE_SELECTED = 5
};

[[nodiscard]]
constexpr bool isValidPortfolioRotationType(
    DevPortfolioRotationType type) noexcept
{
    switch (type)
    {
        case DevPortfolioRotationType::STAY_FLAT:
        case DevPortfolioRotationType::EXIT_TO_FLAT:
        case DevPortfolioRotationType::KEEP_CURRENT:
        case DevPortfolioRotationType::ENTER_SELECTED:
        case DevPortfolioRotationType::ROTATE_TO_SELECTED:
        case DevPortfolioRotationType::REVERSE_SELECTED:
            return true;
    }
    return false;
}

struct DevPortfolioAllocationConfig
{
    // Fraction of current CETE equity that DevAI asks CETE to deploy.
    // CETE remains responsible for risk approval, execution, costs and P&L.
    double deployed_capital_fraction{DEV_DEFAULT_DEPLOYED_CAPITAL_FRACTION};
};

struct DevPortfolioLeg
{
    std::string symbol;
    DevAction action{DevAction::HOLD};

    // Requested target directional exposure after this leg.
    // EXIT always has target_exposure_fraction == 0.
    // LONG/SHORT use configured deployed capital fraction.
    double target_exposure_fraction{0.0};

    bool numerical_integrity{false};
};

struct DevPortfolioRotationPlan
{
    std::int64_t decision_time{0};
    DevManagerDecisionType manager_decision_type{
        DevManagerDecisionType::NO_TRADE};
    DevPortfolioRotationType rotation_type{
        DevPortfolioRotationType::STAY_FLAT};

    std::string current_symbol;
    double current_position{DEV_POSITION_FLAT};

    std::string target_symbol;
    double target_position{DEV_POSITION_FLAT};

    double deployed_capital_fraction{0.0};
    double reserve_capital_fraction{1.0};

    // Ordered intent only. CETE must validate and execute these legs.
    // Cross-symbol rotation is always EXIT old first, then ENTER new.
    std::vector<DevPortfolioLeg> legs;

    bool numerical_integrity{false};
};

class DevPortfolioRotationEngine
{
public:
    explicit DevPortfolioRotationEngine(
        DevPortfolioAllocationConfig config = {});

    [[nodiscard]]
    const DevPortfolioAllocationConfig& config() const noexcept;

    [[nodiscard]]
    DevPortfolioRotationPlan plan(
        const DevManagerState& state,
        const DevManagerDecision& decision,
        const DevManager& manager) const;

    void validatePlan(
        const DevManagerState& state,
        const DevManagerDecision& decision,
        const DevPortfolioRotationPlan& plan,
        const DevManager& manager) const;

private:
    [[nodiscard]]
    static const DevAgentReport& selectedReport(
        const DevManagerState& state,
        const DevManagerDecision& decision);

    [[nodiscard]]
    static double positionForAction(DevAction action);

    [[nodiscard]]
    DevPortfolioLeg makeExitLeg(const std::string& symbol) const;

    [[nodiscard]]
    DevPortfolioLeg makeEntryLeg(
        const std::string& symbol,
        DevAction action) const;

    static void validateLeg(const DevPortfolioLeg& leg);

    DevPortfolioAllocationConfig config_{};
};

} // namespace devai::strategy
