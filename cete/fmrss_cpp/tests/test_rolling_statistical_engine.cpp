#include "devai/statistics/RobustStatistics.hpp"
#include "devai/statistics/RollingStatisticalEngine.hpp"
#include "devai/statistics/RollingStatistics.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

constexpr double EPSILON = 1.0e-9;

bool approximatelyEqual(
    double lhs,
    double rhs,
    double tolerance = EPSILON)
{
    return std::abs(lhs - rhs) <= tolerance;
}

void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    try
    {
        using devai::statistics::RobustStatistics;
        using devai::statistics::RollingStatisticalEngine;
        using devai::statistics::RollingStatistics;

        std::cout
            << "\n"
            << "================================================\n"
            << "PHASE 4.1 — ROLLING STATISTICAL ENGINE TEST\n"
            << "================================================\n";

        // ====================================================
        // TEST 1
        // Rolling Mean
        // ====================================================

        {
            RollingStatistics statistics(5);

            statistics.add(1.0);
            statistics.add(2.0);
            statistics.add(3.0);
            statistics.add(4.0);
            statistics.add(5.0);

            require(
                approximatelyEqual(
                    statistics.mean(),
                    3.0),
                "Rolling mean test failed.");

            std::cout
                << "Rolling Mean                  : PASSED\n";
        }

        // ====================================================
        // TEST 2
        // Population Standard Deviation
        //
        // Values:
        // 1,2,3,4,5
        //
        // Mean = 3
        // Variance = 2
        // StdDev = sqrt(2)
        // ====================================================

        {
            RollingStatistics statistics(5);

            statistics.add(1.0);
            statistics.add(2.0);
            statistics.add(3.0);
            statistics.add(4.0);
            statistics.add(5.0);

            const double expected =
                std::sqrt(2.0);

            require(
                approximatelyEqual(
                    statistics.standardDeviation(),
                    expected),
                "Rolling standard deviation test failed.");

            std::cout
                << "Rolling Standard Deviation    : PASSED\n";
        }

        // ====================================================
        // TEST 3
        // Standard Z-score
        //
        // x = 5
        // mean = 3
        // stddev = sqrt(2)
        //
        // z = (5 - 3) / sqrt(2)
        // ====================================================

        {
            RollingStatistics statistics(5);

            statistics.add(1.0);
            statistics.add(2.0);
            statistics.add(3.0);
            statistics.add(4.0);
            statistics.add(5.0);

            const double expected =
                (5.0 - 3.0) / std::sqrt(2.0);

            require(
                approximatelyEqual(
                    statistics.zScore(5.0),
                    expected),
                "Z-score test failed.");

            std::cout
                << "Z-Score                       : PASSED\n";
        }

        // ====================================================
        // TEST 4
        // Median
        // ====================================================

        {
            const std::vector<double> odd_values{
                1.0,
                5.0,
                3.0,
                2.0,
                4.0
            };

            require(
                approximatelyEqual(
                    RobustStatistics::median(odd_values),
                    3.0),
                "Odd median test failed.");

            const std::vector<double> even_values{
                1.0,
                2.0,
                3.0,
                4.0
            };

            require(
                approximatelyEqual(
                    RobustStatistics::median(even_values),
                    2.5),
                "Even median test failed.");

            std::cout
                << "Median                        : PASSED\n";
        }

        // ====================================================
        // TEST 5
        // Median Absolute Deviation
        //
        // Values:
        // 1,2,3,4,5
        //
        // Median = 3
        //
        // Absolute deviations:
        // 2,1,0,1,2
        //
        // MAD = 1
        // ====================================================

        {
            const std::vector<double> values{
                1.0,
                2.0,
                3.0,
                4.0,
                5.0
            };

            require(
                approximatelyEqual(
                    RobustStatistics::
                        medianAbsoluteDeviation(values),
                    1.0),
                "MAD test failed.");

            std::cout
                << "MAD                           : PASSED\n";
        }

        // ====================================================
        // TEST 6
        // Robust Z-score
        // ====================================================

        {
            const std::vector<double> values{
                1.0,
                2.0,
                3.0,
                4.0,
                5.0
            };

            const double expected =
                0.6744897501960817 *
                (5.0 - 3.0);

            require(
                approximatelyEqual(
                    RobustStatistics::robustZScore(
                        5.0,
                        values),
                    expected),
                "Robust Z-score test failed.");

            std::cout
                << "Robust Z-Score                : PASSED\n";
        }

        // ====================================================
        // TEST 7
        // Rolling window eviction
        // ====================================================

        {
            RollingStatistics statistics(3);

            statistics.add(1.0);
            statistics.add(2.0);
            statistics.add(3.0);

            require(
                approximatelyEqual(
                    statistics.mean(),
                    2.0),
                "Initial rolling-window mean failed.");

            statistics.add(4.0);

            // Window must now be:
            // 2,3,4

            require(
                statistics.size() == 3,
                "Rolling window size incorrect.");

            require(
                approximatelyEqual(
                    statistics.mean(),
                    3.0),
                "Rolling-window eviction failed.");

            std::cout
                << "Rolling Window Eviction       : PASSED\n";
        }

        // ====================================================
        // TEST 8
        // Readiness
        // ====================================================

        {
            RollingStatistics statistics(3);

            require(
                !statistics.ready(),
                "Empty statistics unexpectedly ready.");

            statistics.add(1.0);

            require(
                !statistics.ready(),
                "Statistics ready too early.");

            statistics.add(2.0);

            require(
                !statistics.ready(),
                "Statistics ready too early.");

            statistics.add(3.0);

            require(
                statistics.ready(),
                "Statistics failed readiness.");

            std::cout
                << "Window Readiness              : PASSED\n";
        }

        // ====================================================
        // TEST 9
        // Engine integration
        // ====================================================

        {
            RollingStatisticalEngine engine(
                "RELIANCE",
                5);

            devai::statistics::RollingStatisticalFeatures
                features;

            for (std::int64_t i = 1; i <= 5; ++i)
            {
                features =
                    engine.update(
                        1000 + i,
                        static_cast<double>(i));
            }

            require(
                features.symbol == "RELIANCE",
                "Engine symbol mismatch.");

            require(
                features.decision_time == 1005,
                "Engine decision time mismatch.");

            require(
                features.observation_count == 5,
                "Engine observation count mismatch.");

            require(
                features.window_size == 5,
                "Engine window size mismatch.");

            require(
                features.ready,
                "Engine should be ready.");

            require(
                approximatelyEqual(
                    features.rolling_mean,
                    3.0),
                "Engine rolling mean mismatch.");

            require(
                approximatelyEqual(
                    features.rolling_standard_deviation,
                    std::sqrt(2.0)),
                "Engine standard deviation mismatch.");

            require(
                approximatelyEqual(
                    features.rolling_median,
                    3.0),
                "Engine median mismatch.");

            require(
                approximatelyEqual(
                    features.rolling_mad,
                    1.0),
                "Engine MAD mismatch.");

            std::cout
                << "Engine Integration            : PASSED\n";
        }

        // ====================================================
        // TEST 10
        // Continuous history / no session reset
        //
        // The engine has no session concept.
        // History is continuous unless reset() is explicitly
        // called.
        // ====================================================

        {
            RollingStatisticalEngine engine(
                "RELIANCE",
                5);

            engine.update(1001, 1.0);
            engine.update(1002, 2.0);
            engine.update(1003, 3.0);

            // Simulate a large time jump / next trading
            // session. The statistical history must remain.
            engine.update(100000, 4.0);
            const auto features =
                engine.update(100001, 5.0);

            require(
                features.observation_count == 5,
                "Cross-session statistical history lost.");

            require(
                features.ready,
                "Cross-session window should be ready.");

            require(
                approximatelyEqual(
                    features.rolling_mean,
                    3.0),
                "Cross-session rolling mean mismatch.");

            std::cout
                << "Cross-Session Continuity      : PASSED\n";
        }

        // ====================================================
        // TEST 11
        // Explicit reset
        // ====================================================

        {
            RollingStatisticalEngine engine(
                "RELIANCE",
                5);

            engine.update(1001, 1.0);
            engine.update(1002, 2.0);
            engine.update(1003, 3.0);

            engine.reset();

            require(
                engine.observationCount() == 0,
                "Explicit reset failed.");

            require(
                !engine.ready(),
                "Engine ready after reset.");

            const auto features =
                engine.update(2000, 10.0);

            require(
                features.observation_count == 1,
                "Post-reset observation count incorrect.");

            require(
                approximatelyEqual(
                    features.rolling_mean,
                    10.0),
                "Post-reset mean incorrect.");

            std::cout
                << "Explicit Reset                : PASSED\n";
        }

        // ====================================================
        // TEST 12
        // Backward-time protection
        // ====================================================

        {
            RollingStatisticalEngine engine(
                "RELIANCE",
                5);

            engine.update(1000, 1.0);

            bool threw = false;

            try
            {
                engine.update(999, 2.0);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }

            require(
                threw,
                "Backward-time protection failed.");

            std::cout
                << "Backward-Time Protection      : PASSED\n";
        }

        // ====================================================
        // TEST 13
        // Duplicate-time protection
        // ====================================================

        {
            RollingStatisticalEngine engine(
                "RELIANCE",
                5);

            engine.update(1000, 1.0);

            bool threw = false;

            try
            {
                engine.update(1000, 2.0);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }

            require(
                threw,
                "Duplicate-time protection failed.");

            std::cout
                << "Duplicate-Time Protection     : PASSED\n";
        }

        // ====================================================
        // TEST 14
        // Non-finite protection
        // ====================================================

        {
            RollingStatisticalEngine engine(
                "RELIANCE",
                5);

            bool threw = false;

            try
            {
                engine.update(
                    1000,
                    std::numeric_limits<double>::
                        quiet_NaN());
            }
            catch (const std::invalid_argument&)
            {
                threw = true;
            }

            require(
                threw,
                "Non-finite value protection failed.");

            std::cout
                << "Numeric Integrity             : PASSED\n";
        }

        // ====================================================
        // TEST 15
        // Constant series
        //
        // Standard deviation = 0
        // MAD = 0
        //
        // CETE convention:
        // standard Z = 0
        // robust Z   = 0
        // ====================================================

        {
            RollingStatisticalEngine engine(
                "RELIANCE",
                5);

            devai::statistics::RollingStatisticalFeatures
                features;

            for (std::int64_t i = 0; i < 5; ++i)
            {
                features =
                    engine.update(
                        1000 + i,
                        10.0);
            }

            require(
                approximatelyEqual(
                    features.rolling_standard_deviation,
                    0.0),
                "Constant-series stddev failed.");

            require(
                approximatelyEqual(
                    features.rolling_mad,
                    0.0),
                "Constant-series MAD failed.");

            require(
                approximatelyEqual(
                    features.z_score,
                    0.0),
                "Constant-series Z-score failed.");

            require(
                approximatelyEqual(
                    features.robust_z_score,
                    0.0),
                "Constant-series robust Z-score failed.");

            std::cout
                << "Zero-Dispersion Handling      : PASSED\n";
        }

        // ====================================================
        // FINAL
        // ====================================================

        std::cout
            << "================================================\n"
            << "PHASE 4.1 ROLLING STATISTICAL ENGINE PASSED\n"
            << "================================================\n"
            << "Rolling Mean                  : PASSED\n"
            << "Rolling Standard Deviation    : PASSED\n"
            << "Z-Score                       : PASSED\n"
            << "Median                        : PASSED\n"
            << "MAD                           : PASSED\n"
            << "Robust Z-Score                : PASSED\n"
            << "Rolling Window                : PASSED\n"
            << "Cross-Session Continuity      : PASSED\n"
            << "Explicit Reset                : PASSED\n"
            << "Time Protection               : PASSED\n"
            << "Numeric Integrity             : PASSED\n"
            << "================================================\n";

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "================================================\n"
            << "PHASE 4.1 ROLLING STATISTICAL ENGINE FAILED\n"
            << "================================================\n"
            << exception.what()
            << "\n"
            << "================================================\n";

        return EXIT_FAILURE;
    }
}