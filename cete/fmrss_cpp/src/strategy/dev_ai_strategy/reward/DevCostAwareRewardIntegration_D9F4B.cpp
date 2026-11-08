#include "devai/strategy/dev_ai_strategy/reward/DevCostAwareRewardIntegration_D9F4B.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::d9f4b {
namespace {
bool close(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) &&
        std::abs(a-b) <= 1e-10 * std::max({1.0, std::abs(a), std::abs(b)});
}
Action classify(const devai::strategy::DevPortfolioStep& s) {
    using devai::strategy::DevPortfolioAction;
    if (!s.executed) return s.before.position ? Action::HOLD : Action::WAIT;
    if (s.requested.action == DevPortfolioAction::EXIT) return Action::EXIT;
    return Action::OPEN;
}
}
RewardResult DevCostAwareRewardIntegration::fromHistorical(
    const devai::strategy::DevHistoricalExperience& sample,
    double initial_equity) {
    if (!std::isfinite(sample.portfolio_reward) ||
        sample.decision_time <= 0 ||
        (!sample.terminal && sample.next_decision_time <= sample.decision_time))
        throw std::invalid_argument("D9F4B invalid historical transition");
    // Historical replay has no per-transition execution flag/cost breakdown.
    // Zero here means 'not separately reported', NOT 'no fees incurred'.
    RewardInput input{};
    input.action = Action::WAIT;
    input.equity_before = sample.equity_before;
    input.equity_after = sample.equity_after;
    input.reference_capital = initial_equity;
    auto r = DevCostAwareRewardEngine::evaluate(input);
    if (!close(r.reward, sample.portfolio_reward))
        throw std::logic_error("D9F4B historical reward reconciliation failed");
    return r;
}
void DevCostAwareRewardIntegration::validateHistorical(
    const devai::strategy::DevHistoricalExperienceResult& result) {
    const auto initial = result.execution.replay.initial_equity;
    const auto& samples = result.experiences;
    const auto& curve = result.execution.replay.equity_curve;
    if (samples.empty() || curve.size() != samples.size()+1)
        throw std::logic_error("D9F4B invalid replay size");
    double sum = 0.0;
    for (std::size_t i=0; i<samples.size(); ++i) {
        const auto& s = samples[i];
        if (!close(s.equity_before,curve[i]) ||
            !close(s.equity_after,curve[i+1]) ||
            (i && !close(s.equity_before,samples[i-1].equity_after)) ||
            (s.terminal != (i+1==samples.size())) ||
            (i && s.decision_time <= samples[i-1].decision_time))
            throw std::logic_error("D9F4B replay equity/chronology mismatch");
        sum += fromHistorical(s,initial).reward;
    }
    if (!close(sum,(curve.back()-curve.front())/initial))
        throw std::logic_error("D9F4B cumulative reward mismatch");
}
RewardResult DevCostAwareRewardIntegration::fromPortfolioStep(
    const devai::strategy::DevPortfolioStep& step, double initial_cash) {
    RewardInput input{};
    input.action = classify(step);
    input.equity_before = step.before.equity;
    input.equity_after = step.after.equity;
    input.reference_capital = initial_cash;
    // No fee amount in DevPortfolioStep. Costs are already in after.equity.
    // Do not infer or double-charge costs here.
    input.executed = step.executed;
    auto r = DevCostAwareRewardEngine::evaluate(input);
    if (!close(r.reward,step.reward))
        throw std::logic_error("D9F4B portfolio step reward mismatch");
    return r;
}
}
