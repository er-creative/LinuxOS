#include "devai/benchmark/DevRLModelInterface.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace devai::benchmark {
namespace {
[[nodiscard]] std::size_t actionCount(DevRLRole role) {
    switch (role) {
    case DevRLRole::Agent: return kAgentActionCount;
    case DevRLRole::Manager: return kManagerActionCount;
    }
    throw std::invalid_argument("H2: unknown role");
}
} // namespace

DevRLDecision selectLegalAction(const DevRLRequest& request,
                                const std::array<double, kOutputCount>& values) {
    for (double x : request.features)
        if (!std::isfinite(x)) throw std::invalid_argument("H2: nonfinite feature");
    for (double x : values)
        if (!std::isfinite(x)) throw std::invalid_argument("H2: nonfinite model output");

    const auto count = actionCount(request.role);
    for (std::size_t i = count; i < kOutputCount; ++i)
        if (request.legal_actions[i])
            throw std::invalid_argument("H2: mask enables action outside role action space");

    DevRLDecision result{};
    result.action_values = values;
    double best = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < count; ++i) {
        if (!request.legal_actions[i]) continue;
        // Stable lowest-index tie break.
        if (!result.valid || values[i] > best) {
            best = values[i];
            result.selected_action = i;
            result.valid = true;
        }
    }
    // No legal action means no decision; caller must WAIT/fail closed.
    return result;
}

DevSequentialTDAdapter::DevSequentialTDAdapter(
    std::shared_ptr<const devai::strategy::DevSharedNeuralNetwork> model)
    : model_(std::move(model)) {
    if (!model_) throw std::invalid_argument("H2: null shared model");
}
DevRLCandidate DevSequentialTDAdapter::candidate() const noexcept {
    return DevRLCandidate::SequentialTD;
}
std::string_view DevSequentialTDAdapter::modelName() const noexcept {
    return "Candidate 0 - Sequential TD";
}
DevRLDecision DevSequentialTDAdapter::evaluate(const DevRLRequest& request) const {
    // Validate role/mask/features before invoking inference.
    const std::array<double, kOutputCount> zero{};
    (void)selectLegalAction(request, zero);
    devai::strategy::DevSharedInput input{};
    input.values = request.features;
    return selectLegalAction(request, model_->predict(input));
}
std::string_view candidateName(DevRLCandidate candidate) noexcept {
    switch (candidate) {
    case DevRLCandidate::SequentialTD: return "Sequential TD";
    case DevRLCandidate::MaskablePPO: return "Maskable PPO";
    case DevRLCandidate::QRDQN: return "QR-DQN";
    case DevRLCandidate::DiscreteCQL: return "Discrete CQL";
    }
    return "Unknown";
}
bool isImplementedInH2(DevRLCandidate candidate) noexcept {
    return candidate == DevRLCandidate::SequentialTD;
}
} // namespace devai::benchmark
