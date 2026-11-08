#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace devai::strategy {
enum class DevPortfolioAction { HOLD, LONG, SHORT, EXIT };
struct DevPortfolioConfig {
    double initial_cash{7000.0};
    double max_exposure_fraction{0.75};
    double transaction_cost_bps{5.0};
    double slippage_bps{2.0};
    bool allow_short{false}; // Simulation only; intraday cash short availability is broker-dependent.
    std::int64_t max_gap_seconds{60};
};
struct DevPortfolioQuote {
    std::string symbol;
    double open{0.0};
    double close{0.0};
};
struct DevPortfolioFrame {
    std::int64_t time{0}; // Opening time of 1-minute candle, UTC epoch
    std::vector<DevPortfolioQuote> quotes;
    bool session_end{false}; // Must be supplied by caller, never inferred from missing bars
};
struct DevPortfolioDecision {
    std::string agent_name;
    std::string symbol;
    DevPortfolioAction action{DevPortfolioAction::HOLD};
};
struct DevPortfolioPosition {
    std::string symbol;
    std::string agent_name;
    int side{0}; // +1 long, -1 short
    std::int64_t opened_at{0};
    std::int64_t shares{0};
    double entry_price{0.0};
};
struct DevPortfolioRuntimeState {
    std::int64_t time{0};
    double cash{7000.0};
    double equity{7000.0};
    double realized_pnl{0.0};
    double drawdown_fraction{0.0};
    std::optional<DevPortfolioPosition> position;
    bool terminal{false};
};
struct DevPortfolioStep {
    DevPortfolioRuntimeState before;
    DevPortfolioRuntimeState after;
    DevPortfolioDecision requested;
    bool executed{false};
    std::string reason;
    double reward{0.0}; // Equity delta / initial cash; sequential mark-to-market reward
};
class DevPortfolioEnvironment {
public:
    explicit DevPortfolioEnvironment(DevPortfolioConfig config = {});
    const DevPortfolioRuntimeState& reset();
    // Decision at candle open; fills at current open, marks equity at candle close.
    // A caller MUST NOT use current candle high/low/close to choose the action.
    DevPortfolioStep step(const DevPortfolioFrame& frame, const DevPortfolioDecision& decision);
    // Dedicated no-decision settlement at the replay cutoff.
    // frame.time is the END of the last completed candle; quotes contain its close.
    // Never enters a position or executes an action at a stale candle open.
    DevPortfolioStep settleSession(const DevPortfolioFrame& frame);
    const DevPortfolioRuntimeState& state() const noexcept { return state_; }
private:
    DevPortfolioConfig config_;
    DevPortfolioRuntimeState state_;
    double peak_equity_{7000.0};
    bool started_{false};
    double executablePrice(double raw, int direction) const;
    double fee(double quantity, double price) const;
    const DevPortfolioQuote& quoteFor(const DevPortfolioFrame&, const std::string&) const;
    void closePosition(const DevPortfolioFrame&, bool at_close);
    void markToMarket(const DevPortfolioFrame&);
};
}
