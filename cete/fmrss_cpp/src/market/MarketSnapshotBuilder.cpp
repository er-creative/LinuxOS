#include "devai/market/MarketSnapshotBuilder.hpp"

#include <stdexcept>
#include <utility>

namespace devai::market
{

// ============================================================================
// Constructor
// ============================================================================

MarketSnapshotBuilder::MarketSnapshotBuilder(
    std::string symbol
)
    : symbol_(
          std::move(symbol))
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "MarketSnapshotBuilder symbol cannot be empty."
        );
    }
}


// ============================================================================
// Validate Candle Symbol
//
// A snapshot for RELIANCE must never accidentally contain a candle
// belonging to another instrument.
// ============================================================================

void MarketSnapshotBuilder::validateCandleSymbol(
    const std::optional<TimedCandle>& candle,
    const std::string& expected_symbol,
    const char* timeframe_name)
{
    if (!candle.has_value())
    {
        return;
    }


    if (
        candle->candle.symbol !=
        expected_symbol
    )
    {
        throw std::runtime_error(
            std::string(
                "MarketSnapshot symbol mismatch in "
            ) +
            timeframe_name +
            " stream. Expected: " +
            expected_symbol +
            ", received: " +
            candle->candle.symbol
        );
    }
}


// ============================================================================
// Build Snapshot
// ============================================================================

MarketSnapshot MarketSnapshotBuilder::build(
    std::int64_t decision_time,
    MultiTimeframeCursor& cursor,
    SessionState& session) const
{
    // ========================================================================
    // 1. Advance market-data cursor
    // ========================================================================

    cursor.advanceTo(
        decision_time
    );


    // ========================================================================
    // 2. Update trading-session state
    // ========================================================================

    const bool new_session =
        session.update(
            decision_time
        );


    // ========================================================================
    // 3. Copy cursor state
    //
    // IMPORTANT:
    //
    // We copy the state before filtering it.
    //
    // SessionState::filter() must NOT modify the cursor's internal state.
    // Otherwise removing yesterday's candles from a snapshot could mutate
    // the underlying runtime cursor.
    // ========================================================================

    MultiTimeframeState filtered_state =
        cursor.state();


    // ========================================================================
    // 4. Remove stale / out-of-session candles
    // ========================================================================

    session.filter(
        filtered_state
    );


    // ========================================================================
    // 5. Validate instrument consistency
    // ========================================================================

    validateCandleSymbol(
        filtered_state.one_minute,
        symbol_,
        "1-minute"
    );


    validateCandleSymbol(
        filtered_state.five_minute,
        symbol_,
        "5-minute"
    );


    validateCandleSymbol(
        filtered_state.fifteen_minute,
        symbol_,
        "15-minute"
    );


    // ========================================================================
    // 6. Build immutable-at-this-decision-time snapshot
    // ========================================================================

    MarketSnapshot snapshot;

    snapshot.symbol =
        symbol_;

    snapshot.decision_time =
        decision_time;

    snapshot.trading_date =
        session.tradingDate();

    snapshot.session_phase =
        session.phase();

    snapshot.new_session =
        new_session;


    if (
        filtered_state.one_minute.has_value()
    )
    {
        snapshot.one_minute =
            filtered_state.one_minute
                ->candle;
    }


    if (
        filtered_state.five_minute.has_value()
    )
    {
        snapshot.five_minute =
            filtered_state.five_minute
                ->candle;
    }


    if (
        filtered_state.fifteen_minute.has_value()
    )
    {
        snapshot.fifteen_minute =
            filtered_state.fifteen_minute
                ->candle;
    }


    return snapshot;
}

}