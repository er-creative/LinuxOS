#pragma once
#include "devai/benchmark/DevRLModelInterface.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include "devai/strategy/dev_ai_strategy/DevRankingRefreshController_H47_5E1.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
using DevRLRequestProvider_H47_5E2B = std::function<devai::benchmark::DevRLRequest(
    const std::string& symbol, std::int64_t decision_time, std::size_t slot)>;
struct DevSelectedRLAgentReport_H47_5E2B {
    std::string agent_name, symbol;
    std::int64_t decision_time{};
    std::size_t candidate_rank{};
    devai::benchmark::DevRLDecision decision{};
};
struct DevSelectedRLCycle_H47_5E2B {
    std::int64_t decision_time{}, ranking_time{};
    devai::benchmark::DevRLCandidate candidate{};
    std::array<DevSelectedRLAgentReport_H47_5E2B, DEV_AGENT_SLOT_COUNT> reports{};
    bool complete{false};
};
class DevSelectedRLInference_H47_5E2B final {
public:
    DevSelectedRLInference_H47_5E2B(
        std::shared_ptr<const devai::benchmark::IDevRLModel> selected_model,
        DevRLRequestProvider_H47_5E2B provider)
        : model_(std::move(selected_model)), provider_(std::move(provider)) {
        if (!model_ || !provider_) throw std::invalid_argument("5E2B: model/provider required");
    }
    [[nodiscard]] DevSelectedRLCycle_H47_5E2B evaluate(
        const DevTopCandidateSelection& ranking, std::int64_t decision_time) const {
        if (decision_time <= 0 || decision_time % 60 != 0 ||
            ranking.decision_time <= 0 ||
            !DevRankingRefreshController_H47_5E1::isSessionTime(decision_time) ||
            !DevRankingRefreshController_H47_5E1::isRefreshTime(ranking.decision_time) ||
            ranking.decision_time != DevRankingRefreshController_H47_5E1::rankingEpoch(decision_time) ||
            !ranking.numerical_integrity ||
            ranking.selected_candidate_count != DEV_AGENT_SLOT_COUNT ||
            ranking.eligible_candidate_count < DEV_AGENT_SLOT_COUNT ||
            ranking.benchmark_symbol.empty())
            throw std::invalid_argument("5E2B: invalid or stale ranking");
        // This layer never authorizes orders. The session/15:15 execution cutoff
        // belongs to DevManager's trading-permission gate.
        std::unordered_set<std::string> symbols;
        for (std::size_t i=0; i<DEV_AGENT_SLOT_COUNT; ++i) {
            const auto& a=ranking.assignments[i];
            if (!a.assigned || a.slot_index!=i || a.candidate_rank!=i+1 ||
                a.agent_name!=DevTopCandidateSelector::agentNames()[i] ||
                a.decision_time!=ranking.decision_time || a.symbol.empty() ||
                a.symbol==ranking.benchmark_symbol || !symbols.insert(a.symbol).second)
                throw std::invalid_argument("5E2B: invalid assignment");
        }
        DevSelectedRLCycle_H47_5E2B cycle{};
        cycle.decision_time=decision_time;
        cycle.ranking_time=ranking.decision_time;
        cycle.candidate=model_->candidate();
        for (std::size_t i=0; i<DEV_AGENT_SLOT_COUNT; ++i) {
            const auto& a=ranking.assignments[i];
            const auto request=provider_(a.symbol,decision_time,i);
            if (request.role!=devai::benchmark::DevRLRole::Agent)
                throw std::invalid_argument("5E2B: agent role required");
            const auto decision=model_->evaluate(request);
            if (!decision.valid || decision.selected_action>=devai::benchmark::kAgentActionCount ||
                !request.legal_actions[decision.selected_action])
                throw std::runtime_error("5E2B: invalid or illegal model decision");
            // Fail closed if an adapter returns nonfinite values or bypasses mask rules.
            const auto checked=devai::benchmark::selectLegalAction(request,decision.action_values);
            if (!checked.valid || checked.selected_action!=decision.selected_action)
                throw std::runtime_error("5E2B: model action/mask mismatch");
            cycle.reports[i]={a.agent_name,a.symbol,decision_time,i+1,decision};
        }
        cycle.complete=true;
        return cycle;
    }
    [[nodiscard]] devai::benchmark::DevRLDecision evaluateManager(
        const devai::benchmark::DevRLRequest& request) const {
        if (request.role!=devai::benchmark::DevRLRole::Manager)
            throw std::invalid_argument("5E2B: manager role required");
        const auto result=model_->evaluate(request);
        const auto checked=devai::benchmark::selectLegalAction(request,result.action_values);
        if (!result.valid || !checked.valid || result.selected_action!=checked.selected_action)
            throw std::runtime_error("5E2B: invalid manager decision");
        return result;
    }
private:
    std::shared_ptr<const devai::benchmark::IDevRLModel> model_;
    DevRLRequestProvider_H47_5E2B provider_;
};
} // namespace devai::strategy
