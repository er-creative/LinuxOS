#include "devai/benchmark/DevRuntimeReplayBridge.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
namespace devai::benchmark::h45 {
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::invalid_argument(message); }
int minuteIST(std::int64_t utc) {
    auto seconds=(utc+19800)%86400;
    if(seconds<0)seconds+=86400;
    return static_cast<int>(seconds/60);
}
}
DecisionBatch DevRuntimeReplayBridge::inferLong(const IDevRLModel& model,
    const std::vector<HistoricalManagerObservation>& observations) {
    check(!observations.empty(),"H4.5: empty historical observation sequence");
    DecisionBatch result;
    std::string held;
    std::int64_t previous=0;
    bool previous_terminal=false;
    for(const auto& o:observations) {
        check(o.decision_time_utc>previous && o.decision_time_utc>0,
              "H4.5: nonchronological timestamps");
        check(!previous_terminal,"H4.5: observation after terminal");
        check(o.completed_candles && o.causal_features,
              "H4.5: incomplete or unverified causal observation");
        for(double x:o.encoded_manager_features)
            check(std::isfinite(x),"H4.5: nonfinite manager features");
        std::unordered_set<std::string> symbols;
        for(const auto& symbol:o.ranked_top_five)
            check(!symbol.empty() && symbols.insert(symbol).second,
                  "H4.5: missing or duplicate ranked symbol");
        // H4.6: actions 2..6 are five LONG candidate slots.
        auto legal=o.legal_actions;
        const bool cutoff=minuteIST(o.decision_time_utc)>=915;
        if(cutoff || o.terminal) {
            legal.fill(false);legal[0]=true;
        } else {
            if(held.empty())legal[1]=false;
            else {
                legal[1]=true;
                for(std::size_t a=2;a<=6;++a)
                    if(o.ranked_top_five[a-2]==held)legal[a]=false;
            }
        }
        DevRLRequest request{};
        request.role=DevRLRole::Manager;
        request.features=o.encoded_manager_features;
        request.legal_actions=legal;
        auto decision=model.evaluate(request);
        check(decision.valid && decision.selected_action<7 &&
              legal[decision.selected_action],"H4.5: invalid model decision");
        const auto action=decision.selected_action;
        std::string desired;
        if(action==1) {check(!held.empty(),"H4.5: KEEP without position");desired=held;}
        else if(action>=2 && action<=6)desired=o.ranked_top_five[action-2];
        // 0 = FLAT/EXIT, not WAIT-with-existing-position.
        devai::strategy::d9f4c::ReplayDecision replay{};
        replay.decision_time=o.decision_time_utc;
        replay.manager_action=action;
        replay.legal_actions=legal;
        replay.desired_symbol=desired;
        replay.terminal=o.terminal;
        result.replay_decisions.push_back(replay);
        result.audit.push_back({o.decision_time_utc,action,action,desired});
        held=desired;
        previous=o.decision_time_utc;
        previous_terminal=o.terminal;
    }
    check(held.empty(),"H4.5: last decision must flatten portfolio");
    check(result.replay_decisions.back().terminal,
          "H4.5: final decision must be terminal");
    return result;
}
devai::strategy::d9f4c::ReplayResult DevRuntimeReplayBridge::executeLong(
    const DecisionBatch& batch, const devai::strategy::DevOOSPriceProvider& prices,
    devai::strategy::DevOOSLedgerConfig config) {
    check(!config.allow_intraday_short,"H4.5: SHORT replay not supported");
    return devai::strategy::d9f4c::DevRealLedgerReplay::run(batch.replay_decisions,prices,config);
}
}
