#include "devai/benchmark/DevRLModelInterface.hpp"
#include <array>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace devai::benchmark;

void require(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "H2 TEST FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    auto network = std::make_shared<devai::strategy::DevSharedNeuralNetwork>(73117U);
    DevSequentialTDAdapter adapter(network);
    require(adapter.candidate() == DevRLCandidate::SequentialTD, "assertion failed: adapter.candidate() == DevRLCandidate::SequentialTD");
    require(!isImplementedInH2(DevRLCandidate::MaskablePPO), "assertion failed: !isImplementedInH2(DevRLCandidate::MaskablePPO)");
    require(!isImplementedInH2(DevRLCandidate::QRDQN), "assertion failed: !isImplementedInH2(DevRLCandidate::QRDQN)");
    require(!isImplementedInH2(DevRLCandidate::DiscreteCQL), "assertion failed: !isImplementedInH2(DevRLCandidate::DiscreteCQL)");

    DevRLRequest manager{};
    manager.role = DevRLRole::Manager;
    manager.legal_actions[0] = true;
    manager.legal_actions[2] = true;
    const auto a = adapter.evaluate(manager);
    const auto b = adapter.evaluate(manager);
    require(a.valid && b.valid, "assertion failed: a.valid && b.valid");
    require(a.selected_action == b.selected_action, "assertion failed: a.selected_action == b.selected_action");
    require(a.selected_action == 0 || a.selected_action == 2, "assertion failed: a.selected_action == 0 || a.selected_action == 2");
    require(a.action_values == b.action_values, "assertion failed: a.action_values == b.action_values");

    DevRLRequest agent{};
    agent.role = DevRLRole::Agent;
    agent.legal_actions[1] = true;
    agent.legal_actions[3] = true;
    const auto agentResult = adapter.evaluate(agent);
    require(agentResult.valid, "assertion failed: agentResult.valid");
    require(agentResult.selected_action == 1 || agentResult.selected_action == 3, "assertion failed: agentResult.selected_action == 1 || agentResult.selected_action == 3");

    DevRLRequest noActions{};
    noActions.role = DevRLRole::Manager;
    require(!adapter.evaluate(noActions).valid, "assertion failed: !adapter.evaluate(noActions).valid");

    auto expectThrow = [](auto&& fn) {
        bool threw = false;
        try { fn(); } catch (const std::exception&) { threw = true; }
        require(threw, "assertion failed: threw");
    };
    DevRLRequest badMask{};
    badMask.role = DevRLRole::Agent;
    badMask.legal_actions[6] = true;
    expectThrow([&] { (void)adapter.evaluate(badMask); });

    DevRLRequest badFeature{};
    badFeature.role = DevRLRole::Manager;
    badFeature.legal_actions[0] = true;
    badFeature.features[0] = std::nan("");
    expectThrow([&] { (void)adapter.evaluate(badFeature); });

    auto invalidValues = std::array<double, kOutputCount>{};
    invalidValues[2] = std::nan("");
    expectThrow([&] { (void)selectLegalAction(manager, invalidValues); });
    expectThrow([] { DevSequentialTDAdapter nullAdapter(nullptr); });

    // This is a fresh seeded model, not the frozen V1 checkpoint.
    std::cout << "CETE H2 COMMON RL INTERFACE PASS\n"
              << "Candidate 0 adapter: PASS (seeded model fixture)\n"
              << "Role-aware legal action masks: PASS\n"
              << "Deterministic inference: PASS\n"
              << "Invalid inputs / no legal action: PASS\n"
              << "Advanced candidates: NOT IMPLEMENTED\n"
              << "Frozen V1 semantic equivalence: NOT CERTIFIED\n";
}
