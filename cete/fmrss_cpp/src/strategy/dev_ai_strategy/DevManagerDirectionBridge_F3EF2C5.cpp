#include "devai/strategy/dev_ai_strategy/DevManagerDirectionBridge_F3EF2C5.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace devai::strategy::f3ef2c5 {
namespace {
bool finite(double x) { return std::isfinite(x); }
}
Plan decide(const Context& c, const std::vector<AgentAssessment>& top5) {
    const auto& manager_assignment = h46::assignment(c.manager_id);
    Plan wait{PlanKind::WAIT, {}, 0, 0, "WAIT: no eligible positive-advantage action"};
    if (!finite(c.min_q_advantage) || c.min_q_advantage < 0)
        throw std::invalid_argument("min_q_advantage must be finite and nonnegative");
    if (top5.size() > 5) throw std::invalid_argument("Top-5 contains more than five agents");
    if (c.decision_key <= 0 || !c.risk_approved || !c.model_approved) return wait;
    std::set<std::string> symbols;
    std::set<std::size_t> ranks;
    for (const auto& a : top5) {
        if (!a.assigned || a.rank < 1 || a.rank > 5 || a.symbol.empty() ||
            !symbols.insert(a.symbol).second || !ranks.insert(a.rank).second)
            throw std::invalid_argument("invalid or duplicate Top-5 assignment");
    }
    // This bridge intentionally does not implement HOLD/EXIT RL decisions.
    // Existing manager remains authoritative for those actions.
    if (c.position.open) return {PlanKind::WAIT, {}, 0, 0,
        "Position open: defer HOLD/EXIT/SWITCH to existing DevManager; never auto-switch"};
    double best_advantage = c.min_q_advantage;
    Plan best = wait;
    for (const auto& a : top5) {
        if (!a.numerical_integrity || a.decision_key != c.decision_key ||
            !finite(a.q_open_long) || !finite(a.q_open_short) || !finite(a.q_wait) ||
            !finite(a.fill_price) || a.fill_price <= 0 || a.quantity == 0) continue;
        for (bool is_short : {manager_assignment.side == h46::Side::SHORT}) {
            if (is_short && (!c.trained_shared_model_supports_short ||
                             !c.gateway_short_enabled ||
                             !a.shared_model_supports_short || !a.shortable)) continue;
            const double advantage = (is_short ? a.q_open_short : a.q_open_long) - a.q_wait;
            if (advantage <= best_advantage) continue; // ties => deterministic first rank
            f3ef2c4b::Request req{};
            req.decision = is_short ? f3ef2c4b::Decision::OPEN_SHORT
                                    : f3ef2c4b::Decision::OPEN_LONG;
            req.minute_of_day_ist = c.minute_of_day_ist;
            req.price = a.fill_price;
            req.quantity = a.quantity;
            req.model_approved = c.model_approved;
            req.risk_approved = c.risk_approved;
            req.shortable = a.shortable;
            if (f3ef2c4b::check(c.policy, c.position, req) != f3ef2c4b::Verdict::ALLOW)
                continue;
            best_advantage = advantage;
            best = {is_short ? PlanKind::OPEN_SHORT : PlanKind::OPEN_LONG,
                    a.symbol, a.quantity, a.fill_price,
                    is_short ? "Shared-model SHORT approved" : "Shared-model LONG approved"};
        }
    }
    return best;
}
DevTradeSuggestion toGatewaySuggestion(const Plan& p, const Context& c,
                                      const std::string& decision_id) {
    const auto& manager_assignment = h46::assignment(c.manager_id);
    if ((p.kind == PlanKind::OPEN_LONG && manager_assignment.side != h46::Side::LONG) ||
        (p.kind == PlanKind::OPEN_SHORT && manager_assignment.side != h46::Side::SHORT))
        throw std::invalid_argument("H4.6: manager direction mismatch");
    if (p.kind != PlanKind::OPEN_LONG && p.kind != PlanKind::OPEN_SHORT)
        throw std::invalid_argument("Only approved OPEN plans can be submitted");
    if (decision_id.empty() || p.symbol.empty() || p.quantity == 0)
        throw std::invalid_argument("invalid gateway order identity");
    DevTradeSuggestion s{};
    s.symbol = p.symbol;
    s.decision_key = c.decision_key;
    s.intent = p.kind == PlanKind::OPEN_LONG ? DevIntent::OPEN_LONG : DevIntent::OPEN_SHORT;
    s.quantity = p.quantity;
    s.shortable = p.kind == PlanKind::OPEN_SHORT && c.trained_shared_model_supports_short && c.gateway_short_enabled;
    s.position_exists = c.position.open;
    s.model_approved = c.model_approved;
    s.risk_approved = c.risk_approved;
    s.decision_id = decision_id;
    s.execution_price = p.price;
    return s;
}
}
