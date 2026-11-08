#include "devai/execution/DevHoldDecision.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace cete::supervision;
    const auto h = devai::execution::makeHoldDecision("hold-001", "DevManager_001", 1000);
    assert(h.status == DevOrderStatus::HELD);
    assert(h.action == DevSupervisedAction::HOLD);
    assert(h.order_id == "hold-001");
    assert(h.manager_id == "DevManager_001");
    assert(h.timestamp == 1000);
    assert(h.symbol.empty() && h.agent_id.empty());
    assert(h.shares == 0 && h.fee == 0.0 && h.realized_pnl == 0.0);
    bool threw = false;
    try { (void)devai::execution::makeHoldDecision("", "DevManager_001", 1000); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    std::cout << "PASS 5C-3H-1 local HOLD decision contract\n";
}
