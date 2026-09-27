#include "devai/features/VolumeFeatureEngine.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/SessionState.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace devai::features;
using namespace devai::market;


namespace
{

bool approximatelyEqual(
    double a,
    double b,
    double tolerance = 1e-10)
{
    return
        std::fabs(a - b) <=
        tolerance *
        std::max(
            1.0,
            std::max(
                std::fabs(a),
                std::fabs(b)
            )
        );
}


std::int64_t makeTimestamp(
    int year,
    int month,
    int day,
    int hour,
    int minute)
{
    std::tm value{};

    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = 0;
    value.tm_isdst = -1;

    return
        static_cast<std::int64_t>(
            std::mktime(&value)
        );
}


Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double volume)
{
    Candle candle;

    candle.symbol = symbol;
    candle.timestamp = timestamp;

    candle.open = 100.0;
    candle.high = 101.0;
    candle.low = 99.0;
    candle.close = 100.5;

    candle.volume = volume;

    return candle;
}


MarketSnapshot makeSnapshot(
    const std::string& symbol,
    std::int64_t decision_time,
    const TradingDate& trading_date,
    bool new_session,
    const std::optional<Candle>& one = std::nullopt,
    const std::optional<Candle>& five = std::nullopt,
    const std::optional<Candle>& fifteen = std::nullopt)
{
    MarketSnapshot snapshot;

    snapshot.symbol = symbol;
    snapshot.decision_time = decision_time;
    snapshot.trading_date = trading_date;
    snapshot.session_phase = SessionPhase::ACTIVE;
    snapshot.new_session = new_session;

    snapshot.one_minute = one;
    snapshot.five_minute = five;
    snapshot.fifteen_minute = fifteen;

    return snapshot;
}


// ============================================================================
// Average Volume 20 / RVOL20
// ============================================================================

void testAverage20()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    VolumeFeatures features;


    for (std::size_t i = 0; i < 20; ++i)
    {
        const double volume =
            100.0 +
            static_cast<double>(i);


        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                volume
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );
    }


    // Mean of 100 ... 119 = 109.5

    assert(
        features.one_minute.has_average_volume_20
    );

    assert(
        approximatelyEqual(
            features.one_minute.average_volume_20,
            109.5
        )
    );

    assert(
        features.one_minute.has_relative_volume_20
    );

    assert(
        approximatelyEqual(
            features.one_minute.relative_volume_20,
            119.0 / 109.5
        )
    );
}


// ============================================================================
// Average Volume 50 / RVOL50
// ============================================================================

void testAverage50()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    VolumeFeatures features;


    for (std::size_t i = 0; i < 50; ++i)
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                100.0 +
                    static_cast<double>(i)
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );
    }


    // Mean 100 ... 149 = 124.5

    assert(
        features.one_minute.has_average_volume_50
    );

    assert(
        approximatelyEqual(
            features.one_minute.average_volume_50,
            124.5
        )
    );

    assert(
        features.one_minute.has_relative_volume_50
    );

    assert(
        approximatelyEqual(
            features.one_minute.relative_volume_50,
            149.0 / 124.5
        )
    );
}


// ============================================================================
// Session cumulative volume
// ============================================================================

void testSessionVolume()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    VolumeFeatures features;


    for (std::size_t i = 0; i < 3; ++i)
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                1000.0 +
                    1000.0 *
                    static_cast<double>(i)
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );
    }


    assert(
        features.session_bar_count == 3
    );

    assert(
        approximatelyEqual(
            features.session_cumulative_volume,
            6000.0
        )
    );

    assert(
        approximatelyEqual(
            features.session_average_volume_per_bar,
            2000.0
        )
    );
}


// ============================================================================
// Duplicate protection
// ============================================================================

void testDuplicateProtection()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto timestamp =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    const Candle candle =
        makeCandle(
            symbol,
            timestamp,
            1000.0
        );


    const auto first =
        engine.update(
            makeSnapshot(
                symbol,
                timestamp + 60,
                date,
                true,
                candle
            )
        );


    const auto duplicate =
        engine.update(
            makeSnapshot(
                symbol,
                timestamp + 60,
                date,
                true,
                candle
            )
        );


    assert(
        first.session_bar_count == 1
    );

    assert(
        duplicate.session_bar_count == 1
    );

    assert(
        approximatelyEqual(
            duplicate.session_cumulative_volume,
            1000.0
        )
    );
}


// ============================================================================
// Cross-session behavior
// ============================================================================

void testCrossSessionBehavior()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);


    const TradingDate day_one{
        2026,
        9,
        25
    };


    const TradingDate day_two{
        2026,
        9,
        28
    };


    const auto base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    VolumeFeatures before;


    // Warm 50-volume continuous history.

    for (std::size_t i = 0; i < 50; ++i)
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                100.0 +
                    static_cast<double>(i)
            );


        before =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    day_one,
                    i == 0,
                    candle
                )
            );
    }


    assert(
        before.one_minute.has_average_volume_50
    );

    assert(
        before.session_bar_count == 50
    );


    const auto next_session =
        makeTimestamp(
            2026,
            9,
            28,
            9,
            15
        );


    const Candle first_day_two =
        makeCandle(
            symbol,
            next_session,
            200.0
        );


    const auto after =
        engine.update(
            makeSnapshot(
                symbol,
                next_session + 60,
                day_two,
                true,
                first_day_two
            )
        );


    // Continuous history survives.

    assert(
        after.one_minute.has_average_volume_50
    );


    // Session state resets.

    assert(
        after.session_bar_count == 1
    );

    assert(
        approximatelyEqual(
            after.session_cumulative_volume,
            200.0
        )
    );


    // Duplicate first snapshot must NOT reset or double count.

    const auto duplicate =
        engine.update(
            makeSnapshot(
                symbol,
                next_session + 60,
                day_two,
                true,
                first_day_two
            )
        );


    assert(
        duplicate.session_bar_count == 1
    );

    assert(
        approximatelyEqual(
            duplicate.session_cumulative_volume,
            200.0
        )
    );
}


// ============================================================================
// Multi-timeframe
// ============================================================================

void testMultiTimeframeVolume()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    VolumeFeatures features;


    for (std::size_t i = 0; i < 50; ++i)
    {
        const Candle one =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(i * 60),
                100.0 +
                    static_cast<double>(i)
            );


        const Candle five =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(i * 300),
                200.0 +
                    static_cast<double>(i)
            );


        const Candle fifteen =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(i * 900),
                300.0 +
                    static_cast<double>(i)
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    one.timestamp + 60,
                    date,
                    i == 0,
                    one,
                    five,
                    fifteen
                )
            );
    }


    assert(
        features.one_minute.has_average_volume_50
    );

    assert(
        features.five_minute.has_average_volume_50
    );

    assert(
        features.fifteen_minute.has_average_volume_50
    );


    assert(
        features.one_minute.has_relative_volume_50
    );

    assert(
        features.five_minute.has_relative_volume_50
    );

    assert(
        features.fifteen_minute.has_relative_volume_50
    );
}


// ============================================================================
// Symbol protection
// ============================================================================

void testSymbolProtection()
{
    VolumeFeatureEngine engine("TEST");

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto timestamp =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    bool rejected = false;


    try
    {
        static_cast<void>(
            engine.update(
                makeSnapshot(
                    "WRONG",
                    timestamp + 60,
                    date,
                    true,
                    makeCandle(
                        "WRONG",
                        timestamp,
                        1000.0
                    )
                )
            )
        );
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }


    assert(rejected);
}


// ============================================================================
// Backward decision-time protection
// ============================================================================

void testBackwardTimeProtection()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto timestamp =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    static_cast<void>(
        engine.update(
            makeSnapshot(
                symbol,
                timestamp + 120,
                date,
                true,
                makeCandle(
                    symbol,
                    timestamp,
                    1000.0
                )
            )
        )
    );


    bool rejected = false;


    try
    {
        static_cast<void>(
            engine.update(
                makeSnapshot(
                    symbol,
                    timestamp + 60,
                    date,
                    false,
                    makeCandle(
                        symbol,
                        timestamp + 60,
                        1100.0
                    )
                )
            )
        );
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }


    assert(rejected);
}


// ============================================================================
// Explicit reset
// ============================================================================

void testExplicitReset()
{
    const std::string symbol = "TEST";

    VolumeFeatureEngine engine(symbol);

    const TradingDate date{
        2026,
        9,
        25
    };

    const auto base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    VolumeFeatures features;


    for (std::size_t i = 0; i < 50; ++i)
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(i * 60),
                100.0 +
                    static_cast<double>(i)
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );
    }


    assert(
        features.one_minute.has_average_volume_50
    );

    assert(
        features.session_bar_count == 50
    );


    engine.reset();


    const TradingDate restart_date{
        2026,
        9,
        28
    };


    const auto restart =
        makeTimestamp(
            2026,
            9,
            28,
            9,
            15
        );


    features =
        engine.update(
            makeSnapshot(
                symbol,
                restart + 60,
                restart_date,
                true,
                makeCandle(
                    symbol,
                    restart,
                    500.0
                )
            )
        );


    assert(
        !features.one_minute.has_average_volume_20
    );

    assert(
        !features.one_minute.has_average_volume_50
    );

    assert(
        features.session_bar_count == 1
    );

    assert(
        approximatelyEqual(
            features.session_cumulative_volume,
            500.0
        )
    );
}

} // namespace


int main()
{
    testAverage20();

    testAverage50();

    testSessionVolume();

    testDuplicateProtection();

    testCrossSessionBehavior();

    testMultiTimeframeVolume();

    testSymbolProtection();

    testBackwardTimeProtection();

    testExplicitReset();


    std::cout
        << "\n"
        << "================================================\n"
        << "VolumeFeatureEngine Tests PASSED\n"
        << "================================================\n"
        << "Average Volume 20             : PASSED\n"
        << "Average Volume 50             : PASSED\n"
        << "RVOL20                        : PASSED\n"
        << "RVOL50                        : PASSED\n"
        << "Session cumulative volume     : PASSED\n"
        << "Session bar count             : PASSED\n"
        << "Session average volume        : PASSED\n"
        << "Duplicate candle protection   : PASSED\n"
        << "Cross-session rolling history : PASSED\n"
        << "New-session state reset       : PASSED\n"
        << "Duplicate new-session safety  : PASSED\n"
        << "1m / 5m / 15m volume         : PASSED\n"
        << "Symbol protection             : PASSED\n"
        << "Backward-time protection      : PASSED\n"
        << "Explicit reset                : PASSED\n"
        << "================================================\n";


    return 0;
}