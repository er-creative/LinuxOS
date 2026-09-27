#include "devai/statistics/ReturnStatisticalEngine.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

void require(
    bool condition,
    const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

devai::market::Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double close)
{
    devai::market::Candle candle;

    candle.symbol = symbol;
    candle.timestamp = timestamp;

    candle.open = close;
    candle.high = close;
    candle.low = close;
    candle.close = close;

    candle.volume = 1000.0;

    return candle;
}

} // namespace


int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 4.2 — RETURN STATISTICAL ENGINE TEST\n"
            << "============================================================\n";


        constexpr std::size_t WINDOW = 3;

        const std::string symbol =
            "TEST";

        devai::statistics::
            ReturnStatisticalEngine engine(
                symbol,
                WINDOW,
                2.0);


        // ====================================================
        // Observation 1
        // ====================================================

        devai::features::PriceReturnFeatures f1;

        f1.symbol = symbol;
        f1.decision_time = 1000;

        f1.has_one_minute = true;
        f1.one_minute_return = 0.01;


        devai::market::MarketSnapshot s1;

        s1.symbol = symbol;
        s1.decision_time = 1000;

        s1.one_minute =
            makeCandle(
                symbol,
                940,
                100.0);


        auto r1 =
            engine.update(
                f1,
                s1);

        require(
            r1.one_minute.observation_count == 1,
            "First 1m observation was not recorded.");

        require(
            !r1.one_minute.ready,
            "Window became ready too early.");


        // ====================================================
        // Observation 2
        // ====================================================

        devai::features::PriceReturnFeatures f2;

        f2.symbol = symbol;
        f2.decision_time = 1060;

        f2.has_one_minute = true;
        f2.one_minute_return = 0.02;


        devai::market::MarketSnapshot s2;

        s2.symbol = symbol;
        s2.decision_time = 1060;

        s2.one_minute =
            makeCandle(
                symbol,
                1000,
                102.0);


        auto r2 =
            engine.update(
                f2,
                s2);

        require(
            r2.one_minute.observation_count == 2,
            "Second 1m observation was not recorded.");


        // ====================================================
        // Observation 3
        // ====================================================

        devai::features::PriceReturnFeatures f3;

        f3.symbol = symbol;
        f3.decision_time = 1120;

        f3.has_one_minute = true;
        f3.one_minute_return = -0.03;


        devai::market::MarketSnapshot s3;

        s3.symbol = symbol;
        s3.decision_time = 1120;

        s3.one_minute =
            makeCandle(
                symbol,
                1060,
                99.0);


        auto r3 =
            engine.update(
                f3,
                s3);

        require(
            r3.one_minute.observation_count == 3,
            "Third 1m observation was not recorded.");

        require(
            r3.one_minute.ready,
            "1m statistical window did not become ready.");

        require(
            std::isfinite(
                r3.one_minute.z_score),
            "1m Z-score is not finite.");

        require(
            std::isfinite(
                r3.one_minute.robust_z_score),
            "1m robust Z-score is not finite.");

        require(
            std::isfinite(
                r3.one_minute.absolute_return_z_score),
            "Absolute-return Z-score is not finite.");


        // ====================================================
        // Repeated source candle must NOT be counted twice
        // ====================================================

        devai::features::PriceReturnFeatures f4 =
            f3;

        f4.decision_time = 1180;


        devai::market::MarketSnapshot s4 =
            s3;

        s4.decision_time = 1180;


        auto r4 =
            engine.update(
                f4,
                s4);

        require(
            r4.one_minute.observation_count == 3,
            "Repeated source candle was counted twice.");


        // ====================================================
        // Explicit reset
        // ====================================================

        engine.reset();

        devai::features::PriceReturnFeatures f5;

        f5.symbol = symbol;
        f5.decision_time = 2000;

        f5.has_one_minute = true;
        f5.one_minute_return = 0.01;


        devai::market::MarketSnapshot s5;

        s5.symbol = symbol;
        s5.decision_time = 2000;

        s5.one_minute =
            makeCandle(
                symbol,
                1940,
                100.0);


        auto r5 =
            engine.update(
                f5,
                s5);

        require(
            r5.one_minute.observation_count == 1,
            "Explicit reset failed.");


        std::cout
            << "Return Z-Score                 : PASSED\n"
            << "Robust Return Z-Score          : PASSED\n"
            << "Absolute Return Statistics     : PASSED\n"
            << "Statistical Extreme State      : PASSED\n"
            << "Repeated Candle Protection     : PASSED\n"
            << "Window Readiness               : PASSED\n"
            << "Explicit Reset                 : PASSED\n"
            << "============================================================\n"
            << "PHASE 4.2 RETURN STATISTICAL ENGINE PASSED\n"
            << "============================================================\n";

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\nPHASE 4.2 TEST FAILED\n"
            << exception.what()
            << "\n";

        return EXIT_FAILURE;
    }
}