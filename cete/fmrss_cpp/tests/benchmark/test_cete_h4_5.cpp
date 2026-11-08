
#include "devai/benchmark/DevRuntimeReplayBridge.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace devai::benchmark;
using namespace devai::benchmark::h45;

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Original H4.5 test model:
// OPEN first candidate -> KEEP -> EXIT.
struct Model final : IDevRLModel {
    mutable std::size_t calls = 0;

    DevRLCandidate candidate() const noexcept override {
        return DevRLCandidate::SequentialTD;
    }

    std::string_view modelName() const noexcept override {
        return "H4.5 test policy";
    }

    DevRLDecision evaluate(
        const DevRLRequest& request
    ) const override {
        ++calls;

        DevRLDecision decision{};
        decision.valid = true;

        if (calls == 1) {
            decision.selected_action = 2;
        } else if (calls == 2) {
            decision.selected_action = 1;
        } else {
            decision.selected_action = 0;
        }

        check(
            request.legal_actions.at(decision.selected_action),
            "H4.5: selected action is masked"
        );

        return decision;
    }
};

// H4.6 test model:
// Select fifth LONG candidate -> EXIT.
struct FifthCandidateModel final : IDevRLModel {
    mutable std::size_t calls = 0;

    DevRLCandidate candidate() const noexcept override {
        return DevRLCandidate::SequentialTD;
    }

    std::string_view modelName() const noexcept override {
        return "H4.6 fifth LONG candidate test";
    }

    DevRLDecision evaluate(
        const DevRLRequest& request
    ) const override {
        ++calls;

        DevRLDecision decision{};
        decision.valid = true;

        decision.selected_action =
            calls == 1 ? 6 : 0;

        check(
            request.legal_actions.at(decision.selected_action),
            "H4.6: selected action is masked"
        );

        return decision;
    }
};

// Invalid-action model:
// Must be rejected by the bridge.
struct InvalidActionModel final : IDevRLModel {
    DevRLCandidate candidate() const noexcept override {
        return DevRLCandidate::SequentialTD;
    }

    std::string_view modelName() const noexcept override {
        return "H4.6 invalid action test";
    }

    DevRLDecision evaluate(
        const DevRLRequest&
    ) const override {
        DevRLDecision decision{};
        decision.valid = true;
        decision.selected_action = 7;
        return decision;
    }
};

std::vector<HistoricalManagerObservation>
makeObservations() {
    std::vector<HistoricalManagerObservation> seq(3);

    for (std::size_t i = 0; i < seq.size(); ++i) {
        auto& observation = seq[i];

        // Historical 09:15, 09:16 and 15:15 IST.
        observation.decision_time_utc =
            1793331900 +
            (i == 2
                ? 360 * 60
                : static_cast<std::int64_t>(i) * 60);

        observation.completed_candles = true;
        observation.causal_features = true;

        observation.ranked_top_five = {
            "RELIANCE",
            "SBIN",
            "ITC",
            "NTPC",
            "HDFCBANK"
        };

        // Seven actions: 0 EXIT, 1 KEEP, 2-6 SELECT.
        observation.legal_actions = {
            true, true, true, true, true, true, true
        };

        observation.terminal = (i == 2);
    }

    return seq;
}

void testOriginalReplay() {
    Model model;
    auto seq = makeObservations();

    const auto batch =
        DevRuntimeReplayBridge::inferLong(model, seq);

    check(
        batch.replay_decisions.size() == 3,
        "H4.5: incorrect replay decision count"
    );

    check(
        batch.replay_decisions[0].desired_symbol == "RELIANCE",
        "H4.5: first candidate entry failed"
    );

    check(
        batch.replay_decisions[1].manager_action == 1,
        "H4.5: KEEP action failed"
    );

    check(
        batch.replay_decisions[2].desired_symbol.empty(),
        "H4.5: terminal EXIT failed"
    );
}

void testFifthLongCandidate() {
    FifthCandidateModel model;

    auto seq = makeObservations();

    // Only the opening decision and terminal EXIT are needed.
    std::vector<HistoricalManagerObservation> test_seq = {
        seq.front(),
        seq.back()
    };

    test_seq[0].terminal = false;
    test_seq[1].terminal = true;

    const auto batch =
        DevRuntimeReplayBridge::inferLong(
            model,
            test_seq
        );

    check(
        batch.replay_decisions.size() == 2,
        "H4.6: fifth candidate replay count incorrect"
    );

    check(
        batch.replay_decisions[0].desired_symbol == "HDFCBANK",
        "H4.6: action 6 did not select fifth LONG candidate"
    );

    check(
        batch.replay_decisions[1].desired_symbol.empty(),
        "H4.6: fifth candidate terminal EXIT failed"
    );
}

void testInvalidActionRejected() {
    InvalidActionModel model;
    auto seq = makeObservations();

    bool rejected = false;

    try {
        (void)DevRuntimeReplayBridge::inferLong(
            model,
            seq
        );
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }
    catch (const std::out_of_range&) {
        rejected = true;
    }

    check(
        rejected,
        "H4.6: invalid action 7 was not rejected"
    );
}

void testInvalidManagerChronology() {
    Model model;
    auto seq = makeObservations();

    // Deliberately reverse two timestamps.
    seq[1].decision_time_utc =
        seq[0].decision_time_utc - 60;

    bool rejected = false;

    try {
        (void)DevRuntimeReplayBridge::inferLong(
            model,
            seq
        );
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    check(
        rejected,
        "H4.6: nonchronological observations accepted"
    );
}

} // namespace

int main() {
    try {
        testOriginalReplay();
        testFifthLongCandidate();
        testInvalidActionRejected();
        testInvalidManagerChronology();

        std::cout
            << "CETE H4.5/H4.6 RUNTIME DECISION BRIDGE PASS\n"
            << "Real CETE D9F4C ReplayDecision type: PASS\n"
            << "H2 model contract and legal masks: PASS (test models)\n"
            << "Original LONG entry, KEEP and terminal EXIT: PASS\n"
            << "Fifth LONG candidate (action 6): PASS\n"
            << "Invalid action 7 rejection: PASS\n"
            << "Chronology validation: PASS\n"
            << "Real trained checkpoint inference: NOT VERIFIED\n"
            << "Real CETE historical price provider: NOT CONNECTED\n"
            << "Actual D9F4C execution: NOT RUN IN THIS TEST\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
