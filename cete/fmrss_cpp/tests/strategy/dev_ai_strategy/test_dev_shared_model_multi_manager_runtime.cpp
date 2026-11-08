
#include "devai/strategy/dev_ai_strategy/DevSharedModelMultiManagerRuntime.hpp"
#include "Dev5C3HTestFixture.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace devai::strategy;
namespace fx = cete_5c3h_test;

static DevSharedCandidate candidate(std::string symbol, std::int64_t time) {
    DevSharedCandidate c{};

    c.slot = 0;
    c.symbol = std::move(symbol);
    c.candidate_score = 0.9;
    c.candidate_rank = 1;

    c.observation.symbol = c.symbol;
    c.observation.decision_time = time;
    c.observation.ready = true;
    c.observation.numerical_integrity = true;

    return c;
}

int main() {
    DevQNetwork shared_agent({42U, true});
    DevManagerQNetwork shared_manager({42U, true});

    DevinderjeetSupervisor supervisor({
        100000.0, 0.75, 5.0, 2.0, 2
    });

    auto a = fx::ids(1);
    auto b = fx::ids(6);

    supervisor.registerManager({
        "DevManager_001",
        {a.begin(), a.end()},
        50000.0
    });

    supervisor.registerManager({
        "DevManager_002",
        {b.begin(), b.end()},
        50000.0
    });

    fx::Fixture f(
        std::filesystem::temp_directory_path()
            / "cete_5c3h_shared_runtime_test",
        supervisor
    );

    f.add("DevManager_001", {"RELIANCE"});
    f.add("DevManager_002", {"HDFCBANK"});

    DevSharedModelMultiManagerRuntime runtime(
        shared_agent,
        shared_manager,
        supervisor,
        f.execution
    );

    runtime.addManager("DevManager_001", a);
    runtime.addManager("DevManager_002", b);

    // --------------------------------------------------------
    // 1. Shared-model inference
    // --------------------------------------------------------

    const auto x = runtime.evaluate(
        "DevManager_001",
        1000,
        {candidate("RELIANCE", 1000)}
    );

    const auto y = runtime.evaluate(
        "DevManager_002",
        1000,
        {candidate("HDFCBANK", 1000)}
    );

    assert(
        x.global_agent_reports.at(0).agent_name
        == "DevAgent_001"
    );

    assert(
        y.global_agent_reports.at(0).agent_name
        == "DevAgent_006"
    );

    assert(x.action == DevManagerAction::NO_TRADE);
    assert(y.action == DevManagerAction::NO_TRADE);

    assert(supervisor.snapshot().open_positions == 0);

    // --------------------------------------------------------
    // 2. Stale-data protection
    // --------------------------------------------------------

    bool rejected = false;

    try {
        (void)runtime.evaluate(
            "DevManager_002",
            1000,
            {candidate("HDFCBANK", 999)}
        );
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    // --------------------------------------------------------
    // 3. Invalid BUY must still be rejected
    // --------------------------------------------------------

    rejected = false;

    try {
        (void)runtime.submit(
            x,
            DevSupervisedAction::BUY,
            "bad-buy",
            10,
            2500.0
        );
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    // --------------------------------------------------------
    // 4. HOLD is a local, non-executable decision
    // --------------------------------------------------------

    const auto positions_before =
        supervisor.snapshot().open_positions;

    // This must return normally, without sending HOLD
    // through the secure order execution gateway.
    (void)runtime.submit(
        x,
        DevSupervisedAction::HOLD,
        "hold-1",
        0,
        0.0
    );

    const auto positions_after =
        supervisor.snapshot().open_positions;

    assert(positions_after == positions_before);
    assert(positions_after == 0);

    // --------------------------------------------------------
    // 5. Approval revocation must remain fail-closed
    // --------------------------------------------------------

    f.trusted = false;

    assert(
        !f.execution.eligible(
            "DevManager_001",
            "RELIANCE"
        )
    );

    std::cout
        << "PASS shared-model multi-manager inference, "
        << "stale-data rejection, invalid BUY rejection, "
        << "local HOLD and approval revocation\n";
}
