#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace devai::strategy::f3ef2c4b {
enum class Direction { LONG, SHORT };
enum class Decision { WAIT, OPEN_LONG, OPEN_SHORT, HOLD, EXIT, SWITCH_LONG, SWITCH_SHORT };
struct Config {
    double capital_inr = 7000.0;
    double allocation_fraction = 0.75;
    int entry_cutoff_minute_ist = 14 * 60 + 45;
    int force_flat_minute_ist = 15 * 60 + 15;
    bool enable_long = true;
    bool enable_short = false;
};
struct Position { bool open=false; Direction direction=Direction::LONG; std::uint64_t quantity=0; double entry_price=0; };
struct Request {
    Decision decision=Decision::WAIT;
    int minute_of_day_ist=0;
    double price=0;
    std::uint64_t quantity=0;
    bool model_approved=false;
    bool risk_approved=false;
    bool shortable=false;
};
enum class Verdict { ALLOW, BLOCK };
inline Verdict check(const Config& cfg, const Position& position, const Request& req) {
    if (!std::isfinite(cfg.capital_inr) || cfg.capital_inr <= 0 ||
        !std::isfinite(cfg.allocation_fraction) || cfg.allocation_fraction <= 0 || cfg.allocation_fraction > 1 ||
        cfg.entry_cutoff_minute_ist > cfg.force_flat_minute_ist ||
        cfg.force_flat_minute_ist != 15*60+15) throw std::invalid_argument("invalid intraday config");
    if (req.minute_of_day_ist < 9*60+15 || req.minute_of_day_ist > cfg.force_flat_minute_ist)
        return Verdict::BLOCK;
    if (req.decision == Decision::WAIT) return Verdict::ALLOW;
    if (req.decision == Decision::HOLD) return position.open && req.minute_of_day_ist < cfg.force_flat_minute_ist ? Verdict::ALLOW : Verdict::BLOCK;
    if (req.decision == Decision::EXIT) return position.open ? Verdict::ALLOW : Verdict::BLOCK;
    if (!req.model_approved || !req.risk_approved || !std::isfinite(req.price) || req.price <= 0 || req.quantity == 0)
        return Verdict::BLOCK;
    if (req.minute_of_day_ist >= cfg.entry_cutoff_minute_ist) return Verdict::BLOCK;
    const bool is_short = req.decision == Decision::OPEN_SHORT || req.decision == Decision::SWITCH_SHORT;
    if (is_short ? (!cfg.enable_short || !req.shortable) : !cfg.enable_long) return Verdict::BLOCK;
    // No leverage; a short position is also capped by allocated notional.
    if (static_cast<double>(req.quantity) * req.price > cfg.capital_inr * cfg.allocation_fraction + 1e-8)
        return Verdict::BLOCK;
    if (req.decision == Decision::OPEN_LONG || req.decision == Decision::OPEN_SHORT)
        return position.open ? Verdict::BLOCK : Verdict::ALLOW;
    if (req.decision == Decision::SWITCH_LONG || req.decision == Decision::SWITCH_SHORT)
        return Verdict::BLOCK; // switch is two orders: confirmed EXIT, then new OPEN
    return Verdict::BLOCK;
}
inline double gross_pnl(Direction direction, double entry, double exit, std::uint64_t qty) {
    if (!std::isfinite(entry) || !std::isfinite(exit) || entry <= 0 || exit <= 0 || qty == 0)
        throw std::invalid_argument("invalid trade");
    return (direction == Direction::LONG ? exit-entry : entry-exit) * static_cast<double>(qty);
}
}
