#include <iostream>

#include "devai/market/Candle.hpp"
#include "devai/market/Timeframe.hpp"

int main()
{
    using namespace devai::market;

    Candle candle;

    candle.symbol = "RELIANCE";
    candle.timestamp = 1789953300;

    candle.open   = 3000.00;
    candle.high   = 3010.00;
    candle.low    = 2995.00;
    candle.close  = 3005.00;

    candle.volume = 150000;

    if (!candle.valid())
    {
        std::cerr << "FAILED: Valid candle rejected.\n";
        return 1;
    }

    Candle bad_candle = candle;

    bad_candle.high = 2900.00;

    if (bad_candle.valid())
    {
        std::cerr << "FAILED: Invalid candle accepted.\n";
        return 1;
    }

    if (minutes(Timeframe::ONE_MINUTE) != 1)
    {
        std::cerr << "FAILED: ONE_MINUTE.\n";
        return 1;
    }

    if (minutes(Timeframe::FIVE_MINUTES) != 5)
    {
        std::cerr << "FAILED: FIVE_MINUTES.\n";
        return 1;
    }

    if (minutes(Timeframe::FIFTEEN_MINUTES) != 15)
    {
        std::cerr << "FAILED: FIFTEEN_MINUTES.\n";
        return 1;
    }

    std::cout
        << "========================================\n"
        << "DevAI Market Foundation Test\n"
        << "========================================\n"
        << "Symbol       : " << candle.symbol << '\n'
        << "Open         : " << candle.open << '\n'
        << "High         : " << candle.high << '\n'
        << "Low          : " << candle.low << '\n'
        << "Close        : " << candle.close << '\n'
        << "Volume       : " << candle.volume << '\n'
        << "1 minute     : " << seconds(Timeframe::ONE_MINUTE) << '\n'
        << "5 minutes    : " << seconds(Timeframe::FIVE_MINUTES) << '\n'
        << "15 minutes   : " << seconds(Timeframe::FIFTEEN_MINUTES) << '\n'
        << "========================================\n"
        << "PASS\n";

    return 0;
}