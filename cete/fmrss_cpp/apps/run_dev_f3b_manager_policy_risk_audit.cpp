// D9F.4C-F3B — deterministic decision-only policy / session risk audit.
// Tests the same 7-action slot mapping used by DevCandleManagerReplay_D9F4C.
// Does NOT modify the production replay, execute orders, or validate fills.
#include "devai/strategy/dev_ai_strategy/risk/DevSessionSupervisor.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace session = devai::strategy::session;
namespace {
constexpr std::size_t N = 7;
using Mask = std::array<bool,N>;
using Scores = std::array<double,N>;
struct Portfolio {
    std::string held;
    double cash{7000.0};
    double equity{7000.0};
    double capital{7000.0};
    double allocation{0.75};
};
struct Input {
    std::int64_t time{};
    Portfolio portfolio{};
    std::array<std::string,5> symbols{};
    Scores q{};
    session::PositionSnapshot position{};
};
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
// Action 0 = EXIT if invested / WAIT if flat; 1 = KEEP; 2..6 = select slots 0..4.
// Matches the seven-action mapping in DevCandleManagerReplay_D9F4C::encode().
Mask legal(const Input& in) {
    require(in.portfolio.capital > 0 && std::isfinite(in.portfolio.capital), "invalid capital");
    require(in.portfolio.equity > 0 && std::isfinite(in.portfolio.equity), "invalid equity");
    require(in.portfolio.cash >= 0 && std::isfinite(in.portfolio.cash), "invalid cash");
    require(in.portfolio.allocation > 0 && in.portfolio.allocation <= 0.75 &&
            std::isfinite(in.portfolio.allocation), "allocation exceeds 75% cap");
    Mask mask{};
    mask[0] = true;
    mask[1] = !in.portfolio.held.empty();
    for (std::size_t i=0;i<5;++i) {
        if (in.symbols[i].empty()) continue;
        const bool is_keep = in.symbols[i] == in.portfolio.held;
        // This is a conservative audit-only cash gate for flat portfolios.
        // Production ledger remains responsible for actual sizing and costs.
        const bool enough_cash = !in.portfolio.held.empty() ||
            in.portfolio.cash > 0.0;
        mask[2+i] = (is_keep ||
            (enough_cash && session::entryAllowed(in.time))) &&
            session::targetAllowed(in.portfolio.held,in.symbols[i],in.time);
    }
    if (session::minuteIST(in.time) >= 15*60+15) {
        mask.fill(false);
        mask[0]=true;
    }
    return mask;
}
std::string target(const Input& in, std::size_t action) {
    require(action < N,"invalid action index");
    if (action == 0) return {};
    if (action == 1) return in.portfolio.held;
    return in.symbols[action-2];
}
std::size_t choose(const Input& in, const Mask& mask) {
    require(mask[0],"WAIT/EXIT fallback unavailable");
    for (double q:in.q) require(std::isfinite(q),"nonfinite Q-value");
    const auto assessment = session::assess(in.position);
    if (session::minuteIST(in.time) >= 15*60+15 ||
        assessment.recommendation == session::Recommendation::EXIT_REQUIRED)
        return 0;
    std::size_t best=0;
    for (std::size_t a=1;a<N;++a)
        if (mask[a] && in.q[a] > in.q[best]) best=a;
    return best;
}
void run(const char* label, const Input& in, std::size_t expected,
         bool must_block_action=false, std::size_t blocked=0) {
    require(in.position.decision_time_utc == in.time,"position timestamp mismatch");
    require(in.position.invested == !in.portfolio.held.empty(),"position mismatch");
    const auto mask=legal(in);
    if (must_block_action) require(blocked<N && !mask[blocked],"expected illegal action was allowed");
    const auto action=choose(in,mask);
    require(mask[action],"selected illegal action");
    require(action == expected,std::string(label)+": unexpected action");
    const auto desired=target(in,action);
    require(session::targetAllowed(in.portfolio.held,desired,in.time),"invalid target");
    std::cout << "PASS " << label << " action=" << action
              << " target=" << (desired.empty()?"FLAT":desired) << '\n';
}
// UTC epoch for 2025-01-15 at given IST hour/minute.
constexpr std::int64_t at(int hour,int minute) {
    return 1736879400LL + (hour*60+minute)*60LL;
}
Input flat(int hour,int minute) {
    Input in{};
    in.time=at(hour,minute);
    in.position.decision_time_utc=in.time;
    in.symbols={"AXISBANK","ICICIBANK","HDFCBANK","RELIANCE","SBIN"};
    return in;
}
Input invested(int hour,int minute) {
    Input in=flat(hour,minute);
    in.portfolio.held="AXISBANK";
    in.portfolio.cash=1750;
    in.position.invested=true;
    in.position.symbol="AXISBANK";
    in.position.entry_time_utc=at(10,0);
    in.position.entry_price=100;
    in.position.last_price=101;
    in.position.highest_price_since_entry=102;
    in.position.lowest_price_since_entry=99;
    return in;
}
}
int main() {
    try {
        require(session::minuteIST(at(11,0)) == 11*60,"IST timestamp fixture incorrect");
        auto x=flat(11,0);
        x.q[0]=5; x.q[2]=4;
        run("flat_wait",x,0);
        x.q[2]=10;
        run("flat_select_slot0",x,2);
        x=invested(11,0); x.q[1]=10;
        run("invested_keep",x,1);
        x.q[3]=20;
        run("invested_switch_slot1",x,3);
        x.q[0]=30;
        run("invested_exit",x,0);
        x=flat(11,0); x.portfolio.cash=0; x.q[2]=100;
        run("no_cash_blocks_new_entry",x,0,true,2);
        x=flat(11,0); x.portfolio.allocation=0.76;
        bool rejected=false;
        try { (void)legal(x); } catch(const std::runtime_error&) { rejected=true; }
        require(rejected,"allocation over 75% was accepted");
        std::cout << "PASS allocation_over_cap_rejected\n";
        x=flat(14,45); x.q[2]=100;
        run("cutoff_blocks_new_entry",x,0,true,2);
        x=invested(14,45); x.q[3]=100; x.q[1]=2;
        run("cutoff_blocks_switch",x,1,true,3);
        x=invested(15,10); x.q[1]=100;
        run("urgent_buffer_is_advisory",x,1);
        x=invested(15,15); x.q[1]=100;
        run("deadline_forces_exit",x,0,true,1);
        x=invested(11,0); x.position.stop_loss_price=102; x.q[1]=100;
        run("stop_loss_forces_exit",x,0);
        x=flat(11,0); x.q[2]=100; x.q[2]=std::nan("");
        rejected=false;
        try { (void)choose(x,legal(x)); } catch(const std::runtime_error&) { rejected=true; }
        require(rejected,"nonfinite model output accepted");
        std::cout << "PASS nonfinite_q_rejected\n";
        std::cout << "F3B MANAGER POLICY AND SESSION RISK AUDIT: PASSED\n";
        std::cout << "Decision-only; no checkpoint, training, ledger, orders, or fills.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "F3B FAILED: " << e.what() << '\n';
        return 1;
    }
}
