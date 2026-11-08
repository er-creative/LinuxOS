#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "devai/strategy/outcome/ForwardReturnOutcomeEngine.hpp"


namespace
{

using namespace devai::strategy::outcome;


constexpr double TOLERANCE =
    1.0e-12;


// ============================================================================
// approximatelyEqual
// ============================================================================

bool approximatelyEqual(
    double left,
    double right)
{
    return std::fabs(
        left - right) <= TOLERANCE;
}


// ============================================================================
// require
// ============================================================================

void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(
            message);
    }
}


// ============================================================================
// requireThrows
// ============================================================================

template <typename Function>
void requireThrows(
    Function&& function,
    const std::string& message)
{
    bool threw =
        false;


    try
    {
        function();
    }
    catch (const std::exception&)
    {
        threw =
            true;
    }


    require(
        threw,
        message);
}


// ============================================================================
// makeBars
//
// 10 chronological historical bars:
//
// index   close
//
//   0     100
//   1     101
//   2     102
//   3     103
//   4     104
//   5     105
//   6     100
//   7      95
//   8     110
//   9     120
//
// Times are deliberately simple monotonic test timestamps.
// ============================================================================

std::vector<ForwardReturnBar>
makeBars()
{
    return
    {
        {1000, 100.0},
        {1060, 101.0},
        {1120, 102.0},
        {1180, 103.0},
        {1240, 104.0},
        {1300, 105.0},
        {1360, 100.0},
        {1420,  95.0},
        {1480, 110.0},
        {1540, 120.0}
    };
}

} // namespace


int main()
{
    std::cout
        << "\n"
        << "==========================================================================\n"
        << "CETE PHASE 2.9 — FORWARD-RETURN OUTCOME ENGINE VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        ForwardReturnOutcomeEngine engine;


        const auto bars =
            makeBars();


        // ====================================================================
        // 1. Positive 1-bar forward return
        //
        // 100 -> 101 = +1%
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    0,
                    1);


            require(
                result.available,
                "1-bar forward return should be available.");


            require(
                approximatelyEqual(
                    result.forward_return,
                    0.01),
                "Positive 1-bar forward return is incorrect.");


            require(
                result.origin_index == 0 &&
                result.outcome_index == 1,
                "1-bar origin/outcome indices are incorrect.");


            std::cout
                << "[PASS] Positive 1-bar forward return\n";
        }


        // ====================================================================
        // 2. Positive 5-bar forward return
        //
        // 100 -> 105 = +5%
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    0,
                    5);


            require(
                result.available,
                "5-bar forward return should be available.");


            require(
                approximatelyEqual(
                    result.forward_return,
                    0.05),
                "Positive 5-bar forward return is incorrect.");


            require(
                result.outcome_index == 5,
                "5-bar outcome index is incorrect.");


            require(
                result.origin_time == 1000 &&
                result.outcome_time == 1300,
                "5-bar timestamps are incorrect.");


            std::cout
                << "[PASS] Positive 5-bar forward return\n";
        }


        // ====================================================================
        // 3. Negative forward return
        //
        // index 5 -> index 7
        //
        // 105 -> 95
        //
        // = 95 / 105 - 1
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    5,
                    2);


            const double expected =
                (95.0 / 105.0) - 1.0;


            require(
                result.available,
                "Negative forward return should be available.");


            require(
                approximatelyEqual(
                    result.forward_return,
                    expected),
                "Negative forward return is incorrect.");


            require(
                result.forward_return < 0.0,
                "Negative forward return has wrong sign.");


            std::cout
                << "[PASS] Negative forward return\n";
        }


        // ====================================================================
        // 4. Zero forward return
        //
        // index 0 -> index 6
        //
        // 100 -> 100
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    0,
                    6);


            require(
                result.available,
                "Zero-return outcome should be available.");


            require(
                approximatelyEqual(
                    result.forward_return,
                    0.0),
                "Zero forward return is incorrect.");


            std::cout
                << "[PASS] Zero forward return\n";
        }


        // ====================================================================
        // 5. Exact horizon indexing
        //
        // origin 2 + horizon 3 = outcome index 5
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    2,
                    3);


            require(
                result.available,
                "Exact horizon outcome should be available.");


            require(
                result.origin_index == 2,
                "Origin index changed unexpectedly.");


            require(
                result.outcome_index == 5,
                "Engine did not use exact t+N outcome bar.");


            require(
                approximatelyEqual(
                    result.origin_close,
                    102.0),
                "Origin close is incorrect.");


            require(
                approximatelyEqual(
                    result.outcome_close,
                    105.0),
                "Outcome close is incorrect.");


            std::cout
                << "[PASS] Exact t+N horizon indexing\n";
        }


        // ====================================================================
        // 6. Incomplete future horizon
        //
        // index 8 + horizon 5 does not exist.
        //
        // Engine must NOT substitute index 9.
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    8,
                    5);


            require(
                !result.available,
                "Incomplete horizon was incorrectly marked available.");


            require(
                result.outcome_time == 0,
                "Unavailable outcome leaked a future timestamp.");


            require(
                approximatelyEqual(
                    result.outcome_close,
                    0.0),
                "Unavailable outcome leaked a future close.");


            require(
                approximatelyEqual(
                    result.forward_return,
                    0.0),
                "Unavailable outcome produced a forward return.");


            std::cout
                << "[PASS] Incomplete horizon remains unavailable\n";
        }


        // ====================================================================
        // 7. Final bar cannot have positive-horizon outcome
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    bars.size() - 1,
                    1);


            require(
                !result.available,
                "Final bar incorrectly received a future outcome.");


            std::cout
                << "[PASS] Final-bar future outcome unavailable\n";
        }


        // ====================================================================
        // 8. Horizon zero rejected
        // ====================================================================

        requireThrows(
            [&engine, &bars]()
            {
                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        bars,
                        0,
                        0);
            },
            "Zero forward horizon was accepted.");


        std::cout
            << "[PASS] Zero-horizon rejection\n";


        // ====================================================================
        // 9. Origin index out of range
        // ====================================================================

        requireThrows(
            [&engine, &bars]()
            {
                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        bars,
                        bars.size(),
                        1);
            },
            "Out-of-range origin index was accepted.");


        std::cout
            << "[PASS] Out-of-range origin rejection\n";


        // ====================================================================
        // 10. Empty historical sequence rejected
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                const std::vector<ForwardReturnBar> empty;

                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        empty,
                        0,
                        1);
            },
            "Empty historical sequence was accepted.");


        std::cout
            << "[PASS] Empty-sequence rejection\n";


        // ====================================================================
        // 11. Invalid close rejected
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                const std::vector<ForwardReturnBar> invalid =
                {
                    {1000, 100.0},
                    {1060,   0.0}
                };


                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        invalid,
                        0,
                        1);
            },
            "Zero close was accepted.");


        std::cout
            << "[PASS] Invalid-close rejection\n";


        // ====================================================================
        // 12. Non-finite close rejected
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                const std::vector<ForwardReturnBar> invalid =
                {
                    {
                        1000,
                        std::numeric_limits<double>::quiet_NaN()
                    },
                    {
                        1060,
                        101.0
                    }
                };


                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        invalid,
                        0,
                        1);
            },
            "Non-finite close was accepted.");


        std::cout
            << "[PASS] Non-finite close rejection\n";


        // ====================================================================
        // 13. Duplicate time rejected
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                const std::vector<ForwardReturnBar> invalid =
                {
                    {1000, 100.0},
                    {1000, 101.0}
                };


                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        invalid,
                        0,
                        1);
            },
            "Duplicate decision time was accepted.");


        std::cout
            << "[PASS] Duplicate-time rejection\n";


        // ====================================================================
        // 14. Backward time rejected
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                const std::vector<ForwardReturnBar> invalid =
                {
                    {1060, 100.0},
                    {1000, 101.0}
                };


                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        invalid,
                        0,
                        1);
            },
            "Backward decision time was accepted.");


        std::cout
            << "[PASS] Backward-time rejection\n";


        // ====================================================================
        // 15. Invalid decision time rejected
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                const std::vector<ForwardReturnBar> invalid =
                {
                    {0,    100.0},
                    {1060, 101.0}
                };


                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        invalid,
                        0,
                        1);
            },
            "Invalid decision time was accepted.");


        std::cout
            << "[PASS] Invalid decision-time rejection\n";


        // ====================================================================
        // 16. Pure calculate() validation
        // ====================================================================

        {
            const double result =
                ForwardReturnOutcomeEngine::calculate(
                    200.0,
                    220.0);


            require(
                approximatelyEqual(
                    result,
                    0.10),
                "Pure forward-return calculation is incorrect.");


            std::cout
                << "[PASS] Pure forward-return calculation\n";
        }


        // ====================================================================
        // 17. evaluateAll size
        // ====================================================================

        const auto all =
            engine.evaluateAll(
                bars,
                3);


        require(
            all.size() ==
                bars.size(),
            "evaluateAll returned incorrect number of records.");


        std::cout
            << "[PASS] Full-sequence outcome generation\n";


        // ====================================================================
        // 18. evaluateAll maturity boundary
        //
        // With 10 bars and horizon 3:
        //
        // available origins:
        //
        //     0..6
        //
        // unavailable origins:
        //
        //     7..9
        // ====================================================================

        {
            for (std::size_t index = 0;
                 index <= 6;
                 ++index)
            {
                require(
                    all[index].available,
                    "Mature outcome unexpectedly unavailable.");
            }


            for (std::size_t index = 7;
                 index < all.size();
                 ++index)
            {
                require(
                    !all[index].available,
                    "Immature outcome unexpectedly available.");
            }


            std::cout
                << "[PASS] Outcome-maturity boundary\n";
        }


        // ====================================================================
        // 19. Exact number of unavailable outcomes
        // ====================================================================

        {
            std::size_t unavailable_count =
                0;


            for (const auto& result : all)
            {
                if (!result.available)
                {
                    ++unavailable_count;
                }
            }


            require(
                unavailable_count == 3,
                "Incorrect number of immature outcomes.");


            std::cout
                << "[PASS] Exact immature-outcome count\n";
        }


        // ====================================================================
        // 20. No partial-horizon substitution
        //
        // index 7 has bars 8 and 9 available, but horizon=3 requires index 10.
        //
        // It MUST remain unavailable.
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    7,
                    3);


            require(
                !result.available,
                "Partial future horizon was incorrectly substituted.");


            std::cout
                << "[PASS] No partial-horizon substitution\n";
        }


        // ====================================================================
        // 21. Large horizon safely unavailable
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    bars,
                    0,
                    std::numeric_limits<std::size_t>::max());


            require(
                !result.available,
                "Huge horizon was incorrectly marked available.");


            std::cout
                << "[PASS] Large-horizon safety\n";
        }


        // ====================================================================
        // 22. Determinism
        // ====================================================================

        {
            const auto first =
                engine.evaluate(
                    bars,
                    1,
                    5);


            const auto second =
                engine.evaluate(
                    bars,
                    1,
                    5);


            require(
                first.origin_index ==
                    second.origin_index &&
                first.horizon_bars ==
                    second.horizon_bars &&
                first.outcome_index ==
                    second.outcome_index &&
                first.origin_time ==
                    second.origin_time &&
                first.outcome_time ==
                    second.outcome_time &&
                approximatelyEqual(
                    first.origin_close,
                    second.origin_close) &&
                approximatelyEqual(
                    first.outcome_close,
                    second.outcome_close) &&
                approximatelyEqual(
                    first.forward_return,
                    second.forward_return) &&
                first.available ==
                    second.available &&
                first.numerical_integrity ==
                    second.numerical_integrity,
                "Forward-return calculation is not deterministic.");


            std::cout
                << "[PASS] Deterministic outcome calculation\n";
        }


        // ====================================================================
        // 23. Numerical integrity
        // ====================================================================

        {
            for (const auto& result : all)
            {
                require(
                    result.numerical_integrity,
                    "Outcome numerical-integrity flag failed.");
            }


            std::cout
                << "[PASS] Numerical integrity\n";
        }


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Component owner            : CETE SHARED BASE\n"
            << "Outcome type               : REALIZED HISTORICAL OUTCOME\n"
            << "N-bar forward return       : VERIFIED\n"
            << "Positive return            : VERIFIED\n"
            << "Negative return            : VERIFIED\n"
            << "Zero return                : VERIFIED\n"
            << "Exact t+N indexing         : VERIFIED\n"
            << "Outcome maturity           : VERIFIED\n"
            << "Incomplete horizon         : UNAVAILABLE\n"
            << "Partial-horizon fallback   : NONE\n"
            << "Chronological integrity    : VERIFIED\n"
            << "Numerical integrity        : VERIFIED\n"
            << "Determinism                : VERIFIED\n"
            << "Forecasting                : NONE\n"
            << "Signal generation          : NONE\n"
            << "Trade execution            : NONE\n"
            << "P&L calculation            : NONE\n"
            << "Reward calculation         : NONE\n"
            << "Dev observation dependency : NONE\n"
            << "Future-data extrapolation  : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "CETE PHASE 2.9 FORWARD-RETURN OUTCOME ENGINE VALIDATION PASSED\n"
            << "==========================================================================\n";


        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n[FAIL] "
            << exception.what()
            << "\n";


        return 1;
    }
}