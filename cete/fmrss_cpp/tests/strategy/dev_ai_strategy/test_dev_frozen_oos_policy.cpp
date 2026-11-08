#include "devai/strategy/dev_ai_strategy/DevFrozenOOSPolicy.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace devai::strategy;

int main() {
    const auto folder = std::filesystem::temp_directory_path();
    const auto a = folder / "cete_319d_agent.chk";
    const auto m = folder / "cete_319d_manager.chk";

    DevQNetwork agent(DevQNetworkConfig{17U, true});
    DevManagerQNetwork manager(DevManagerNetworkConfig{17U, true});
    saveDevFrozenAgentCheckpoint(a, agent);
    saveDevFrozenManagerCheckpoint(m, manager);

    DevFrozenOOSPolicyAdapter policy(a, m);

    DevOOSLedgerState ledger{};
    DevOOSDecisionPoint point{};
    point.decision_time = 1000;
    point.benchmark_symbol = "NIFTY%2050";
    point.numerical_integrity = true;

    // Phase 3.19D-1: the policy requires a valid, ledger-aligned
    // mark-to-market portfolio state, even when no position is open.
    point.portfolio.current_symbol = ledger.held_symbol;
    point.portfolio.numerical_integrity = true;

    const auto result = policy(point, ledger);
    assert(result.empty());

    std::filesystem::remove(a);
    std::filesystem::remove(m);
    std::cout << "3.19D frozen checkpoint smoke test: PASSED "
                 "(initialized flat portfolio; not trained OOS)\n";
}
