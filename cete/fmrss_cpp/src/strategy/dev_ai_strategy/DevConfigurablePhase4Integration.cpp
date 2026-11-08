#include "devai/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai {
namespace {
bool probability(double x) { return std::isfinite(x) && x >= 0.0 && x <= 1.0; }
std::string agentId(std::size_t one_based_index) {
    std::ostringstream out;
    out << "DevAgent_" << std::setw(3) << std::setfill('0') << one_based_index;
    return out.str();
}
}

DevConfigurablePhase4Integration::DevConfigurablePhase4Integration(
    DevConfigurableManagerSpec spec, DevStrategyRankingCallback strategy,
    DevAgentInferenceCallback model)
    : spec_(std::move(spec)), strategy_(std::move(strategy)), model_(std::move(model)) {
    if (spec_.manager_id.empty() || spec_.model_id.empty() || !strategy_ || !model_ ||
        spec_.universe.empty() || spec_.top_k == 0 || spec_.top_k > spec_.universe.size() ||
        spec_.interval_seconds <= 0)
        throw std::invalid_argument("Invalid DevManager configuration or missing adapters");
    std::unordered_set<std::string> symbols;
    for (const auto& symbol : spec_.universe)
        if (symbol.empty() || !symbols.insert(symbol).second)
            throw std::invalid_argument("Universe symbols must be nonempty and unique");
}

DevManagerCandidateFrame DevConfigurablePhase4Integration::evaluate(
    std::int64_t completed_boundary_time) const {
    if (completed_boundary_time <= 0 || completed_boundary_time % spec_.interval_seconds != 0)
        throw std::invalid_argument("Not a completed configured interval boundary");

    // DevManager invokes its strategy over its entire assigned universe.
    auto ranked = strategy_(spec_.strategy, spec_.universe, completed_boundary_time);
    if (ranked.size() != spec_.universe.size())
        throw std::runtime_error("Strategy must evaluate every assigned universe symbol");
    const std::unordered_set<std::string> allowed(spec_.universe.begin(), spec_.universe.end());
    std::unordered_set<std::string> seen;
    for (const auto& item : ranked) {
        if (!allowed.count(item.symbol) || !seen.insert(item.symbol).second ||
            !std::isfinite(item.ranking_score) || item.latest_observation_time <= 0 ||
            item.latest_observation_time > completed_boundary_time)
            throw std::runtime_error("Invalid, duplicate or future-dated strategy output");
    }
    // Score is supplied by the configured strategy. No replacement score invented here.
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.ranking_score != b.ranking_score) return a.ranking_score > b.ranking_score;
        return a.symbol < b.symbol;
    });
    ranked.resize(spec_.top_k);

    DevManagerCandidateFrame frame;
    frame.manager_id = spec_.manager_id;
    frame.strategy = spec_.strategy;
    frame.decision_time = completed_boundary_time;
    frame.selected = std::move(ranked);
    frame.reports.reserve(frame.selected.size());
    for (std::size_t i = 0; i < frame.selected.size(); ++i) {
        const auto id = agentId(i + 1); // IDs are scoped by manager_id.
        auto report = model_(spec_.model_id, spec_.manager_id, id,
                             frame.selected[i], completed_boundary_time);
        if (report.manager_id != spec_.manager_id || report.agent_id != id ||
            report.symbol != frame.selected[i].symbol ||
            !probability(report.buy_probability) ||
            !probability(report.sell_probability) ||
            !probability(report.exit_probability))
            throw std::runtime_error("Invalid agent/model report");
        frame.reports.push_back(std::move(report));
    }
    return frame;
}
} // namespace devai
