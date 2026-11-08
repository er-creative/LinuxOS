// CETE Step 5E: diagnostic-only harness. Reuses the exact D9C CSV/session/replay code.
// Does not change the trading model or save/promote a production model.
#define main d9c_training_main_not_called
#include "run_dev_d9c_shared_rl_training.cpp"
#undef main
#include <cstdlib>
#include <unistd.h>
#include <numeric>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("STEP5E_ASSERT " + message);
}
DevD9BObservation observation(const std::string& symbol, Key key, double close) {
    DevD9BObservation o;
    o.symbol=symbol; o.decision_key=key; o.close=close;
    o.forecast_ready=true; o.predicted_forward_return=0.00001;
    return o;
}
// A: exact synthetic event-level ledger, including the mandatory 15:15 liquidation.
void syntheticLedger() {
    const Key day=dateKey("2025-07-01");
    for (double bps : {0.0,2.0,5.0,10.0,20.0}) {
        DevD9BConfig cfg; cfg.round_trip_cost_bps=bps;
        DevD9BEnvironment env(cfg);
        auto a=observation("TEST",day+899,100.0);
        auto b=observation("TEST",day+900,101.0);
        auto c=observation("TEST",day+915,102.0);
        auto open=env.step(a,DevD9AAction::OPEN_LONG);
        auto hold=env.step(b,DevD9AAction::HOLD_LONG);
        for (int minute=901;minute<=914;++minute) {
            auto intermediate=observation("TEST",day+minute,101.0);
            (void)env.step(intermediate,DevD9AAction::HOLD_LONG);
        }
        auto close=env.step(c,DevD9AAction::FORCE_EXIT);
        const double gross=(101.0/100.0-1.0)+(102.0/101.0-1.0);
        const double costs=open.total_cost+hold.total_cost+close.total_cost;
        const double net=open.reward+hold.reward+close.reward;
        require(std::abs(costs-bps/10000.0)<1e-12,"round-trip cost mismatch");
        require(std::abs(net-(gross-bps/10000.0))<1e-12,"mark-to-market ledger mismatch");
        require(close.terminal && close.position_after==DevD9APosition::FLAT,"15:15 not flat");
        // The runner's pending attribution: the action at 09:13 earns 09:13->09:14,
        // and the final HOLD at 09:14 earns 09:14->09:15 plus terminal exit cost.
        const double open_transition=-open.total_cost+hold.reward+hold.total_cost;
        const double hold_transition=-hold.total_cost+close.reward;
        require(std::abs(open_transition+hold_transition-net)<1e-12,
                "pending transition reward does not reconcile with ledger");
        std::cout<<"STEP5E_SYNTHETIC_LEDGER PASS cost_bps="<<bps
                 <<" gross="<<gross<<" costs="<<costs<<" net="<<net
                 <<" terminal_flat=yes pending_reconciled=yes\n";
    }
    DevD9BConfig cfg;
    DevD9BEnvironment env(cfg);
    auto a=observation("TEST",dateKey("2025-07-01")+899,100);
    auto b=observation("TEST",dateKey("2025-07-01")+915,101);
    (void)env.step(a,DevD9AAction::OPEN_LONG);
    bool rejected=false;
    try { (void)env.step(b,DevD9AAction::FORCE_EXIT); }
    catch(const std::runtime_error&) {rejected=true;}
    require(rejected,"missing minute while holding must fail");
    std::cout<<"STEP5E_MISSING_MINUTE_GUARD PASS\n";
}

// Deterministic statistical baseline: top-five selected + z1 <= -1,
// one long per symbol; exit on z1>=0, after five completed minutes, or at 15:15.
// The threshold and five-minute hold are diagnostic choices, not optimized parameters.
Score statisticalBenchmark(const Sessions& days, Key begin, Key end, double cost_bps) {
    Score result;
    DevD9BConfig cfg;cfg.round_trip_cost_bps=cost_bps;
    cfg.permissions.enable_long=true;cfg.permissions.enable_short=false;
    for(const auto& [day,rows] : days) {
        if(day*1440<begin || day*1440>=end) continue;
        DevD9BEnvironment env(cfg);
        std::map<std::string,Key> entry;
        for(const auto& r : rows) {
            const auto& o=r.obs;
            const auto p=env.position(o.symbol).side;
            auto legal=env.legalActions(o);
            DevD9AAction action=DevD9AAction::HOLD_FLAT;
            const int m=static_cast<int>((o.decision_key%1440+1440)%1440);
            if(m>=cfg.permissions.force_exit_minutes)
                action=p==DevD9APosition::FLAT?DevD9AAction::HOLD_FLAT:DevD9AAction::FORCE_EXIT;
            else if(p==DevD9APosition::LONG) {
                const bool exit=(o.decision_key-entry.at(o.symbol)>=5 || o.zscore_1m>=0);
                action=exit?DevD9AAction::CLOSE_LONG:DevD9AAction::HOLD_LONG;
            } else if(r.selected && o.zscore_1m<=-1 && hasAction(legal,DevD9AAction::OPEN_LONG)) {
                action=DevD9AAction::OPEN_LONG;
                entry[o.symbol]=o.decision_key;
            }
            const auto s=env.step(o,action);
            result.reward+=s.reward;result.transaction_cost+=s.total_cost;
            result.gross_return+=s.reward+s.total_cost;
            ++result.decisions;
            if(action==DevD9AAction::OPEN_LONG)++result.opens;
            if(action==DevD9AAction::FORCE_EXIT)++result.forced;
        }
        for(const auto& r:rows) require(env.position(r.obs.symbol).side==DevD9APosition::FLAT,
                                         "benchmark overnight position");
        ++result.sessions;
    }
    return result;
}

void reportScore(const char* label, const Score& s, double cost) {
    std::cout<<"STEP5E_SCORE phase="<<label<<" cost_bps="<<cost
             <<" sessions="<<s.sessions<<" opens="<<s.opens
             <<" decisions="<<s.decisions<<" gross="<<s.gross_return
             <<" costs="<<s.transaction_cost<<" net="<<s.reward
             <<" net_per_session="<<(s.sessions?s.reward/s.sessions:0.0)<<'\n';
}
void runMode(const Sessions& days, Key train, Key val, Key oos,
             bool forecast_on, double cost_bps, const ForecastThresholds& thresholds) {
    const char* mode=forecast_on?"forecast_on":"forecast_off";
    std::cout<<"STEP5E_MODE_BEGIN mode="<<mode<<" cost_bps="<<cost_bps<<'\n';
    struct Trial {double alpha,gamma,epsilon;};
    const std::array<Trial,6> grid{{{0.02,0.90,0.05},{0.02,0.95,0.10},
       {0.05,0.90,0.05},{0.05,0.95,0.10},{0.10,0.90,0.05},{0.10,0.95,0.10}}};
    std::size_t trial_id=0;
    for(const auto& t:grid) {
        DevD9BConfig cfg;
        cfg.permissions.enable_long=true;cfg.permissions.enable_short=false;
        cfg.enable_predicted_forward_return=forecast_on;
        cfg.forecast_inner_abs=forecast_on?thresholds.inner:0;
        cfg.forecast_outer_abs=forecast_on?thresholds.outer:0;
        cfg.round_trip_cost_bps=cost_bps;
        cfg.q_alpha=t.alpha;cfg.q_gamma=t.gamma;cfg.epsilon=t.epsilon;cfg.seed=17;
        DevD9BSharedQModel model(cfg);
        const auto training=replay(days,train,val,model,cfg,true);
        const auto validation=replay(days,val,oos,model,cfg,false);
        ++trial_id;
        std::cout<<"STEP5E_TRIAL mode="<<mode<<" cost_bps="<<cost_bps
                 <<" trial="<<trial_id<<" alpha="<<t.alpha<<" gamma="<<t.gamma
                 <<" epsilon="<<t.epsilon<<" states="<<model.stateCount()
                 <<" train_opens="<<training.opens<<" train_gross="<<training.gross_return
                 <<" train_costs="<<training.transaction_cost<<" train_net="<<training.reward
                 <<" val_opens="<<validation.opens<<" val_gross="<<validation.gross_return
                 <<" val_costs="<<validation.transaction_cost<<" val_net="<<validation.reward<<'\n';
        // Snapshot via versioned public interface; no model output is retained.
        // A unique filename per process avoids conflicts with other runs.
        const std::string snapshot="/tmp/cete_step5e_qdiag_"+std::to_string(static_cast<long long>(::getpid()))+".tmp";
        model.save(snapshot);
        const auto q=readQSnapshot(snapshot);
        require(std::remove(snapshot.c_str())==0,"cannot remove temporary Q snapshot");
        const auto qtrain=qDiagnostics(days,train,val,q,model,forecast_on);
        const auto qval=qDiagnostics(days,val,oos,q,model,forecast_on);
        std::cout<<"STEP5E_Q_DIAG mode="<<mode<<" cost_bps="<<cost_bps
                 <<" trial="<<trial_id<<" train_positive="<<qtrain.positive
                 <<" train_negative="<<qtrain.negative<<" train_ties="<<qtrain.ties
                 <<" val_positive="<<qval.positive<<" val_negative="<<qval.negative
                 <<" val_ties="<<qval.ties<<" val_unseen="<<qval.unseen
                 <<" val_mean_adv="<<(qval.eligible?qval.sum_advantage/qval.eligible:0)<<'\n';
    }
    std::cout<<"STEP5E_MODE_END mode="<<mode<<" cost_bps="<<cost_bps<<'\n';
}
} // namespace

int main(int argc,char** argv) {
    try {
        if(argc!=5) throw std::invalid_argument(
          "Usage: run_dev_d9e_diagnostics causal.csv TRAIN_START VALIDATION_START OOS_START");
        const Key train=dateKey(argv[2]),val=dateKey(argv[3]),oos=dateKey(argv[4]);
        require(train<val && val<oos,"invalid partition order");
        std::cout<<std::fixed<<std::setprecision(10);
        syntheticLedger();
        const auto rows=load(argv[1]);
        Diagnostics d;const auto days=sessions(rows,d,train,val,oos);
        printDiagnostics(d);
        require(d.accepted_train>0 && d.accepted_val>0,"missing accepted train or validation sessions");
        const auto thresholds=calibrateForecast(days,train,val);
        std::cout<<"STEP5E_TRAIN_ONLY_FORECAST_CALIBRATION count="<<thresholds.n
                 <<" inner="<<thresholds.inner<<" outer="<<thresholds.outer<<'\n';
        std::cout<<"STEP5E_OOS_GUARD oos_evaluated=no model_saved=no\n";
        for(double cost : {0.0,2.0,5.0,10.0,20.0}) {
            reportScore("TRAIN_STATISTICAL_BENCHMARK",statisticalBenchmark(days,train,val,cost),cost);
            reportScore("VALIDATION_STATISTICAL_BENCHMARK",statisticalBenchmark(days,val,oos,cost),cost);
            runMode(days,train,val,oos,false,cost,thresholds);
            runMode(days,train,val,oos,true,cost,thresholds);
        }
        std::cout<<"STEP5E_DIAGNOSTICS_COMPLETE no_model_approved=yes oos_evaluated=no\n";
    } catch(const std::exception& e) {
        std::cerr<<"STEP5E_FAIL "<<e.what()<<'\n';return 1;
    }
}
