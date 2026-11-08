#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include <filesystem>
#include <memory>
#include <string>

namespace devai::strategy {

// Read-only inference adapter. The same agent network is shared across slots,
// as in the existing Phase 3.19A validator.
class DevFrozenOOSPolicyAdapter {
public:
    DevFrozenOOSPolicyAdapter(const std::filesystem::path& agent_checkpoint,
                              const std::filesystem::path& manager_checkpoint);
    [[nodiscard]] std::string operator()(const DevOOSDecisionPoint& point,
                                         const DevOOSLedgerState& ledger) const;
private:
    std::unique_ptr<DevQNetwork> agent_;
    std::unique_ptr<DevManagerQNetwork> manager_;
};

// Explicit checkpoint format for the six parameter vectors of each network.
// The files MUST come from previously trained/frozen model parameters.
void saveDevFrozenAgentCheckpoint(const std::filesystem::path& path,
                                  const DevQNetwork& network);
void saveDevFrozenManagerCheckpoint(const std::filesystem::path& path,
                                    const DevManagerQNetwork& network);
}
