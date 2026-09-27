#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace devai::features
{

struct PriceReturnFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};

    // ------------------------------------------------------
    // Current candle return:
    //
    // (Close - Open) / Open
    // ------------------------------------------------------

    double one_minute_candle_return{
        std::numeric_limits<double>::quiet_NaN()
    };

    double five_minute_candle_return{
        std::numeric_limits<double>::quiet_NaN()
    };

    double fifteen_minute_candle_return{
        std::numeric_limits<double>::quiet_NaN()
    };


    // ------------------------------------------------------
    // Close-to-close return
    //
    // (CurrentClose / PreviousClose) - 1
    // ------------------------------------------------------

    double one_minute_return{
        std::numeric_limits<double>::quiet_NaN()
    };

    double five_minute_return{
        std::numeric_limits<double>::quiet_NaN()
    };

    double fifteen_minute_return{
        std::numeric_limits<double>::quiet_NaN()
    };


    // ------------------------------------------------------
    // Log close-to-close return
    //
    // log(CurrentClose / PreviousClose)
    // ------------------------------------------------------

    double one_minute_log_return{
        std::numeric_limits<double>::quiet_NaN()
    };

    double five_minute_log_return{
        std::numeric_limits<double>::quiet_NaN()
    };

    double fifteen_minute_log_return{
        std::numeric_limits<double>::quiet_NaN()
    };


    // ------------------------------------------------------
    // Multi-bar returns based on 1-minute closes
    // ------------------------------------------------------

    double return_5_bars{
        std::numeric_limits<double>::quiet_NaN()
    };

    double return_15_bars{
        std::numeric_limits<double>::quiet_NaN()
    };

    double return_30_bars{
        std::numeric_limits<double>::quiet_NaN()
    };


    // ------------------------------------------------------
    // Candle geometry
    //
    // Calculated from latest completed 1-minute candle.
    // ------------------------------------------------------

    double candle_body_percent{
        std::numeric_limits<double>::quiet_NaN()
    };

    double candle_range_percent{
        std::numeric_limits<double>::quiet_NaN()
    };

    // 0.0 = close at low
    // 1.0 = close at high

    double close_location{
        std::numeric_limits<double>::quiet_NaN()
    };


    // ------------------------------------------------------
    // Availability
    // ------------------------------------------------------

    bool has_one_minute{false};
    bool has_five_minute{false};
    bool has_fifteen_minute{false};

    bool has_5_bar_history{false};
    bool has_15_bar_history{false};
    bool has_30_bar_history{false};
};

}