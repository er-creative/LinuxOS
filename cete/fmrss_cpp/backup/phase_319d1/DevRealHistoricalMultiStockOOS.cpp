#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace devai::strategy {
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
void checkPrice(double p) {
    check(std::isfinite(p) && p > 0.0, "3.19B: missing/invalid aligned historical price");
}
}
DevRealHistoricalMultiStockOOS::DevRealHistoricalMultiStockOOS(DevOOSLedgerConfig c)
    : config_(c) {
    check(std::isfinite(c.initial_capital) && c.initial_capital > 0.0,
          "3.19B: invalid initial capital");
    check(std::isfinite(c.max_allocation_fraction) && c.max_allocation_fraction > 0.0 &&
          c.max_allocation_fraction <= 1.0, "3.19B: invalid allocation");
    for (double r : {c.brokerage_rate, c.slippage_rate, c.other_cost_rate})
        check(std::isfinite(r) && r >= 0.0 && r < 1.0, "3.19B: invalid cost rate");
    state_.cash = state_.equity = state_.peak_equity = c.initial_capital;
}
const DevOOSLedgerState& DevRealHistoricalMultiStockOOS::state() const noexcept {
    return state_;
}
DevOOSExecutionResult DevRealHistoricalMultiStockOOS::execute(
    std::int64_t t, const std::string& desired, const DevOOSPriceProvider& prices) {
    check(static_cast<bool>(prices), "3.19B: price provider missing");
    check(t > state_.last_time, "3.19B: decision times must strictly increase");
    // Obtain all required prices before mutating ledger; no partial fills on bad data.
    const double old_price = state_.quantity ? prices(state_.held_symbol, t) : 0.0;
    if (state_.quantity) checkPrice(old_price);
    const bool rotate = desired != state_.held_symbol;
    const double new_price = (rotate && !desired.empty()) ? prices(desired, t) : 0.0;
    if (rotate && !desired.empty()) checkPrice(new_price);

    DevOOSLedgerState next = state_;
    DevOOSExecutionResult r{};
    r.decision_time = t;
    r.equity_before = state_.equity;
    const double market_equity = state_.cash + static_cast<double>(state_.quantity) * old_price;
    r.gross_pnl = market_equity - state_.equity;
    const auto charge = [&](double notional) {
        r.brokerage += notional * config_.brokerage_rate;
        r.slippage += notional * config_.slippage_rate;
        r.other_costs += notional * config_.other_cost_rate;
    };
    if (rotate && state_.quantity > 0) {
        const double proceeds = static_cast<double>(state_.quantity) * old_price;
        next.cash += proceeds;
        charge(proceeds);
        next.held_symbol.clear();
        next.quantity = 0;
        ++r.executed_legs;
    }
    if (rotate && !desired.empty()) {
        const double unit_total = new_price * (1.0 + config_.brokerage_rate +
                              config_.slippage_rate + config_.other_cost_rate);
        const double budget = std::min(next.cash - r.brokerage - r.slippage - r.other_costs,
                                       market_equity * config_.max_allocation_fraction);
        const auto qty = static_cast<std::int64_t>(std::floor(std::max(0.0, budget) / unit_total));
        if (qty > 0) {
            const double notional = static_cast<double>(qty) * new_price;
            next.cash -= notional;
            charge(notional);
            next.held_symbol = desired;
            next.quantity = qty;
            ++r.executed_legs;
        }
    }
    next.cash -= r.brokerage + r.slippage + r.other_costs;
    check(next.cash >= -1e-7, "3.19B: insufficient cash for fills and costs");
    if (next.cash < 0.0) next.cash = 0.0;
    r.net_pnl = r.gross_pnl - r.brokerage - r.slippage - r.other_costs;
    r.equity_after = r.equity_before + r.net_pnl;
    check(r.equity_after > 0.0 && std::isfinite(r.equity_after), "3.19B: bankrupt/invalid equity");
    next.equity = r.equity_after;
    next.peak_equity = std::max(next.peak_equity, next.equity);
    next.last_time = t;
    r.drawdown = (next.peak_equity - next.equity) / next.peak_equity;
    r.numerical_integrity = true;
    state_ = std::move(next);
    return r;
}
DevOOSDecisionPoint DevRealHistoricalMultiStockOOS::toDecisionPoint(
    const DevHistoricalDecisionRecord& record,
    const DevManagerPortfolioState& portfolio) {
    DevHistoricalTrainingDatasetValidator::validateRecord(record);
    DevOOSDecisionPoint point{};
    point.decision_time = record.decision_time;
    point.benchmark_symbol = record.benchmark_symbol;
    point.portfolio = portfolio;
    point.numerical_integrity = record.numerical_integrity;
    for (const auto& candidate : record.candidates) {
        DevOOSCandidate item{};
        item.assignment.assigned = true;
        item.assignment.slot_index = candidate.slot_index;
        item.assignment.candidate_rank = candidate.candidate_rank;
        item.assignment.agent_name = candidate.agent_name;
        item.assignment.symbol = candidate.symbol;
        item.assignment.decision_time = record.decision_time;
        item.assignment.candidate_score = candidate.candidate_score;
        item.observation = candidate.observation;
        point.candidates.push_back(std::move(item));
    }
    DevMultiStockOOSValidator::validatePoint(point);
    return point;
}
}
