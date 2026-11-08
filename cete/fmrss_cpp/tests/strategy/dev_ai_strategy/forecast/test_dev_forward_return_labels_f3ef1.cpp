#include "devai/strategy/dev_ai_strategy/forecast/DevForwardReturnLabels_F3EF1.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace devai::strategy::f3ef1;
int main() {
    std::vector<MinuteClose> bars;
    for (int i=0; i<20; ++i) bars.push_back({1'700'000'000LL+60LL*i,100.0+i,20250115});
    auto rows=make_forward_return_labels(bars);
    assert(rows.size()==20);
    assert(rows[0].horizons[0].valid);
    assert(std::abs(rows[0].horizons[0].simple_return-0.01)<1e-12);
    assert(rows[0].horizons[4].valid);
    assert(std::abs(rows[0].horizons[4].simple_return-0.15)<1e-12);
    assert(!rows[19].horizons[0].valid);
    bars[8].epoch_seconds+=30; // a missing/irregular minute invalidates crossing horizons
    rows=make_forward_return_labels(bars);
    assert(!rows[0].horizons[3].valid);
    assert(rows[0].horizons[1].valid);
    bars[8].epoch_seconds-=30;
    bars[12].session_id=20250116; // forbid crossing session boundary
    rows=make_forward_return_labels(bars);
    assert(!rows[0].horizons[4].valid);
    assert(rows[0].horizons[3].valid);
    bool threw=false;
    try { bars[0].close=0; (void)make_forward_return_labels(bars); }
    catch (const std::invalid_argument&) { threw=true; }
    assert(threw);
    std::cout << "F3E-F1 FORWARD RETURN LABELS: PASS (horizons, gaps, session, invalid price)\n";
}
