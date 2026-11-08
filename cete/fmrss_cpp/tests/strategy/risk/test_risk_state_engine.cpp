#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "devai/strategy/risk/RiskStateEngine.hpp"

namespace
{

using namespace devai::strategy::risk;


void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}


bool approximatelyEqual(
    double left,
    double right,
    double absolute_tolerance = 1.0e-9,
    double relative_tolerance = 1.0e-10)
{
    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            });

    return
        std::fabs(left - right) <=
        absolute_tolerance +
        relative_tolerance * scale;
}


template <typename Function>
void requireThrows(
    Function&& function,
    const std::string& message)
{
    bool threw = false;

    try
    {
        function();
    }
    catch (const std::exception&)
    {
        threw = true;
    }

    require(
        threw,
        message);
}

} // namespace


int main()
{
    std::cout
        << "\n"
        << "==========================================================================\n"
        << "CETE PHASE 2.7A — SHARED RISK-STATE ENGINE VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        RiskStateEngine engine;


        // ====================================================================
        // 1. Initial FLAT risk state
        // ====================================================================

        engine.reset(
            {
                1000,
                100000.0,
                100000.0,
                0.0,
                0.0,
                100.0
            });


        const auto initial =
            engine.state();


        require(
            initial.initialized &&
            initial.numerical_integrity,
            "Initial risk state was not initialized correctly.");


        require(
            approximatelyEqual(
                initial.peak_equity,
                100000.0) &&
            approximatelyEqual(
                initial.current_drawdown,
                0.0) &&
            approximatelyEqual(
                initial.maximum_drawdown,
                0.0),
            "Initial drawdown state is incorrect.");


        require(
            approximatelyEqual(
                initial.gross_exposure,
                0.0) &&
            approximatelyEqual(
                initial.exposure_ratio,
                0.0),
            "Initial FLAT exposure is incorrect.");


        std::cout
            << "[PASS] Initial FLAT risk state\n";


        // ====================================================================
        // 2. New equity peak
        // ====================================================================

        const auto peak =
            engine.update(
                {
                    1060,
                    100000.0,
                    105000.0,
                    0.0,
                    0.0,
                    105.0
                });


        require(
            approximatelyEqual(
                peak.peak_equity,
                105000.0) &&
            approximatelyEqual(
                peak.current_drawdown,
                0.0),
            "New equity peak tracking failed.");


        std::cout
            << "[PASS] Peak-equity tracking\n";


        // ====================================================================
        // 3. Drawdown
        //
        // (105000 - 99750) / 105000 = 0.05
        // ====================================================================

        const auto drawdown =
            engine.update(
                {
                    1120,
                    100000.0,
                    99750.0,
                    0.0,
                    0.0,
                    100.0
                });


        require(
            approximatelyEqual(
                drawdown.current_drawdown,
                0.05),
            "Current drawdown calculation failed.");


        require(
            approximatelyEqual(
                drawdown.maximum_drawdown,
                0.05),
            "Maximum drawdown tracking failed.");


        std::cout
            << "[PASS] Current drawdown calculation\n";
        std::cout
            << "[PASS] Maximum drawdown tracking\n";


        // ====================================================================
        // 4. Partial recovery
        // ====================================================================

        const auto recovery =
            engine.update(
                {
                    1180,
                    100000.0,
                    102900.0,
                    0.0,
                    0.0,
                    102.0
                });


        require(
            approximatelyEqual(
                recovery.current_drawdown,
                0.02),
            "Drawdown recovery calculation failed.");


        require(
            approximatelyEqual(
                recovery.maximum_drawdown,
                0.05),
            "Maximum drawdown must survive recovery.");


        std::cout
            << "[PASS] Drawdown recovery\n";


        // ====================================================================
        // 5. New peak resets current drawdown, not maximum drawdown
        // ====================================================================

        const auto new_peak =
            engine.update(
                {
                    1240,
                    100000.0,
                    110000.0,
                    0.0,
                    0.0,
                    110.0
                });


        require(
            approximatelyEqual(
                new_peak.peak_equity,
                110000.0) &&
            approximatelyEqual(
                new_peak.current_drawdown,
                0.0) &&
            approximatelyEqual(
                new_peak.maximum_drawdown,
                0.05),
            "New peak / historical maximum drawdown handling failed.");


        std::cout
            << "[PASS] New-peak drawdown behavior\n";


        // ====================================================================
        // 6. LONG exposure
        //
        // 1 * 800 * 100 = 80000
        // 80000 / 100000 = 0.80
        // ====================================================================

        engine.reset(
            {
                2000,
                100000.0,
                100000.0,
                1.0,
                800.0,
                100.0
            });


        const auto long_state =
            engine.state();


        require(
            approximatelyEqual(
                long_state.gross_exposure,
                80000.0),
            "LONG gross exposure calculation failed.");


        require(
            approximatelyEqual(
                long_state.exposure_ratio,
                0.80),
            "LONG exposure ratio calculation failed.");


        std::cout
            << "[PASS] LONG exposure calculation\n";


        // ====================================================================
        // 7. SHORT exposure
        // ====================================================================

        engine.reset(
            {
                3000,
                100000.0,
                100000.0,
                -1.0,
                500.0,
                120.0
            });


        const auto short_state =
            engine.state();


        require(
            approximatelyEqual(
                short_state.gross_exposure,
                60000.0),
            "SHORT gross exposure calculation failed.");


        require(
            approximatelyEqual(
                short_state.exposure_ratio,
                0.60),
            "SHORT exposure ratio calculation failed.");


        std::cout
            << "[PASS] SHORT exposure calculation\n";


        // ====================================================================
        // 8. FLAT exposure
        // ====================================================================

        engine.reset(
            {
                4000,
                100000.0,
                100000.0,
                0.0,
                0.0,
                250.0
            });


        require(
            approximatelyEqual(
                engine.state().gross_exposure,
                0.0) &&
            approximatelyEqual(
                engine.state().exposure_ratio,
                0.0),
            "FLAT exposure calculation failed.");


        std::cout
            << "[PASS] FLAT exposure calculation\n";


        // ====================================================================
        // 9. Exposure ratio can exceed 1.0
        //
        // Risk engine reports facts; it does not impose policy.
        // ====================================================================

        engine.reset(
            {
                5000,
                100000.0,
                100000.0,
                1.0,
                1200.0,
                100.0
            });


        require(
            approximatelyEqual(
                engine.state().exposure_ratio,
                1.20),
            "Exposure ratio above 1.0 was not represented correctly.");


        std::cout
            << "[PASS] Uncapped exposure-state reporting\n";


        // ====================================================================
        // 10. Strict chronological progression
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                engine.update(
                    {
                        5000,
                        100000.0,
                        100000.0,
                        1.0,
                        1200.0,
                        100.0
                    });
            },
            "Non-advancing decision time was not rejected.");


        std::cout
            << "[PASS] Strict decision-time progression\n";


        // ====================================================================
        // 11. Invalid position rejection
        // ====================================================================

        requireThrows(
            []
            {
                RiskStateEngine local;

                local.reset(
                    {
                        6000,
                        100000.0,
                        100000.0,
                        0.5,
                        100.0,
                        100.0
                    });
            },
            "Invalid position was not rejected.");


        std::cout
            << "[PASS] Invalid-position rejection\n";


        // ====================================================================
        // 12. FLAT nonzero quantity rejection
        // ====================================================================

        requireThrows(
            []
            {
                RiskStateEngine local;

                local.reset(
                    {
                        6000,
                        100000.0,
                        100000.0,
                        0.0,
                        10.0,
                        100.0
                    });
            },
            "FLAT nonzero quantity was not rejected.");


        std::cout
            << "[PASS] FLAT quantity integrity\n";


        // ====================================================================
        // 13. Non-finite input rejection
        // ====================================================================

        requireThrows(
            []
            {
                RiskStateEngine local;

                local.reset(
                    {
                        6000,
                        100000.0,
                        std::numeric_limits<double>::quiet_NaN(),
                        0.0,
                        0.0,
                        100.0
                    });
            },
            "Non-finite equity was not rejected.");


        std::cout
            << "[PASS] Non-finite input rejection\n";


        // ====================================================================
        // 14. Initial-equity mutation rejection
        // ====================================================================

        engine.reset(
            {
                7000,
                100000.0,
                100000.0,
                0.0,
                0.0,
                100.0
            });


        requireThrows(
            [&engine]()
            {
                engine.update(
                    {
                        7060,
                        200000.0,
                        100000.0,
                        0.0,
                        0.0,
                        100.0
                    });
            },
            "Initial-equity mutation was not rejected.");


        std::cout
            << "[PASS] Initial-equity episode integrity\n";


        // ====================================================================
        // 15. Reset isolation
        // ====================================================================

        engine.reset(
            {
                8000,
                100000.0,
                110000.0,
                0.0,
                0.0,
                100.0
            });


        engine.update(
            {
                8060,
                100000.0,
                99000.0,
                0.0,
                0.0,
                100.0
            });


        require(
            engine.state().maximum_drawdown > 0.0,
            "Pre-reset maximum drawdown was not created.");


        engine.reset(
            {
                9000,
                100000.0,
                100000.0,
                0.0,
                0.0,
                100.0
            });


        require(
            approximatelyEqual(
                engine.state().peak_equity,
                100000.0) &&
            approximatelyEqual(
                engine.state().current_drawdown,
                0.0) &&
            approximatelyEqual(
                engine.state().maximum_drawdown,
                0.0),
            "Risk state leaked across reset.");


        std::cout
            << "[PASS] Cross-episode reset isolation\n";


        // ====================================================================
        // 16. Determinism
        // ====================================================================

        RiskStateEngine engine_a;
        RiskStateEngine engine_b;


        const RiskStateInput deterministic_initial
        {
            10000,
            100000.0,
            100000.0,
            1.0,
            500.0,
            100.0
        };


        const RiskStateInput deterministic_update
        {
            10060,
            100000.0,
            95000.0,
            1.0,
            500.0,
            95.0
        };


        engine_a.reset(
            deterministic_initial);

        engine_b.reset(
            deterministic_initial);


        const auto result_a =
            engine_a.update(
                deterministic_update);

        const auto result_b =
            engine_b.update(
                deterministic_update);


        require(
            approximatelyEqual(
                result_a.peak_equity,
                result_b.peak_equity) &&
            approximatelyEqual(
                result_a.current_drawdown,
                result_b.current_drawdown) &&
            approximatelyEqual(
                result_a.maximum_drawdown,
                result_b.maximum_drawdown) &&
            approximatelyEqual(
                result_a.gross_exposure,
                result_b.gross_exposure) &&
            approximatelyEqual(
                result_a.exposure_ratio,
                result_b.exposure_ratio),
            "RiskStateEngine is not deterministic.");


        std::cout
            << "[PASS] Deterministic risk-state calculation\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Component ownership        : CETE SHARED BASE\n"
            << "Peak-equity tracking       : VERIFIED\n"
            << "Current drawdown            : VERIFIED\n"
            << "Maximum drawdown            : VERIFIED\n"
            << "Drawdown recovery           : VERIFIED\n"
            << "LONG exposure               : VERIFIED\n"
            << "SHORT exposure              : VERIFIED\n"
            << "FLAT exposure               : VERIFIED\n"
            << "Exposure ratio              : VERIFIED\n"
            << "Exposure policy             : NONE\n"
            << "Terminal policy             : NONE\n"
            << "Reward calculation          : NONE\n"
            << "Forward-return input        : NONE\n"
            << "Future-price input          : NONE\n"
            << "Cross-episode leakage       : NONE\n"
            << "Numerical integrity         : VERIFIED\n"
            << "Determinism                 : VERIFIED\n"
            << "--------------------------------------------------------------------------\n"
            << "CETE PHASE 2.7A SHARED RISK-STATE ENGINE VALIDATION PASSED\n"
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