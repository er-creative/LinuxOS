#pragma once
#include "devai/benchmark/DevRLModelInterface.hpp"
#include "devai/benchmark/DevMaskablePPO.hpp"
#include <memory>
#include <stdexcept>

namespace devai::benchmark {
// Read-only inference over a frozen PPO checkpoint. PPO outputs policy logits,
// not Q-values, expected returns, or calibrated probabilities.
class DevMaskablePPOInferenceAdapter_H47_5E2B final : public IDevRLModel {
public:
    explicit DevMaskablePPOInferenceAdapter_H47_5E2B(std::shared_ptr<const DevMaskablePPO> model)
        : model_(std::move(model)) {
        if (!model_) throw std::invalid_argument("5E2B: null PPO model");
    }
    DevRLCandidate candidate() const noexcept override { return DevRLCandidate::MaskablePPO; }
    std::string_view modelName() const noexcept override { return "Candidate 1 - Maskable PPO"; }
    DevRLDecision evaluate(const DevRLRequest& request) const override {
        // Validate features and role-specific mask BEFORE model evaluation.
        (void)selectLegalAction(request, {});
        return selectLegalAction(request, model_->policyLogits(request.features));
    }
private:
    std::shared_ptr<const DevMaskablePPO> model_;
};
} // namespace devai::benchmark
