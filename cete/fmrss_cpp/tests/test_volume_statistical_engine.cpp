#include "devai/statistics/VolumeStatisticalEngine.hpp"

#include "devai/features/VolumeFeatures.hpp"
#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using devai::features::VolumeFeatures;

using devai::market::Candle;
using devai::market::MarketSnapshot;

using devai::statistics::AbnormalVolumeState;
using devai::statistics::VolumeStatisticalEngine;


// ============================================================================
// Constants
// ============================================================================

constexpr double EPSILON = 1e-9;


// ============================================================================
// Utility
// ============================================================================

bool approximatelyEqual(
    double lhs,
    double rhs,
    double tolerance = EPSILON)
{
    if (std::isnan(lhs) &&
        std::isnan(rhs))
    {
        return true;
    }

    if (!std::isfinite(lhs) ||
        !std::isfinite(rhs))
    {
        return lhs == rhs;
    }

    const double scale =
        std::max(
            1.0,
            std::max(
                std::fabs(lhs),
                std::fabs(rhs)));

    return
        std::fabs(lhs - rhs) <=
        tolerance * scale;
}


// ============================================================================
// Test reporting
// ============================================================================

struct TestReport
{
    bool volume_z_score{true};

    bool robust_volume_z_score{true};

    bool rvol20_statistics{true};

    bool rvol50_statistics{true};

    bool abnormal_volume_state{true};

    bool repeated_candle_protection{true};

    bool window_readiness{true};

    bool independent_metric_warmup{true};

    bool cross_session_continuity{true};

    bool explicit_reset{true};

    bool backward_time_protection{true};

    bool duplicate_time_protection{true};

    bool symbol_protection{true};

    bool decision_time_protection{true};

    bool numeric_integrity{true};


    [[nodiscard]]
    bool passed() const noexcept
    {
        return
            volume_z_score &&
            robust_volume_z_score &&
            rvol20_statistics &&
            rvol50_statistics &&
            abnormal_volume_state &&
            repeated_candle_protection &&
            window_readiness &&
            independent_metric_warmup &&
            cross_session_continuity &&
            explicit_reset &&
            backward_time_protection &&
            duplicate_time_protection &&
            symbol_protection &&
            decision_time_protection &&
            numeric_integrity;
    }
};


// ============================================================================
// Candle factory
// ============================================================================

Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double volume)
{
    Candle candle;

    candle.symbol =
        symbol;

    candle.timestamp =
        timestamp;

    candle.open =
        100.0;

    candle.high =
        101.0;

    candle.low =
        99.0;

    candle.close =
        100.5;

    candle.volume =
        volume;

    return candle;
}


// ============================================================================
// Snapshot factory
//
// The statistical engine uses MarketSnapshot only for:
//
//     symbol
//     decision_time
//     source candle timestamps
//
// No statistical value is calculated from MarketSnapshot.
// ============================================================================

MarketSnapshot makeSnapshot(
    const std::string& symbol,
    std::int64_t decision_time,
    std::int64_t one_minute_timestamp,
    std::int64_t five_minute_timestamp,
    std::int64_t fifteen_minute_timestamp)
{
    MarketSnapshot snapshot;

    snapshot.symbol =
        symbol;

    snapshot.decision_time =
        decision_time;


    snapshot.one_minute =
        makeCandle(
            symbol,
            one_minute_timestamp,
            1000.0);


    snapshot.five_minute =
        makeCandle(
            symbol,
            five_minute_timestamp,
            5000.0);


    snapshot.fifteen_minute =
        makeCandle(
            symbol,
            fifteen_minute_timestamp,
            15000.0);


    return snapshot;
}


// ============================================================================
// VolumeFeatures factory
// ============================================================================

VolumeFeatures makeVolumeFeatures(
    const std::string& symbol,
    std::int64_t decision_time,

    double one_minute_volume,
    double five_minute_volume,
    double fifteen_minute_volume,

    double one_minute_rvol20,
    double five_minute_rvol20,
    double fifteen_minute_rvol20,

    double one_minute_rvol50,
    double five_minute_rvol50,
    double fifteen_minute_rvol50,

    bool has_rvol20 = true,
    bool has_rvol50 = true)
{
    VolumeFeatures features;

    features.symbol =
        symbol;

    features.decision_time =
        decision_time;


    // ========================================================================
    // 1 minute
    // ========================================================================

    features.one_minute.volume =
        one_minute_volume;

    features.one_minute.has_volume =
        true;


    features.one_minute.relative_volume_20 =
        one_minute_rvol20;

    features.one_minute.has_relative_volume_20 =
        has_rvol20;


    features.one_minute.relative_volume_50 =
        one_minute_rvol50;

    features.one_minute.has_relative_volume_50 =
        has_rvol50;


    // ========================================================================
    // 5 minute
    // ========================================================================

    features.five_minute.volume =
        five_minute_volume;

    features.five_minute.has_volume =
        true;


    features.five_minute.relative_volume_20 =
        five_minute_rvol20;

    features.five_minute.has_relative_volume_20 =
        has_rvol20;


    features.five_minute.relative_volume_50 =
        five_minute_rvol50;

    features.five_minute.has_relative_volume_50 =
        has_rvol50;


    // ========================================================================
    // 15 minute
    // ========================================================================

    features.fifteen_minute.volume =
        fifteen_minute_volume;

    features.fifteen_minute.has_volume =
        true;


    features.fifteen_minute.relative_volume_20 =
        fifteen_minute_rvol20;

    features.fifteen_minute.has_relative_volume_20 =
        has_rvol20;


    features.fifteen_minute.relative_volume_50 =
        fifteen_minute_rvol50;

    features.fifteen_minute.has_relative_volume_50 =
        has_rvol50;


    return features;
}


// ============================================================================
// TEST 1
//
// Volume Z-score
// Robust Volume Z-score
// RVOL20 statistics
// RVOL50 statistics
// Window readiness
// ============================================================================

void testStatisticalCalculations(
    TestReport& report)
{
    constexpr std::size_t WINDOW_SIZE = 3;

    VolumeStatisticalEngine engine(
        "TEST",
        WINDOW_SIZE);


    // ------------------------------------------------------------------------
    // Observation 1
    // ------------------------------------------------------------------------

    {
        const std::int64_t decision_time =
            1000;

        const auto snapshot =
            makeSnapshot(
                "TEST",
                decision_time,
                940,
                700,
                100);

        const auto features =
            makeVolumeFeatures(
                "TEST",
                decision_time,

                100.0,
                500.0,
                1500.0,

                1.0,
                1.0,
                1.0,

                1.0,
                1.0,
                1.0);


        const auto result =
            engine.update(
                features,
                snapshot);


        if (result.one_minute.volume.ready)
        {
            report.window_readiness =
                false;
        }


        if (result.one_minute.volume.observation_count !=
            1)
        {
            report.window_readiness =
                false;
        }
    }


    // ------------------------------------------------------------------------
    // Observation 2
    // ------------------------------------------------------------------------

    {
        const std::int64_t decision_time =
            1060;

        const auto snapshot =
            makeSnapshot(
                "TEST",
                decision_time,
                1000,
                1000,
                1000);

        const auto features =
            makeVolumeFeatures(
                "TEST",
                decision_time,

                200.0,
                600.0,
                1600.0,

                1.5,
                1.2,
                1.1,

                1.2,
                1.1,
                1.05);


        const auto result =
            engine.update(
                features,
                snapshot);


        if (result.one_minute.volume.ready)
        {
            report.window_readiness =
                false;
        }


        if (result.one_minute.volume.observation_count !=
            2)
        {
            report.window_readiness =
                false;
        }
    }


    // ------------------------------------------------------------------------
    // Observation 3
    //
    // 1m raw volume window:
    //
    //     100, 200, 300
    //
    // Mean = 200
    //
    // Population variance:
    //
    //     ((100-200)^2 +
    //      (200-200)^2 +
    //      (300-200)^2) / 3
    //
    //     = 6666.666666...
    //
    // StdDev = 81.649658...
    // ------------------------------------------------------------------------

    {
        const std::int64_t decision_time =
            1120;

        const auto snapshot =
            makeSnapshot(
                "TEST",
                decision_time,
                1060,
                1060,
                1060);

        const auto features =
            makeVolumeFeatures(
                "TEST",
                decision_time,

                300.0,
                700.0,
                1700.0,

                2.0,
                1.4,
                1.2,

                1.4,
                1.2,
                1.10);


        const auto result =
            engine.update(
                features,
                snapshot);


        const double expected_mean =
            200.0;


        const double expected_stddev =
            std::sqrt(
                20000.0 / 3.0);


        const double expected_z =
            (300.0 - expected_mean) /
            expected_stddev;


        // --------------------------------------------------------------------
        // Raw volume
        // --------------------------------------------------------------------

        if (!approximatelyEqual(
                result.one_minute.volume.rolling_mean,
                expected_mean))
        {
            report.volume_z_score =
                false;
        }


        if (!approximatelyEqual(
                result.one_minute.volume.
                    rolling_standard_deviation,
                expected_stddev))
        {
            report.volume_z_score =
                false;
        }


        if (!approximatelyEqual(
                result.one_minute.volume.z_score,
                expected_z))
        {
            report.volume_z_score =
                false;
        }


        // --------------------------------------------------------------------
        // Median = 200
        //
        // Absolute deviations:
        //
        // 100, 0, 100
        //
        // MAD = 100
        //
        // Robust Z:
        //
        // 0.6744897501960817 * (300 - 200) / 100
        // --------------------------------------------------------------------

        const double expected_robust_z =
            0.6744897501960817;


        if (!approximatelyEqual(
                result.one_minute.volume.
                    rolling_median,
                200.0))
        {
            report.robust_volume_z_score =
                false;
        }


        if (!approximatelyEqual(
                result.one_minute.volume.
                    rolling_mad,
                100.0))
        {
            report.robust_volume_z_score =
                false;
        }


        if (!approximatelyEqual(
                result.one_minute.volume.
                    robust_z_score,
                expected_robust_z))
        {
            report.robust_volume_z_score =
                false;
        }


        // --------------------------------------------------------------------
        // RVOL20
        //
        // Values: 1.0, 1.5, 2.0
        // Mean:   1.5
        // --------------------------------------------------------------------

        if (!approximatelyEqual(
                result.one_minute.rvol20.
                    rolling_mean,
                1.5))
        {
            report.rvol20_statistics =
                false;
        }


        if (!result.one_minute.rvol20.ready)
        {
            report.rvol20_statistics =
                false;
        }


        // --------------------------------------------------------------------
        // RVOL50
        //
        // Values: 1.0, 1.2, 1.4
        // Mean:   1.2
        // --------------------------------------------------------------------

        if (!approximatelyEqual(
                result.one_minute.rvol50.
                    rolling_mean,
                1.2))
        {
            report.rvol50_statistics =
                false;
        }


        if (!result.one_minute.rvol50.ready)
        {
            report.rvol50_statistics =
                false;
        }


        // --------------------------------------------------------------------
        // Readiness
        // --------------------------------------------------------------------

        if (!result.one_minute.volume.ready ||
            !result.one_minute.rvol20.ready ||
            !result.one_minute.rvol50.ready ||
            !result.one_minute.fully_ready)
        {
            report.window_readiness =
                false;
        }


        if (result.one_minute.volume.observation_count !=
                WINDOW_SIZE ||
            result.one_minute.rvol20.observation_count !=
                WINDOW_SIZE ||
            result.one_minute.rvol50.observation_count !=
                WINDOW_SIZE)
        {
            report.window_readiness =
                false;
        }
    }
}


// ============================================================================
// TEST 2
//
// Independent metric warm-up
//
// Raw volume can begin before RVOL20.
// RVOL20 can begin before RVOL50.
// ============================================================================

void testIndependentMetricWarmup(
    TestReport& report)
{
    constexpr std::size_t WINDOW_SIZE = 3;

    VolumeStatisticalEngine engine(
        "TEST",
        WINDOW_SIZE);


    for (int i = 0;
         i < 5;
         ++i)
    {
        const std::int64_t decision_time =
            2000 + i * 60;

        const std::int64_t source_time =
            decision_time - 60;


        const bool has_rvol20 =
            i >= 1;

        const bool has_rvol50 =
            i >= 2;


        const auto snapshot =
            makeSnapshot(
                "TEST",
                decision_time,
                source_time,
                source_time,
                source_time);


        const auto features =
            makeVolumeFeatures(
                "TEST",
                decision_time,

                100.0 + i * 10.0,
                500.0 + i * 10.0,
                1500.0 + i * 10.0,

                1.0 + i * 0.1,
                1.0 + i * 0.1,
                1.0 + i * 0.1,

                1.0 + i * 0.05,
                1.0 + i * 0.05,
                1.0 + i * 0.05,

                has_rvol20,
                has_rvol50);


        const auto result =
            engine.update(
                features,
                snapshot);


        const std::size_t expected_volume_count =
            static_cast<std::size_t>(
                std::min(
                    i + 1,
                    static_cast<int>(
                        WINDOW_SIZE)));


        const std::size_t expected_rvol20_count =
            i >= 1
                ? static_cast<std::size_t>(
                    std::min(
                        i,
                        static_cast<int>(
                            WINDOW_SIZE)))
                : 0;


        const std::size_t expected_rvol50_count =
            i >= 2
                ? static_cast<std::size_t>(
                    std::min(
                        i - 1,
                        static_cast<int>(
                            WINDOW_SIZE)))
                : 0;


        if (result.one_minute.volume.
                observation_count !=
            expected_volume_count)
        {
            report.independent_metric_warmup =
                false;
        }


        if (result.one_minute.rvol20.
                observation_count !=
            expected_rvol20_count)
        {
            report.independent_metric_warmup =
                false;
        }


        if (result.one_minute.rvol50.
                observation_count !=
            expected_rvol50_count)
        {
            report.independent_metric_warmup =
                false;
        }
    }
}


// ============================================================================
// TEST 3
//
// Abnormal-volume state
//
// Four identical observations followed by an outlier produce z = +2 using
// population standard deviation.
// ============================================================================

void testAbnormalVolumeState(
    TestReport& report)
{
    constexpr std::size_t WINDOW_SIZE = 5;

    VolumeStatisticalEngine engine(
        "TEST",
        WINDOW_SIZE);


    devai::statistics::VolumeStatisticalFeatures result;


    for (int i = 0;
         i < 5;
         ++i)
    {
        const std::int64_t decision_time =
            3000 + i * 60;

        const std::int64_t source_time =
            decision_time - 60;


        const double volume =
            i < 4
                ? 100.0
                : 1000.0;


        const auto snapshot =
            makeSnapshot(
                "TEST",
                decision_time,
                source_time,
                source_time,
                source_time);


        const auto features =
            makeVolumeFeatures(
                "TEST",
                decision_time,

                volume,
                volume,
                volume,

                1.0,
                1.0,
                1.0,

                1.0,
                1.0,
                1.0);


        result =
            engine.update(
                features,
                snapshot);
    }


    if (!approximatelyEqual(
            result.one_minute.volume.z_score,
            2.0))
    {
        report.abnormal_volume_state =
            false;
    }


    if (result.one_minute.
            abnormal_volume_state !=
        AbnormalVolumeState::EXTREME_HIGH)
    {
        report.abnormal_volume_state =
            false;
    }
}


// ============================================================================
// TEST 4
//
// Repeated 5m / 15m candle protection
// ============================================================================

void testRepeatedCandleProtection(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    const auto snapshot_1 =
        makeSnapshot(
            "TEST",
            4000,
            3940,
            3700,
            3100);


    const auto features_1 =
        makeVolumeFeatures(
            "TEST",
            4000,

            100.0,
            500.0,
            1500.0,

            1.0,
            1.0,
            1.0,

            1.0,
            1.0,
            1.0);


    const auto result_1 =
        engine.update(
            features_1,
            snapshot_1);


    // 1m source changes.
    // 5m and 15m source timestamps remain unchanged.

    const auto snapshot_2 =
        makeSnapshot(
            "TEST",
            4060,
            4000,
            3700,
            3100);


    const auto features_2 =
        makeVolumeFeatures(
            "TEST",
            4060,

            110.0,
            500.0,
            1500.0,

            1.1,
            1.0,
            1.0,

            1.1,
            1.0,
            1.0);


    const auto result_2 =
        engine.update(
            features_2,
            snapshot_2);


    if (result_2.one_minute.volume.
            observation_count !=
        result_1.one_minute.volume.
            observation_count + 1)
    {
        report.repeated_candle_protection =
            false;
    }


    if (result_2.five_minute.volume.
            observation_count !=
        result_1.five_minute.volume.
            observation_count)
    {
        report.repeated_candle_protection =
            false;
    }


    if (result_2.fifteen_minute.volume.
            observation_count !=
        result_1.fifteen_minute.volume.
            observation_count)
    {
        report.repeated_candle_protection =
            false;
    }
}


// ============================================================================
// TEST 5
//
// Cross-session continuity
//
// Phase 4.3 must not reset continuous statistical history at a session
// boundary.
// ============================================================================

void testCrossSessionContinuity(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    for (int i = 0;
         i < 2;
         ++i)
    {
        const std::int64_t decision_time =
            5000 + i * 60;

        const auto snapshot =
            makeSnapshot(
                "TEST",
                decision_time,
                decision_time - 60,
                decision_time - 300,
                decision_time - 900);


        const auto features =
            makeVolumeFeatures(
                "TEST",
                decision_time,

                100.0 + i * 10.0,
                500.0 + i * 10.0,
                1500.0 + i * 10.0,

                1.0 + i * 0.1,
                1.0 + i * 0.1,
                1.0 + i * 0.1,

                1.0 + i * 0.05,
                1.0 + i * 0.05,
                1.0 + i * 0.05);


        engine.update(
            features,
            snapshot);
    }


    // Simulated next trading session.
    // No engine.reset().

    const std::int64_t next_session_time =
        5000 + 86400;


    const auto next_snapshot =
        makeSnapshot(
            "TEST",
            next_session_time,
            next_session_time - 60,
            next_session_time - 300,
            next_session_time - 900);


    const auto next_features =
        makeVolumeFeatures(
            "TEST",
            next_session_time,

            130.0,
            530.0,
            1530.0,

            1.3,
            1.3,
            1.3,

            1.15,
            1.15,
            1.15);


    const auto result =
        engine.update(
            next_features,
            next_snapshot);


    if (result.one_minute.volume.
            observation_count != 3)
    {
        report.cross_session_continuity =
            false;
    }


    if (!result.one_minute.volume.ready)
    {
        report.cross_session_continuity =
            false;
    }
}


// ============================================================================
// TEST 6
//
// Explicit reset
// ============================================================================

void testExplicitReset(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    {
        const auto snapshot =
            makeSnapshot(
                "TEST",
                6000,
                5940,
                5700,
                5100);


        const auto features =
            makeVolumeFeatures(
                "TEST",
                6000,

                100.0,
                500.0,
                1500.0,

                1.0,
                1.0,
                1.0,

                1.0,
                1.0,
                1.0);


        engine.update(
            features,
            snapshot);
    }


    engine.reset();


    {
        const auto snapshot =
            makeSnapshot(
                "TEST",
                7000,
                6940,
                6700,
                6100);


        const auto features =
            makeVolumeFeatures(
                "TEST",
                7000,

                200.0,
                600.0,
                1600.0,

                1.2,
                1.2,
                1.2,

                1.1,
                1.1,
                1.1);


        const auto result =
            engine.update(
                features,
                snapshot);


        if (result.one_minute.volume.
                observation_count != 1 ||
            result.five_minute.volume.
                observation_count != 1 ||
            result.fifteen_minute.volume.
                observation_count != 1)
        {
            report.explicit_reset =
                false;
        }


        if (result.one_minute.volume.ready ||
            result.five_minute.volume.ready ||
            result.fifteen_minute.volume.ready)
        {
            report.explicit_reset =
                false;
        }
    }
}


// ============================================================================
// TEST 7
//
// Backward decision-time protection
// ============================================================================

void testBackwardTimeProtection(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    const auto first_snapshot =
        makeSnapshot(
            "TEST",
            8000,
            7940,
            7700,
            7100);


    const auto first_features =
        makeVolumeFeatures(
            "TEST",
            8000,

            100.0,
            500.0,
            1500.0,

            1.0,
            1.0,
            1.0,

            1.0,
            1.0,
            1.0);


    engine.update(
        first_features,
        first_snapshot);


    bool exception_thrown =
        false;


    try
    {
        const auto backward_snapshot =
            makeSnapshot(
                "TEST",
                7900,
                7840,
                7600,
                7000);


        const auto backward_features =
            makeVolumeFeatures(
                "TEST",
                7900,

                110.0,
                510.0,
                1510.0,

                1.1,
                1.1,
                1.1,

                1.1,
                1.1,
                1.1);


        engine.update(
            backward_features,
            backward_snapshot);
    }
    catch (const std::runtime_error&)
    {
        exception_thrown =
            true;
    }


    if (!exception_thrown)
    {
        report.backward_time_protection =
            false;
    }
}


// ============================================================================
// TEST 8
//
// Duplicate decision-time protection
// ============================================================================

void testDuplicateDecisionTimeProtection(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    const auto snapshot =
        makeSnapshot(
            "TEST",
            9000,
            8940,
            8700,
            8100);


    const auto features =
        makeVolumeFeatures(
            "TEST",
            9000,

            100.0,
            500.0,
            1500.0,

            1.0,
            1.0,
            1.0,

            1.0,
            1.0,
            1.0);


    engine.update(
        features,
        snapshot);


    bool exception_thrown =
        false;


    try
    {
        engine.update(
            features,
            snapshot);
    }
    catch (const std::runtime_error&)
    {
        exception_thrown =
            true;
    }


    if (!exception_thrown)
    {
        report.duplicate_time_protection =
            false;
    }
}


// ============================================================================
// TEST 9
//
// Symbol protection
// ============================================================================

void testSymbolProtection(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    auto snapshot =
        makeSnapshot(
            "TEST",
            10000,
            9940,
            9700,
            9100);


    auto features =
        makeVolumeFeatures(
            "WRONG",
            10000,

            100.0,
            500.0,
            1500.0,

            1.0,
            1.0,
            1.0,

            1.0,
            1.0,
            1.0);


    bool exception_thrown =
        false;


    try
    {
        engine.update(
            features,
            snapshot);
    }
    catch (const std::invalid_argument&)
    {
        exception_thrown =
            true;
    }


    if (!exception_thrown)
    {
        report.symbol_protection =
            false;
    }
}


// ============================================================================
// TEST 10
//
// Feature / snapshot decision-time mismatch
// ============================================================================

void testDecisionTimeProtection(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    const auto snapshot =
        makeSnapshot(
            "TEST",
            11000,
            10940,
            10700,
            10100);


    const auto features =
        makeVolumeFeatures(
            "TEST",
            11060,

            100.0,
            500.0,
            1500.0,

            1.0,
            1.0,
            1.0,

            1.0,
            1.0,
            1.0);


    bool exception_thrown =
        false;


    try
    {
        engine.update(
            features,
            snapshot);
    }
    catch (const std::invalid_argument&)
    {
        exception_thrown =
            true;
    }


    if (!exception_thrown)
    {
        report.decision_time_protection =
            false;
    }
}


// ============================================================================
// TEST 11
//
// Numeric integrity
//
// If a Phase 3.3 metric says it is available, its value must be finite.
// ============================================================================

void testNumericIntegrity(
    TestReport& report)
{
    VolumeStatisticalEngine engine(
        "TEST",
        3);


    const auto snapshot =
        makeSnapshot(
            "TEST",
            12000,
            11940,
            11700,
            11100);


    auto features =
        makeVolumeFeatures(
            "TEST",
            12000,

            100.0,
            500.0,
            1500.0,

            1.0,
            1.0,
            1.0,

            1.0,
            1.0,
            1.0);


    features.one_minute.volume =
        std::numeric_limits<double>::
            quiet_NaN();


    features.one_minute.has_volume =
        true;


    bool exception_thrown =
        false;


    try
    {
        engine.update(
            features,
            snapshot);
    }
    catch (const std::runtime_error&)
    {
        exception_thrown =
            true;
    }


    if (!exception_thrown)
    {
        report.numeric_integrity =
            false;
    }
}


// ============================================================================
// IMPORTANT
//
// The anonymous namespace opened near the top of this file MUST be closed
// before main().
// ============================================================================

} // namespace


// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.3 — VOLUME STATISTICAL ENGINE TEST\n"
        << "============================================================\n";


    TestReport report;


    try
    {
        testStatisticalCalculations(
            report);

        testIndependentMetricWarmup(
            report);

        testAbnormalVolumeState(
            report);

        testRepeatedCandleProtection(
            report);

        testCrossSessionContinuity(
            report);

        testExplicitReset(
            report);

        testBackwardTimeProtection(
            report);

        testDuplicateDecisionTimeProtection(
            report);

        testSymbolProtection(
            report);

        testDecisionTimeProtection(
            report);

        testNumericIntegrity(
            report);
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\nUnexpected test exception:\n"
            << exception.what()
            << "\n";

        return EXIT_FAILURE;
    }


    const auto printResult =
        [](const std::string& name,
           bool passed)
        {
            std::cout
                << name
                << " : "
                << (passed
                        ? "PASSED"
                        : "FAILED")
                << "\n";
        };


    printResult(
        "Volume Z-Score                 ",
        report.volume_z_score);

    printResult(
        "Robust Volume Z-Score          ",
        report.robust_volume_z_score);

    printResult(
        "RVOL20 Statistical Context     ",
        report.rvol20_statistics);

    printResult(
        "RVOL50 Statistical Context     ",
        report.rvol50_statistics);

    printResult(
        "Abnormal Volume State          ",
        report.abnormal_volume_state);

    printResult(
        "Repeated Candle Protection     ",
        report.repeated_candle_protection);

    printResult(
        "Window Readiness               ",
        report.window_readiness);

    printResult(
        "Independent Metric Warm-up     ",
        report.independent_metric_warmup);

    printResult(
        "Cross-Session Continuity       ",
        report.cross_session_continuity);

    printResult(
        "Explicit Reset                 ",
        report.explicit_reset);

    printResult(
        "Backward-Time Protection       ",
        report.backward_time_protection);

    printResult(
        "Duplicate-Time Protection      ",
        report.duplicate_time_protection);

    printResult(
        "Symbol Protection              ",
        report.symbol_protection);

    printResult(
        "Decision-Time Integrity        ",
        report.decision_time_protection);

    printResult(
        "Numeric Integrity              ",
        report.numeric_integrity);


    std::cout
        << "============================================================\n";


    if (!report.passed())
    {
        std::cout
            << "PHASE 4.3 VOLUME STATISTICAL ENGINE FAILED\n"
            << "============================================================\n";

        return EXIT_FAILURE;
    }


    std::cout
        << "PHASE 4.3 VOLUME STATISTICAL ENGINE PASSED\n"
        << "============================================================\n";


    return EXIT_SUCCESS;
}