#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace devai::strategy::f3ef2c5e {
inline constexpr std::size_t universe_size = 10;
inline constexpr std::size_t long_agents = 5;
inline constexpr std::size_t agent_count = 6;
inline constexpr std::size_t manager_action_count = 8;
inline constexpr std::size_t manager_input_size = agent_count * 10 + 6 + agent_count + 1; // 73
inline constexpr std::size_t short_slot = 5;

enum class Direction { LONG, SHORT };
enum class ManagerAction : std::size_t {
    WAIT = 0, KEEP_CURRENT = 1, AGENT_001 = 2, AGENT_002 = 3,
    AGENT_003 = 4, AGENT_004 = 5, AGENT_005 = 6, AGENT_006 = 7
};
struct RankedStock {
    std::string symbol;
    double statistical_score{};
    bool shortable{false};
};
struct Assignment {
    std::size_t agent_slot{};   // zero based
    std::size_t statistical_rank{}; // 1..10
    Direction direction{Direction::LONG};
    RankedStock stock;
};
struct Selection {
    std::array<Assignment, agent_count> agents;
    std::array<RankedStock, universe_size> ranked;
};
inline Selection select(const std::vector<RankedStock>& universe) {
    if (universe.size() != universe_size)
        throw std::invalid_argument("F3E-F2C.5E: require exactly 10 stocks");
    std::unordered_set<std::string> seen;
    Selection result{};
    for (std::size_t i=0;i<universe_size;++i) {
        const auto& s=universe[i];
        if (s.symbol.empty() || !std::isfinite(s.statistical_score) || !seen.insert(s.symbol).second)
            throw std::invalid_argument("F3E-F2C.5E: duplicate/invalid stock");
        result.ranked[i]=s;
    }
    std::sort(result.ranked.begin(),result.ranked.end(),[](const auto& a,const auto& b){
        if(a.statistical_score != b.statistical_score) return a.statistical_score > b.statistical_score;
        return a.symbol < b.symbol; // deterministic tie-break
    });
    for (std::size_t i=0;i<long_agents;++i)
        result.agents[i]={i,i+1,Direction::LONG,result.ranked[i]};
    result.agents[short_slot]={short_slot,10,Direction::SHORT,result.ranked[9]};
    return result;
}
struct AgentAssessment {
    Assignment assignment;
    double q_direction{}; // learned action value; NOT probability or currency
    double q_wait{};
    double estimated_net_profit{}; // calibrated separately; currency
    double probability{}; // calibrated estimate in [0,1]
    bool valid{false};
};
struct ExistingPosition {
    bool open{false};
    std::string symbol;
    Direction direction{Direction::LONG};
    double estimated_hold_net_profit{}; // same horizon as candidate estimates
};
struct Policy {
    double minimum_probability{0.55};
    double minimum_incremental_net_profit{0.0};
    double minimum_q_advantage{0.0};
    bool allow_short{true};
    bool entry_window_open{true};
    bool risk_approved{true};
};
enum class DecisionKind { WAIT, KEEP, OPEN_LONG, OPEN_SHORT, EXIT_THEN_OPEN_LONG, EXIT_THEN_OPEN_SHORT };
struct Decision {
    DecisionKind kind{DecisionKind::WAIT};
    std::string symbol;
    std::size_t agent_slot{agent_count};
    double estimated_incremental_net_profit{};
};
inline Decision decide(const std::array<AgentAssessment,agent_count>& reports,
                       const ExistingPosition& position, const Policy& policy) {
    if (!std::isfinite(policy.minimum_probability) || policy.minimum_probability<0 || policy.minimum_probability>1 ||
        !std::isfinite(policy.minimum_incremental_net_profit) || !std::isfinite(policy.minimum_q_advantage) ||
        (position.open && (!std::isfinite(position.estimated_hold_net_profit) || position.symbol.empty())))
        throw std::invalid_argument("F3E-F2C.5E: invalid policy/position");
    Decision best{};
    if(position.open) best.kind=DecisionKind::KEEP;
    if(!policy.entry_window_open || !policy.risk_approved) return best;
    double best_gain=policy.minimum_incremental_net_profit;
    for(std::size_t i=0;i<agent_count;++i) {
        const auto& a=reports[i];
        if(!a.valid || a.assignment.agent_slot!=i || a.assignment.statistical_rank!=(i==short_slot?10:i+1) ||
           a.assignment.direction!=(i==short_slot?Direction::SHORT:Direction::LONG)) continue;
        if(i==short_slot && (!policy.allow_short || !a.assignment.stock.shortable)) continue;
        if(!std::isfinite(a.q_direction) || !std::isfinite(a.q_wait) ||
           !std::isfinite(a.estimated_net_profit) || !std::isfinite(a.probability) ||
           a.probability<policy.minimum_probability || a.probability>1 ||
           a.q_direction-a.q_wait<=policy.minimum_q_advantage) continue;
        // Estimated net profit must already include ENTRY, EXIT, taxes, slippage and switching costs.
        const double gain=a.estimated_net_profit-(position.open?position.estimated_hold_net_profit:0.0);
        if(gain<=best_gain) continue;
        // Switching to exactly the same position is not an actionable opportunity.
        if(position.open && position.symbol==a.assignment.stock.symbol && position.direction==a.assignment.direction) continue;
        best_gain=gain;
        best.kind=position.open
            ? (i==short_slot?DecisionKind::EXIT_THEN_OPEN_SHORT:DecisionKind::EXIT_THEN_OPEN_LONG)
            : (i==short_slot?DecisionKind::OPEN_SHORT:DecisionKind::OPEN_LONG);
        best.symbol=a.assignment.stock.symbol;
        best.agent_slot=i;
        best.estimated_incremental_net_profit=gain;
    }
    return best;
}
} // namespace devai::strategy::f3ef2c5e
