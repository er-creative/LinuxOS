#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


using namespace devai::strategy;


namespace
{

// ============================================================================
// TestStats
// ============================================================================

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
};


// ============================================================================
// require
// ============================================================================

void require(
    const bool condition,
    const std::string& message,
    TestStats& stats)
{
    ++stats.checks;

    if (condition)
    {
        std::cout
            << "[PASS] "
            << message
            << '\n';

        return;
    }


    ++stats.failures;

    std::cout
        << "[FAIL] "
        << message
        << '\n';
}


// ============================================================================
// throwsInvalidArgument
// ============================================================================

template <typename Function>
bool throwsInvalidArgument(
    Function&& function)
{
    try
    {
        function();
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }

    return false;
}


// ============================================================================
// throwsOutOfRange
// ============================================================================

template <typename Function>
bool throwsOutOfRange(
    Function&& function)
{
    try
    {
        function();
    }
    catch (const std::out_of_range&)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }

    return false;
}


// ============================================================================
// makeTransition
//
// Creates a structurally valid Phase 3.1 transition.
//
// reward and times uniquely identify the transition during tests.
// ============================================================================

DevTransition makeTransition(
    const std::int64_t decision_time,
    const double reward,
    const DevAction action = DevAction::HOLD,
    const bool terminal = false)
{
    DevTransition transition;

    transition.action =
        action;

    transition.reward =
        reward;

    transition.terminal =
        terminal;

    transition.decision_time =
        decision_time;

    transition.next_decision_time =
        decision_time + 60;

    return transition;
}


// ============================================================================
// sameTransitionIdentity
//
// We only compare fields deliberately populated by makeTransition().
// Observation contents remain default constructed in this Phase 3.2 unit test.
// ============================================================================

bool sameTransitionIdentity(
    const DevTransition& lhs,
    const DevTransition& rhs)
{
    return
        lhs.action ==
            rhs.action
        &&
        lhs.reward ==
            rhs.reward
        &&
        lhs.terminal ==
            rhs.terminal
        &&
        lhs.decision_time ==
            rhs.decision_time
        &&
        lhs.next_decision_time ==
            rhs.next_decision_time;
}

} // namespace


// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.2 — DEV REPLAY BUFFER VALIDATION\n"
        << "======================================================================\n";


    TestStats stats;


    // ========================================================================
    // 1. Zero capacity must be rejected
    // ========================================================================

    require(
        throwsInvalidArgument(
            []()
            {
                [[maybe_unused]]
                DevReplayBuffer buffer(
                    0,
                    123U);
            }),
        "Zero replay-buffer capacity is rejected",
        stats);


    // ========================================================================
    // 2. Initial state
    // ========================================================================

    DevReplayBuffer buffer(
        3,
        12345U);


    require(
        buffer.capacity() == 3,
        "Configured replay-buffer capacity is preserved",
        stats);


    require(
        buffer.size() == 0,
        "New replay buffer has zero stored transitions",
        stats);


    require(
        buffer.empty(),
        "New replay buffer reports empty",
        stats);


    require(
        !buffer.full(),
        "New replay buffer does not report full",
        stats);


    // ========================================================================
    // 3. First insertion
    // ========================================================================

    const DevTransition t1 =
        makeTransition(
            1'000,
            0.10,
            DevAction::LONG);


    buffer.push(
        t1);


    require(
        buffer.size() == 1,
        "First transition increases size to one",
        stats);


    require(
        !buffer.empty(),
        "Replay buffer is no longer empty after insertion",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(0),
            t1),
        "First stored transition is retrievable",
        stats);


    // ========================================================================
    // 4. Fill buffer
    // ========================================================================

    const DevTransition t2 =
        makeTransition(
            1'060,
            -0.20,
            DevAction::HOLD);


    const DevTransition t3 =
        makeTransition(
            1'120,
            0.30,
            DevAction::SHORT);


    buffer.push(
        t2);

    buffer.push(
        t3);


    require(
        buffer.size() == 3,
        "Replay buffer reaches configured capacity",
        stats);


    require(
        buffer.full(),
        "Replay buffer reports full at capacity",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(0),
            t1),
        "Oldest transition remains first before overwrite",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(1),
            t2),
        "Middle transition remains chronologically ordered",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(2),
            t3),
        "Newest transition remains last before overwrite",
        stats);


    // ========================================================================
    // 5. Oldest transition replacement
    // ========================================================================

    const DevTransition t4 =
        makeTransition(
            1'180,
            -0.40,
            DevAction::EXIT);


    buffer.push(
        t4);


    require(
        buffer.size() == 3,
        "Size remains bounded after overwrite",
        stats);


    require(
        buffer.full(),
        "Replay buffer remains full after overwrite",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(0),
            t2),
        "Oldest transition is evicted when buffer is full",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(1),
            t3),
        "Second-oldest transition advances to logical front",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(2),
            t4),
        "Newest transition appears at logical back",
        stats);


    // ========================================================================
    // 6. Second wrap verifies ring ordering
    // ========================================================================

    const DevTransition t5 =
        makeTransition(
            1'240,
            0.50,
            DevAction::LONG,
            true);


    buffer.push(
        t5);


    require(
        buffer.size() == 3,
        "Repeated overwrite preserves bounded size",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(0),
            t3),
        "Ring buffer preserves oldest transition after second overwrite",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(1),
            t4),
        "Ring buffer preserves middle transition after second overwrite",
        stats);


    require(
        sameTransitionIdentity(
            buffer.at(2),
            t5),
        "Ring buffer preserves newest transition after second overwrite",
        stats);


    require(
        buffer.at(2).terminal,
        "Terminal transition is stored without modification",
        stats);


    // ========================================================================
    // 7. Out-of-range access
    // ========================================================================

    require(
        throwsOutOfRange(
            [&]()
            {
                [[maybe_unused]]
                const DevTransition& transition =
                    buffer.at(3);
            }),
        "Out-of-range logical access is rejected",
        stats);


    // ========================================================================
    // 8. Invalid transition must be rejected
    //
    // Replay buffer delegates structural validity to frozen Phase 3.1.
    // ========================================================================

    {
        DevTransition invalid =
            makeTransition(
                2'000,
                0.25);

        invalid.next_decision_time =
            invalid.decision_time;


        const std::size_t size_before =
            buffer.size();


        require(
            throwsInvalidArgument(
                [&]()
                {
                    buffer.push(
                        invalid);
                }),
            "Invalid transition is rejected on insertion",
            stats);


        require(
            buffer.size() ==
                size_before,
            "Rejected transition does not change replay-buffer size",
            stats);


        require(
            sameTransitionIdentity(
                buffer.at(0),
                t3),
            "Rejected transition does not corrupt oldest stored experience",
            stats);


        require(
            sameTransitionIdentity(
                buffer.at(2),
                t5),
            "Rejected transition does not corrupt newest stored experience",
            stats);
    }


    // ========================================================================
    // 9. Zero-size sampling must fail
    // ========================================================================

    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const auto batch =
                    buffer.sample(0);
            }),
        "Zero-size replay sample is rejected",
        stats);


    // ========================================================================
    // 10. Oversized sampling must fail
    // ========================================================================

    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const auto batch =
                    buffer.sample(
                        buffer.size() + 1);
            }),
        "Replay sample larger than stored population is rejected",
        stats);


    // ========================================================================
    // 11. Valid sampling
    // ========================================================================

    {
        const auto batch =
            buffer.sample(2);


        require(
            batch.size() == 2,
            "Valid replay sample returns requested batch size",
            stats);


        require(
            batch[0].decision_time !=
                batch[1].decision_time,
            "Replay sample contains no duplicate transition",
            stats);
    }


    // ========================================================================
    // 12. Sampling entire population produces every stored transition once
    // ========================================================================

    {
        const auto batch =
            buffer.sample(
                buffer.size());


        require(
            batch.size() ==
                buffer.size(),
            "Full-population sample returns every stored transition",
            stats);


        bool found_t3 = false;
        bool found_t4 = false;
        bool found_t5 = false;


        for (const auto& transition : batch)
        {
            if (transition.decision_time ==
                t3.decision_time)
            {
                found_t3 = true;
            }

            if (transition.decision_time ==
                t4.decision_time)
            {
                found_t4 = true;
            }

            if (transition.decision_time ==
                t5.decision_time)
            {
                found_t5 = true;
            }
        }


        require(
            found_t3,
            "Full-population sample contains oldest stored transition",
            stats);


        require(
            found_t4,
            "Full-population sample contains middle stored transition",
            stats);


        require(
            found_t5,
            "Full-population sample contains newest stored transition",
            stats);
    }


    // ========================================================================
    // 13. Deterministic seeded sampling
    //
    // Two identically seeded buffers receiving identical transitions must
    // produce identical sample sequences.
    // ========================================================================

    {
        DevReplayBuffer first(
            5,
            777U);

        DevReplayBuffer second(
            5,
            777U);


        for (std::int64_t i = 0;
             i < 5;
             ++i)
        {
            const DevTransition transition =
                makeTransition(
                    3'000 + (i * 60),
                    static_cast<double>(i) / 100.0);


            first.push(
                transition);

            second.push(
                transition);
        }


        const auto first_batch =
            first.sample(3);

        const auto second_batch =
            second.sample(3);


        bool identical = true;


        for (std::size_t i = 0;
             i < first_batch.size();
             ++i)
        {
            if (!sameTransitionIdentity(
                    first_batch[i],
                    second_batch[i]))
            {
                identical = false;
                break;
            }
        }


        require(
            identical,
            "Identically seeded replay buffers produce reproducible sampling",
            stats);
    }


    // ========================================================================
    // 14. Clear
    // ========================================================================

    buffer.clear();


    require(
        buffer.size() == 0,
        "Clear removes all stored transitions",
        stats);


    require(
        buffer.empty(),
        "Cleared replay buffer reports empty",
        stats);


    require(
        !buffer.full(),
        "Cleared replay buffer does not report full",
        stats);


    require(
        buffer.capacity() == 3,
        "Clear preserves configured capacity",
        stats);


    require(
        throwsOutOfRange(
            [&]()
            {
                [[maybe_unused]]
                const DevTransition& transition =
                    buffer.at(0);
            }),
        "Cleared replay buffer rejects logical access",
        stats);


    // ========================================================================
    // 15. Sampling empty buffer
    // ========================================================================

    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const auto batch =
                    buffer.sample(1);
            }),
        "Empty replay buffer rejects sampling",
        stats);


    // ========================================================================
    // 16. Reuse after clear
    // ========================================================================

    {
        const DevTransition transition =
            makeTransition(
                5'000,
                0.75,
                DevAction::SHORT);


        buffer.push(
            transition);


        require(
            buffer.size() == 1,
            "Replay buffer accepts new experience after clear",
            stats);


        require(
            sameTransitionIdentity(
                buffer.at(0),
                transition),
            "Replay buffer preserves correct ordering after clear and reuse",
            stats);
    }


    // ========================================================================
    // Summary
    // ========================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 3.2 VALIDATION SUMMARY\n"
        << "======================================================================\n"
        << "Total checks : "
        << stats.checks
        << '\n'
        << "Failures     : "
        << stats.failures
        << '\n'
        << "======================================================================\n";


    if (stats.failures != 0)
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 3.2 — DEV REPLAY BUFFER VALIDATION: FAILED\n"
            << "============================================================\n";

        return 1;
    }


    std::cout
        << "\n"
        << "============================================================\n"
        << "DEVAI PHASE 3.2 — DEV REPLAY BUFFER VALIDATION: PASSED\n"
        << "============================================================\n";


    return 0;
}