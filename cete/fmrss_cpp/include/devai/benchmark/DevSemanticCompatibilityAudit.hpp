#pragma once
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include <array>
#include <filesystem>
#include <string>

namespace devai::benchmark {

struct DevSemanticAuditResult {
    bool checkpoint_structure_valid{false};
    bool current_layout_valid{false};
    bool deterministic_inference{false};
    bool manager_action_mapping_valid{false};
    bool historical_semantics_certified{false}; // Never inferred from dimensions or inference alone.
    std::string diagnostic;
};

// A compatibility audit, NOT a historical V1/V3 semantic-equivalence certificate.
// Does not modify or resave the checkpoint.
class DevSemanticCompatibilityAudit {
public:
    [[nodiscard]] static DevSemanticAuditResult run(
        const std::filesystem::path& v1_checkpoint);
};
}
