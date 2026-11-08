#include "devai/strategy/dev_ai_strategy/DevFrozenOOSPolicy.hpp"
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <vector>

namespace devai::strategy {
namespace {
template<class P>
void save(const std::filesystem::path& path, const char* kind, const P& p) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("3.19D: cannot create checkpoint");
    out << "CETE_FROZEN_V1 " << kind << '\n' << std::setprecision(17);
    for (const auto* v : {&p.weights1,&p.biases1,&p.weights2,&p.biases2,
                          &p.weights3,&p.biases3}) {
        out << v->size() << '\n';
        for (double x : *v) {
            if (!std::isfinite(x)) throw std::runtime_error("3.19D: nonfinite weight");
            out << x << '\n';
        }
    }
    if (!out) throw std::runtime_error("3.19D: checkpoint write failed");
}
template<class P>
P load(const std::filesystem::path& path, const char* kind) {
    std::ifstream in(path);
    std::string magic, actual;
    if (!(in >> magic >> actual) || magic!="CETE_FROZEN_V1" || actual!=kind)
        throw std::runtime_error("3.19D: wrong checkpoint type/version: "+path.string());
    P p;
    for (auto* v : {&p.weights1,&p.biases1,&p.weights2,&p.biases2,
                    &p.weights3,&p.biases3}) {
        std::size_t n=0;
        if (!(in>>n) || n==0 || n>1000000)
            throw std::runtime_error("3.19D: invalid checkpoint vector length");
        v->resize(n);
        for (auto& x : *v)
            if (!(in>>x) || !std::isfinite(x))
                throw std::runtime_error("3.19D: corrupt checkpoint weight");
    }
    std::string extra;
    if (in>>extra) throw std::runtime_error("3.19D: trailing checkpoint data");
    return p;
}
}
void saveDevFrozenAgentCheckpoint(const std::filesystem::path& path,
                                  const DevQNetwork& network) {
    save(path,"AGENT",network.parameters());
}
void saveDevFrozenManagerCheckpoint(const std::filesystem::path& path,
                                    const DevManagerQNetwork& network) {
    save(path,"MANAGER",network.parameters());
}
DevFrozenOOSPolicyAdapter::DevFrozenOOSPolicyAdapter(
    const std::filesystem::path& agent_checkpoint,
    const std::filesystem::path& manager_checkpoint)
    : agent_(std::make_unique<DevQNetwork>(DevQNetworkConfig{73117U,true})),
      manager_(std::make_unique<DevManagerQNetwork>(
          DevManagerNetworkConfig{73117U,true})) {
    agent_->setParameters(load<DevQNetworkParameters>(agent_checkpoint,"AGENT"));
    manager_->setParameters(load<DevManagerNetworkParameters>(manager_checkpoint,"MANAGER"));
}
std::string DevFrozenOOSPolicyAdapter::operator()(
    const DevOOSDecisionPoint& point, const DevOOSLedgerState& ledger) const {
    if (!point.numerical_integrity || point.decision_time<=ledger.last_time)
        throw std::runtime_error("3.19D: invalid/nonchronological decision");
    DevManagerPortfolioState portfolio{};
    portfolio.current_symbol=ledger.held_symbol;
    portfolio.portfolio.position=ledger.quantity>0 ? DEV_POSITION_LONG : DEV_POSITION_FLAT;
    // Phase 3.19B ledger has no entry-price/holding-duration fields. Do not
    // fabricate unrealized return or holding period; those are zero here.
    portfolio.numerical_integrity=true;
    std::vector<DevAgentReport> reports;
    reports.reserve(point.candidates.size());
    for (const auto& c : point.candidates) {
        auto observation=c.observation;
        // Do not expose cached portfolio features from a different ledger state.
        observation.portfolio=portfolio.portfolio;
        const auto q=agent_->predict(observation);
        DevAgentReport r{};
        r.slot_index=c.assignment.slot_index;
        r.agent_name=c.assignment.agent_name;
        r.assigned=true;
        r.symbol=c.assignment.symbol;
        r.decision_time=point.decision_time;
        r.candidate_score=c.assignment.candidate_score;
        r.candidate_rank=c.assignment.candidate_rank;
        r.q_values=q;
        r.preferred_action=q.greedyAction();
        // Cash equities: short is not an eligible new long entry.
        r.preferred_q_value=q.value(r.preferred_action);
        r.numerical_integrity=q.numerical_integrity;
        reports.push_back(r);
    }
    DevManager architecture;
    const auto state=architecture.buildState(point.decision_time,reports,portfolio);
    DevManagerStateEncoder encoder;
    auto encoded=encoder.encode(state);
    // Cash equities: manager cannot select an agent requesting SHORT/EXIT.
    for (std::size_t i=0;i<reports.size();++i) {
        if (reports[i].preferred_action!=DevAction::LONG)
            encoded.valid_actions[i+2]=false;
    }
    const auto q=manager_->predict(encoded);
    const auto action=q.greedyAction(encoded.valid_actions);
    if (action==DevManagerAction::NO_TRADE) return {};
    if (action==DevManagerAction::KEEP_CURRENT) return ledger.held_symbol;
    const auto slot=static_cast<std::size_t>(action)-2;
    if (slot>=reports.size() || !encoded.valid_actions[slot+2])
        throw std::runtime_error("3.19D: invalid manager slot");
    return reports[slot].symbol;
}
}
