#include "devai/strategy/dev_ai_strategy/DevPortfolioRotation.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy
{
namespace
{

[[nodiscard]] bool finite(double value) noexcept
{
    return std::isfinite(value);
}

[[nodiscard]] bool same(double a, double b) noexcept
{
    return std::fabs(a - b) <= 1.0e-12;
}

[[nodiscard]] bool isDirectional(DevAction action) noexcept
{
    return action == DevAction::LONG || action == DevAction::SHORT;
}

} // namespace

DevPortfolioRotationEngine::DevPortfolioRotationEngine(
    DevPortfolioAllocationConfig config)
    : config_(std::move(config))
{
    if (!finite(config_.deployed_capital_fraction) ||
        config_.deployed_capital_fraction <= 0.0 ||
        config_.deployed_capital_fraction > 1.0)
    {
        throw std::invalid_argument(
            "DevPortfolioRotationEngine: deployed_capital_fraction must be in (0, 1].");
    }
}

const DevPortfolioAllocationConfig&
DevPortfolioRotationEngine::config() const noexcept
{
    return config_;
}

const DevAgentReport& DevPortfolioRotationEngine::selectedReport(
    const DevManagerState& state,
    const DevManagerDecision& decision)
{
    if (!decision.selected_slot_index.has_value())
        throw std::invalid_argument("DevPortfolioRotationEngine: SELECT_AGENT has no slot.");

    const std::size_t slot = *decision.selected_slot_index;
    if (slot >= DEV_AGENT_SLOT_COUNT || !state.agent_reports[slot].has_value())
        throw std::invalid_argument("DevPortfolioRotationEngine: selected DevAgent slot is EMPTY.");

    const auto& report = *state.agent_reports[slot];
    if (report.agent_name != decision.selected_agent_name ||
        report.symbol != decision.selected_symbol)
    {
        throw std::invalid_argument(
            "DevPortfolioRotationEngine: selected report does not match Manager decision.");
    }
    return report;
}

double DevPortfolioRotationEngine::positionForAction(DevAction action)
{
    if (action == DevAction::LONG) return DEV_POSITION_LONG;
    if (action == DevAction::SHORT) return DEV_POSITION_SHORT;
    if (action == DevAction::EXIT) return DEV_POSITION_FLAT;
    throw std::invalid_argument(
        "DevPortfolioRotationEngine: HOLD has no new directional target.");
}

DevPortfolioLeg DevPortfolioRotationEngine::makeExitLeg(
    const std::string& symbol) const
{
    if (symbol.empty())
        throw std::invalid_argument("DevPortfolioRotationEngine: EXIT leg requires symbol.");

    DevPortfolioLeg leg{};
    leg.symbol = symbol;
    leg.action = DevAction::EXIT;
    leg.target_exposure_fraction = 0.0;
    leg.numerical_integrity = true;
    return leg;
}

DevPortfolioLeg DevPortfolioRotationEngine::makeEntryLeg(
    const std::string& symbol,
    DevAction action) const
{
    if (symbol.empty() || !isDirectional(action))
        throw std::invalid_argument(
            "DevPortfolioRotationEngine: entry leg requires symbol and LONG/SHORT action.");

    DevPortfolioLeg leg{};
    leg.symbol = symbol;
    leg.action = action;
    leg.target_exposure_fraction = config_.deployed_capital_fraction;
    leg.numerical_integrity = true;
    return leg;
}

void DevPortfolioRotationEngine::validateLeg(const DevPortfolioLeg& leg)
{
    if (!leg.numerical_integrity || leg.symbol.empty() ||
        !finite(leg.target_exposure_fraction) ||
        leg.target_exposure_fraction < 0.0 ||
        leg.target_exposure_fraction > 1.0)
    {
        throw std::invalid_argument("DevPortfolioRotationEngine: invalid portfolio leg.");
    }

    if (leg.action == DevAction::EXIT)
    {
        if (!same(leg.target_exposure_fraction, 0.0))
            throw std::invalid_argument("DevPortfolioRotationEngine: EXIT exposure must be zero.");
        return;
    }

    if (!isDirectional(leg.action))
        throw std::invalid_argument("DevPortfolioRotationEngine: leg action must be LONG/SHORT/EXIT.");

    if (!(leg.target_exposure_fraction > 0.0))
        throw std::invalid_argument("DevPortfolioRotationEngine: entry exposure must be positive.");
}

DevPortfolioRotationPlan DevPortfolioRotationEngine::plan(
    const DevManagerState& state,
    const DevManagerDecision& decision,
    const DevManager& manager) const
{
    manager.validateState(state);
    manager.validateDecision(state, decision);

    DevPortfolioRotationPlan result{};
    result.decision_time = state.decision_time;
    result.manager_decision_type = decision.type;
    result.current_symbol = state.portfolio.current_symbol;
    result.current_position = state.portfolio.portfolio.position;

    const bool flat = same(result.current_position, DEV_POSITION_FLAT);

    if (decision.type == DevManagerDecisionType::NO_TRADE)
    {
        result.target_position = DEV_POSITION_FLAT;
        result.deployed_capital_fraction = 0.0;
        result.reserve_capital_fraction = 1.0;

        if (flat)
        {
            result.rotation_type = DevPortfolioRotationType::STAY_FLAT;
        }
        else
        {
            result.rotation_type = DevPortfolioRotationType::EXIT_TO_FLAT;
            result.legs.push_back(makeExitLeg(result.current_symbol));
        }
    }
    else if (decision.type == DevManagerDecisionType::KEEP_CURRENT)
    {
        if (flat)
            throw std::invalid_argument("DevPortfolioRotationEngine: cannot KEEP_CURRENT while flat.");

        result.rotation_type = DevPortfolioRotationType::KEEP_CURRENT;
        result.target_symbol = result.current_symbol;
        result.target_position = result.current_position;
        result.deployed_capital_fraction = config_.deployed_capital_fraction;
        result.reserve_capital_fraction = 1.0 - config_.deployed_capital_fraction;
        // No execution leg: existing position remains untouched. Phase 3.18 does
        // not manufacture a rebalance trade solely to force exact notional.
    }
    else if (decision.type == DevManagerDecisionType::SELECT_AGENT)
    {
        const auto& report = selectedReport(state, decision);

        // SELECT_AGENT means "deploy into this candidate". HOLD/EXIT are valid
        // stock-level Q outputs, but they do not define a new deployable target.
        // Manager training/integration must therefore choose a SELECT_AGENT only
        // when that selected report recommends LONG or SHORT.
        if (!isDirectional(report.preferred_action))
            throw std::invalid_argument(
                "DevPortfolioRotationEngine: selected DevAgent must recommend LONG or SHORT.");

        result.target_symbol = report.symbol;
        result.target_position = positionForAction(report.preferred_action);
        result.deployed_capital_fraction = config_.deployed_capital_fraction;
        result.reserve_capital_fraction = 1.0 - config_.deployed_capital_fraction;

        if (flat)
        {
            result.rotation_type = DevPortfolioRotationType::ENTER_SELECTED;
            result.legs.push_back(makeEntryLeg(report.symbol, report.preferred_action));
        }
        else if (result.current_symbol != report.symbol)
        {
            result.rotation_type = DevPortfolioRotationType::ROTATE_TO_SELECTED;
            result.legs.push_back(makeExitLeg(result.current_symbol));
            result.legs.push_back(makeEntryLeg(report.symbol, report.preferred_action));
        }
        else if (!same(result.current_position, result.target_position))
        {
            // Same symbol, opposite direction. CETE receives an explicit close
            // then open, so costs/slippage are accounted by CETE exactly once.
            result.rotation_type = DevPortfolioRotationType::REVERSE_SELECTED;
            result.legs.push_back(makeExitLeg(result.current_symbol));
            result.legs.push_back(makeEntryLeg(report.symbol, report.preferred_action));
        }
        else
        {
            // Selecting the already-held symbol in the already-held direction
            // is economically KEEP_CURRENT; do not create unnecessary turnover.
            result.rotation_type = DevPortfolioRotationType::KEEP_CURRENT;
        }
    }
    else
    {
        throw std::invalid_argument("DevPortfolioRotationEngine: invalid Manager decision type.");
    }

    result.numerical_integrity = true;
    validatePlan(state, decision, result, manager);
    return result;
}

void DevPortfolioRotationEngine::validatePlan(
    const DevManagerState& state,
    const DevManagerDecision& decision,
    const DevPortfolioRotationPlan& plan_value,
    const DevManager& manager) const
{
    manager.validateState(state);
    manager.validateDecision(state, decision);

    if (!plan_value.numerical_integrity ||
        plan_value.decision_time != state.decision_time ||
        plan_value.manager_decision_type != decision.type ||
        !isValidPortfolioRotationType(plan_value.rotation_type) ||
        !finite(plan_value.current_position) ||
        !finite(plan_value.target_position) ||
        !finite(plan_value.deployed_capital_fraction) ||
        !finite(plan_value.reserve_capital_fraction))
    {
        throw std::invalid_argument("DevPortfolioRotationEngine: invalid plan integrity.");
    }

    if (plan_value.current_symbol != state.portfolio.current_symbol ||
        !same(plan_value.current_position, state.portfolio.portfolio.position))
    {
        throw std::invalid_argument("DevPortfolioRotationEngine: plan origin does not match CETE state.");
    }

    if (plan_value.deployed_capital_fraction < 0.0 ||
        plan_value.deployed_capital_fraction > 1.0 ||
        plan_value.reserve_capital_fraction < 0.0 ||
        plan_value.reserve_capital_fraction > 1.0 ||
        !same(plan_value.deployed_capital_fraction + plan_value.reserve_capital_fraction, 1.0))
    {
        throw std::invalid_argument("DevPortfolioRotationEngine: invalid capital fractions.");
    }

    if (plan_value.legs.size() > 2)
        throw std::invalid_argument("DevPortfolioRotationEngine: rotation may contain at most two legs.");

    for (const auto& leg : plan_value.legs)
    {
        validateLeg(leg);

        // Every directional entry/re-entry leg must preserve the DevAI
        // allocation policy exactly. validateLeg() only validates the generic
        // legal range; this plan-level check binds the leg to this engine's
        // configured deployment fraction (75% by default).
        if (isDirectional(leg.action) &&
            !same(leg.target_exposure_fraction,
                  config_.deployed_capital_fraction))
        {
            throw std::invalid_argument(
                "DevPortfolioRotationEngine: directional leg exposure does "
                "not match configured deployed capital fraction.");
        }
    }

    if (decision.type == DevManagerDecisionType::NO_TRADE)
    {
        if (!plan_value.target_symbol.empty() ||
            !same(plan_value.target_position, DEV_POSITION_FLAT) ||
            !same(plan_value.deployed_capital_fraction, 0.0) ||
            !same(plan_value.reserve_capital_fraction, 1.0))
            throw std::invalid_argument("DevPortfolioRotationEngine: invalid NO_TRADE plan.");

        const bool was_flat = same(state.portfolio.portfolio.position, DEV_POSITION_FLAT);
        if (was_flat)
        {
            if (plan_value.rotation_type != DevPortfolioRotationType::STAY_FLAT || !plan_value.legs.empty())
                throw std::invalid_argument("DevPortfolioRotationEngine: invalid STAY_FLAT plan.");
        }
        else
        {
            if (plan_value.rotation_type != DevPortfolioRotationType::EXIT_TO_FLAT ||
                plan_value.legs.size() != 1 ||
                plan_value.legs[0].symbol != state.portfolio.current_symbol ||
                plan_value.legs[0].action != DevAction::EXIT)
                throw std::invalid_argument("DevPortfolioRotationEngine: invalid EXIT_TO_FLAT plan.");
        }
        return;
    }

    if (decision.type == DevManagerDecisionType::KEEP_CURRENT)
    {
        if (plan_value.rotation_type != DevPortfolioRotationType::KEEP_CURRENT ||
            plan_value.target_symbol != state.portfolio.current_symbol ||
            !same(plan_value.target_position, state.portfolio.portfolio.position) ||
            !same(plan_value.deployed_capital_fraction, config_.deployed_capital_fraction) ||
            !plan_value.legs.empty())
            throw std::invalid_argument("DevPortfolioRotationEngine: invalid KEEP_CURRENT plan.");
        return;
    }

    const auto& report = selectedReport(state, decision);
    if (!isDirectional(report.preferred_action) ||
        plan_value.target_symbol != report.symbol ||
        !same(plan_value.target_position, positionForAction(report.preferred_action)) ||
        !same(plan_value.deployed_capital_fraction, config_.deployed_capital_fraction))
    {
        throw std::invalid_argument("DevPortfolioRotationEngine: invalid SELECT_AGENT target.");
    }

    const bool was_flat = same(state.portfolio.portfolio.position, DEV_POSITION_FLAT);
    if (was_flat)
    {
        if (plan_value.rotation_type != DevPortfolioRotationType::ENTER_SELECTED ||
            plan_value.legs.size() != 1 ||
            plan_value.legs[0].symbol != report.symbol ||
            plan_value.legs[0].action != report.preferred_action)
            throw std::invalid_argument("DevPortfolioRotationEngine: invalid entry plan.");
    }
    else if (state.portfolio.current_symbol != report.symbol)
    {
        if (plan_value.rotation_type != DevPortfolioRotationType::ROTATE_TO_SELECTED ||
            plan_value.legs.size() != 2 ||
            plan_value.legs[0].action != DevAction::EXIT ||
            plan_value.legs[0].symbol != state.portfolio.current_symbol ||
            plan_value.legs[1].symbol != report.symbol ||
            plan_value.legs[1].action != report.preferred_action)
            throw std::invalid_argument("DevPortfolioRotationEngine: invalid cross-symbol rotation plan.");
    }
    else if (!same(state.portfolio.portfolio.position, plan_value.target_position))
    {
        if (plan_value.rotation_type != DevPortfolioRotationType::REVERSE_SELECTED ||
            plan_value.legs.size() != 2 ||
            plan_value.legs[0].action != DevAction::EXIT ||
            plan_value.legs[1].action != report.preferred_action)
            throw std::invalid_argument("DevPortfolioRotationEngine: invalid reversal plan.");
    }
    else
    {
        if (plan_value.rotation_type != DevPortfolioRotationType::KEEP_CURRENT || !plan_value.legs.empty())
            throw std::invalid_argument("DevPortfolioRotationEngine: redundant same-position selection must not trade.");
    }
}

} // namespace devai::strategy
