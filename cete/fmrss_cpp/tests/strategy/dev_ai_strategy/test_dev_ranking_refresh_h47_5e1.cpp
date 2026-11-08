#include "devai/strategy/dev_ai_strategy/DevRankingRefreshController_H47_5E1.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace devai::strategy;

static DevTopCandidateSelection makeSelection(std::int64_t t) {
    DevTopCandidateSelection s{};
    s.decision_time = t;
    s.benchmark_symbol = "NIFTY%2050";
    s.input_candidate_count = 10;
    s.eligible_candidate_count = 10;
    s.selected_candidate_count = DEV_AGENT_SLOT_COUNT;
    s.numerical_integrity = true;
    for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i) {
        auto& a = s.assignments[i];
        a.slot_index = i;
        a.agent_name = DevTopCandidateSelector::agentNames()[i];
        a.symbol = "STOCK" + std::to_string(i + 1);
        a.decision_time = t;
        a.candidate_score = 5.0 - static_cast<double>(i);
        a.candidate_rank = i + 1;
        a.assigned = true;
    }
    return s;
}

template<class F> static void mustThrow(F&& f) {
    bool threw = false;
    try { f(); } catch (const std::exception&) { threw = true; }
    assert(threw);
}

int main() {
    using Controller = DevRankingRefreshController_H47_5E1;
    // 2026-10-16 09:15, 10:15 and 10:45 IST.
    constexpr std::int64_t session_open = 1792122300LL;
    constexpr std::int64_t t = 1792125900LL;
    constexpr std::int64_t next = t + 1800;
    constexpr std::int64_t session_close = 1792143900LL; // 15:15 IST
    static_assert(Controller::refresh_seconds == 1800);
    static_assert(Controller::decision_seconds == 60);
    assert(Controller::isRefreshTime(session_open));
    assert(Controller::isRefreshTime(t));
    assert(Controller::isRefreshTime(next));
    assert(!Controller::isRefreshTime(t + 900));
    assert(!Controller::isRefreshTime(t + 60));
    assert(!Controller::isRefreshTime(session_close));
    assert(Controller::rankingEpoch(t + 1740) == t);
    assert(Controller::rankingEpoch(next) == next);
    assert(Controller::rankingEpoch(session_close) == session_close - 1800);

    Controller c;
    assert(!c.activeAt(t));
    assert(c.needsRefresh(t));
    assert(!c.needsRefresh(t + 60));
    auto s = makeSelection(t);
    c.publish(s);
    assert(c.activeAt(t) && c.activeAt(t + 1740));
    assert(!c.activeAt(next));
    assert(!c.activeAt(t - 60));
    assert(!c.activeAt(t + 30));
    assert(!c.needsRefresh(t));
    assert(c.needsRefresh(next));
    mustThrow([&] { c.publish(s); });

    auto invalid = makeSelection(next);
    invalid.assignments[1].symbol = invalid.assignments[0].symbol;
    mustThrow([&] { c.publish(invalid); });
    assert(c.activeAt(t + 1740)); // rejected publication does not corrupt active ranking
    invalid = makeSelection(next);
    invalid.assignments[0].candidate_score = std::numeric_limits<double>::quiet_NaN();
    mustThrow([&] { c.publish(invalid); });
    mustThrow([&] { c.publish(makeSelection(next + 60)); });
    mustThrow([&] { c.publish(makeSelection(t + 900)); });

    c.publish(makeSelection(next));
    assert(c.activeAt(next));
    assert(c.activeAt(next + 1740));
    assert(!c.activeAt(next + 1800));

    // No ranking from the previous trading day may be reused the next day.
    constexpr std::int64_t next_day_open = session_open + 86400;
    assert(Controller::isRefreshTime(next_day_open));
    assert(!c.activeAt(next_day_open));
    assert(c.needsRefresh(next_day_open));
    c.publish(makeSelection(next_day_open));
    assert(c.activeAt(next_day_open));

    std::cout << "H4.7 Stage 5E.2I 30-minute ranking refresh tests PASS\n";
}
