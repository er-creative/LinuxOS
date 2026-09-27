#include "devai/market/StockBenchmarkSynchronizer.hpp"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace devai::market;


namespace
{

std::int64_t makeTimestamp(
    int year,
    int month,
    int day,
    int hour,
    int minute)
{
    std::tm value{};

    value.tm_year =
        year - 1900;

    value.tm_mon =
        month - 1;

    value.tm_mday =
        day;

    value.tm_hour =
        hour;

    value.tm_min =
        minute;

    value.tm_sec =
        0;

    value.tm_isdst =
        -1;

    return static_cast<std::int64_t>(
        std::mktime(&value)
    );
}


Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double price)
{
    Candle candle;

    candle.symbol =
        symbol;

    candle.timestamp =
        timestamp;

    candle.open =
        price;

    candle.high =
        price + 1.0;

    candle.low =
        price - 1.0;

    candle.close =
        price + 0.5;

    candle.volume =
        1000;

    return candle;
}


MarketSnapshot makeSnapshot(
    const std::string& symbol,
    std::int64_t decision_time,
    const TradingDate& trading_date,
    std::int64_t one_timestamp,
    std::int64_t five_timestamp,
    std::int64_t fifteen_timestamp,
    double base_price)
{
    MarketSnapshot snapshot;

    snapshot.symbol =
        symbol;

    snapshot.decision_time =
        decision_time;

    snapshot.trading_date =
        trading_date;

    snapshot.session_phase =
        SessionPhase::ACTIVE;

    snapshot.new_session =
        false;


    snapshot.one_minute =
        makeCandle(
            symbol,
            one_timestamp,
            base_price
        );


    snapshot.five_minute =
        makeCandle(
            symbol,
            five_timestamp,
            base_price
        );


    snapshot.fifteen_minute =
        makeCandle(
            symbol,
            fifteen_timestamp,
            base_price
        );


    return snapshot;
}

}


// ============================================================================
// Main
// ============================================================================

int main()
{
    StockBenchmarkSynchronizer synchronizer;


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t decision_time =
        makeTimestamp(
            2026, 9, 25,
            9, 30
        );


    const std::int64_t one_timestamp =
        makeTimestamp(
            2026, 9, 25,
            9, 29
        );


    const std::int64_t five_timestamp =
        makeTimestamp(
            2026, 9, 25,
            9, 25
        );


    const std::int64_t fifteen_timestamp =
        makeTimestamp(
            2026, 9, 25,
            9, 15
        );


    // ========================================================================
    // Correctly aligned stock
    // ========================================================================

    MarketSnapshot stock =
        makeSnapshot(
            "HDFCBANK",
            decision_time,
            date,
            one_timestamp,
            five_timestamp,
            fifteen_timestamp,
            3000.0
        );


    // ========================================================================
    // Correctly aligned benchmark
    // ========================================================================

    MarketSnapshot nifty =
        makeSnapshot(
            "NIFTY%2050",
            decision_time,
            date,
            one_timestamp,
            five_timestamp,
            fifteen_timestamp,
            25000.0
        );


    // ========================================================================
    // TEST 1
    //
    // Correct synchronization
    // ========================================================================

    const StockBenchmarkSnapshot pair =
        synchronizer.synchronize(
            stock,
            nifty
        );


    assert(
        pair.stock.symbol ==
        "HDFCBANK"
    );


    assert(
        pair.benchmark.symbol ==
        "NIFTY%2050"
    );


    assert(
        pair.decision_time ==
        decision_time
    );


    assert(
        pair.sameDecisionTime()
    );


    assert(
        pair.sameTradingDate()
    );


    assert(
        pair.sessionsActive()
    );


    assert(
        pair.stockFullySynchronized()
    );


    assert(
        pair.benchmarkFullySynchronized()
    );


    assert(
        pair.fullySynchronized()
    );


    // ========================================================================
    // TEST 2
    //
    // Different decision times must be rejected.
    // ========================================================================

    bool decision_mismatch_rejected =
        false;


    MarketSnapshot wrong_decision =
        nifty;


    wrong_decision.decision_time =
        decision_time + 60;


    try
    {
        (void) synchronizer.synchronize(
            stock,
            wrong_decision
        );
    }
    catch (const std::invalid_argument&)
    {
        decision_mismatch_rejected =
            true;
    }


    assert(
        decision_mismatch_rejected
    );


    // ========================================================================
    // TEST 3
    //
    // Different trading dates must be rejected.
    // ========================================================================

    bool date_mismatch_rejected =
        false;


    MarketSnapshot wrong_date =
        nifty;


    wrong_date.trading_date =
        TradingDate{
            2026,
            9,
            24
        };


    try
    {
        (void) synchronizer.synchronize(
            stock,
            wrong_date
        );
    }
    catch (const std::invalid_argument&)
    {
        date_mismatch_rejected =
            true;
    }


    assert(
        date_mismatch_rejected
    );


    // ========================================================================
    // TEST 4
    //
    // Mismatched 5-minute market timestamps must be rejected.
    // ========================================================================

    bool timeframe_mismatch_rejected =
        false;


    MarketSnapshot wrong_five =
        nifty;


    wrong_five.five_minute =
        makeCandle(
            "NIFTY%2050",
            makeTimestamp(
                2026, 9, 25,
                9, 20
            ),
            25000.0
        );


    try
    {
        (void) synchronizer.synchronize(
            stock,
            wrong_five
        );
    }
    catch (const std::runtime_error&)
    {
        timeframe_mismatch_rejected =
            true;
    }


    assert(
        timeframe_mismatch_rejected
    );


    // ========================================================================
    // TEST 5
    //
    // Missing stock 15m candle:
    //
    // Pair creation is allowed because the data is honestly unavailable,
    // but the pair must NOT report fully synchronized.
    // ========================================================================

    MarketSnapshot missing_stock_fifteen =
        stock;


    missing_stock_fifteen.fifteen_minute.reset();


    const StockBenchmarkSnapshot incomplete_pair =
        synchronizer.synchronize(
            missing_stock_fifteen,
            nifty
        );


    assert(
        !incomplete_pair.stockFullySynchronized()
    );


    assert(
        incomplete_pair.benchmarkFullySynchronized()
    );


    assert(
        !incomplete_pair.fullySynchronized()
    );


    // ========================================================================
    // TEST 6
    //
    // Same symbol cannot be both stock and benchmark.
    // ========================================================================

    bool same_symbol_rejected =
        false;


    MarketSnapshot same_symbol =
        nifty;


    same_symbol.symbol =
        "HDFCBANK";


    if (same_symbol.one_minute)
    {
        same_symbol.one_minute->symbol =
            "HDFCBANK";
    }

    if (same_symbol.five_minute)
    {
        same_symbol.five_minute->symbol =
            "HDFCBANK";
    }

    if (same_symbol.fifteen_minute)
    {
        same_symbol.fifteen_minute->symbol =
            "HDFCBANK";
    }


    try
    {
        (void) synchronizer.synchronize(
            stock,
            same_symbol
        );
    }
    catch (const std::invalid_argument&)
    {
        same_symbol_rejected =
            true;
    }


    assert(
        same_symbol_rejected
    );


    // ========================================================================
    // TEST 7
    //
    // Session phase mismatch must be rejected.
    // ========================================================================

    bool session_mismatch_rejected =
        false;


    MarketSnapshot wrong_session =
        nifty;


    wrong_session.session_phase =
        SessionPhase::AFTER_CLOSE;


    try
    {
        (void) synchronizer.synchronize(
            stock,
            wrong_session
        );
    }
    catch (const std::runtime_error&)
    {
        session_mismatch_rejected =
            true;
    }


    assert(
        session_mismatch_rejected
    );


    // ========================================================================
    // RESULT
    // ========================================================================

    std::cout
        << "\n"
        << "================================================\n"
        << "StockBenchmarkSynchronizer Tests PASSED\n"
        << "================================================\n"
        << "Stock + NIFTY pairing          : PASSED\n"
        << "Decision-time alignment        : PASSED\n"
        << "Trading-date alignment         : PASSED\n"
        << "1m / 5m / 15m alignment       : PASSED\n"
        << "Missing-timeframe protection   : PASSED\n"
        << "Same-symbol protection         : PASSED\n"
        << "Session-phase protection       : PASSED\n"
        << "Full synchronization status    : PASSED\n"
        << "================================================\n";


    return 0;
}