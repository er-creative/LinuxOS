#include "devai/strategy/dev_ai_strategy/reward/DevProductionTransitionIntegration_D9F4C.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    // Smoke-check that production adapter types and both reward modes are linkable.
    // A real replay fixture must provide valid DevManagerState and ledger.execute results.
    using namespace devai::strategy::d9f4c;
    const auto empty = DevProductionTransitionIntegration::buildSequence(
        {}, LedgerRewardMode::NET_EXECUTION_REWARD);
    if (!empty.empty()) throw std::runtime_error("empty sequence not empty");
    std::cout << "D9F4C_PRODUCTION_ADAPTER_LINK_PASS model_trained=0 oos_evaluated=0\n";
}
