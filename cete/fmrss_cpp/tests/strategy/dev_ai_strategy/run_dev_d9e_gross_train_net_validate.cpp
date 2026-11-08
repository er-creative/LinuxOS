// CETE D9E revised: train on gross rewards; evaluate identical frozen policy net of costs.
// Does not save/approve a model or access the OOS partition.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#endif
#define main d9c_main_unused
#include "run_dev_d9c_shared_rl_training.cpp"
#undef main
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <array>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
struct GuardConfig {
    int min_hold_minutes=5;
    int cooldown_minutes=5;
    int max_entries_per_symbol_session=2;
};
struct SymbolGuard {
    Key entry=0;
    Key last_exit=0;
    int entries=0;
    bool exited=false;
};
struct Result {
    Score score;
    std::size_t blocked_cooldown=0,blocked_daily_cap=0,blocked_min_hold=0;
    std::size_t completed_trades=0,short_holds=0;
    double gross_per_entry=0;
};
void check(bool ok,const std::string& msg){if(!ok)throw std::runtime_error("D9E_ASSERT "+msg);}

std::vector<DevD9AAction> restrictActions(
    std::vector<DevD9AAction> legal,const DevD9BObservation& obs,
    DevD9APosition pos,const SymbolGuard& g,const GuardConfig& guard,
    Result& result) {
    const bool entry=hasAction(legal,DevD9AAction::OPEN_LONG);
    if(pos==DevD9APosition::FLAT && entry) {
        if(g.entries>=guard.max_entries_per_symbol_session) {
            legal.erase(std::remove(legal.begin(),legal.end(),DevD9AAction::OPEN_LONG),legal.end());
            ++result.blocked_daily_cap;
        } else if(g.exited && obs.decision_key-g.last_exit<guard.cooldown_minutes) {
            legal.erase(std::remove(legal.begin(),legal.end(),DevD9AAction::OPEN_LONG),legal.end());
            ++result.blocked_cooldown;
        }
    }
    if(pos==DevD9APosition::LONG && hasAction(legal,DevD9AAction::CLOSE_LONG) &&
       obs.decision_key-g.entry<guard.min_hold_minutes) {
        legal.erase(std::remove(legal.begin(),legal.end(),DevD9AAction::CLOSE_LONG),legal.end());
        ++result.blocked_min_hold;
    }
    // FORCE_EXIT is never masked; mandatory session liquidation takes priority.
    check(!legal.empty(),"empty legal mask");
    return legal;
}

Result guardedReplay(const Sessions& sessions,Key begin,Key end,
    DevD9BSharedQModel& model,DevD9BConfig cfg,bool learn,
    const GuardConfig& guard) {
    Result result;
    for(const auto& [day,rows]:sessions) {
        if(day*1440<begin || day*1440>=end)continue;
        DevD9BEnvironment env(cfg);
        std::map<std::string,Pending> pending;
        std::map<std::string,SymbolGuard> guards;
        for(const auto& r:rows) {
            const auto& obs=r.obs;
            const auto pos=env.position(obs.symbol).side;
            auto legal=env.legalActions(obs);
            if(pos==DevD9APosition::FLAT && !r.selected) {
                legal.erase(std::remove(legal.begin(),legal.end(),DevD9AAction::OPEN_LONG),legal.end());
                legal.erase(std::remove(legal.begin(),legal.end(),DevD9AAction::OPEN_SHORT),legal.end());
            }
            auto& g=guards[obs.symbol];
            legal=restrictActions(std::move(legal),obs,pos,g,guard,result);
            const auto action=model.choose(obs,pos,legal,learn);
            const auto step=env.step(obs,action);
            auto& p=pending[obs.symbol];
            if(p.valid && learn) {
                const double reward=p.entry_action_cost+step.reward+(step.terminal?0.0:step.total_cost);
                model.learn(p.obs,p.pos,p.action,reward,obs,pos,legal,step.terminal);
                ++result.score.learning_updates;
                if(step.terminal)++result.score.terminal_updates;
            }
            if(action==DevD9AAction::OPEN_LONG) {
                ++g.entries;g.entry=obs.decision_key;
                ++result.score.opens;++result.score.long_choices;
            } else if(action==DevD9AAction::CLOSE_LONG ||
                      (action==DevD9AAction::FORCE_EXIT && pos==DevD9APosition::LONG)) {
                if(obs.decision_key-g.entry<guard.min_hold_minutes)++result.short_holds;
                ++result.completed_trades;
                g.exited=true;g.last_exit=obs.decision_key;
                if(action==DevD9AAction::CLOSE_LONG)++result.score.close_long;
                else ++result.score.forced;
            } else if(action==DevD9AAction::HOLD_FLAT)++result.score.flat_choices;
            else if(action==DevD9AAction::HOLD_LONG)++result.score.hold_long;
            else if(action==DevD9AAction::FORCE_EXIT)++result.score.forced;
            result.score.reward+=step.reward;
            result.score.transaction_cost+=step.total_cost;
            result.score.gross_return+=step.reward+step.total_cost;
            ++result.score.decisions;
            if(step.terminal) {
                check(step.position_after==DevD9APosition::FLAT,"terminal exposure");
                p.valid=false;
            } else p={true,obs,pos,action,-step.total_cost};
        }
        for(const auto& [symbol,p]:pending)
            check(!p.valid && env.position(symbol).side==DevD9APosition::FLAT,"overnight exposure");
        ++result.score.sessions;
    }
    return result;
}
void printResult(const std::string& mode,int trial,const char* phase,double cost,
                 const Result& r){
    std::cout<<"D9E_RESULT mode="<<mode<<" trial="<<trial<<" phase="<<phase
             <<" cost_bps="<<cost<<" sessions="<<r.score.sessions
             <<" opens="<<r.score.opens<<" completed="<<r.completed_trades
             <<" gross="<<r.score.gross_return<<" costs="<<r.score.transaction_cost
             <<" net="<<r.score.reward
             <<" gross_bps_per_entry="<<(r.score.opens?10000*r.score.gross_return/r.score.opens:0)
             <<" net_bps_per_entry="<<(r.score.opens?10000*r.score.reward/r.score.opens:0)
             <<" blocked_cooldown="<<r.blocked_cooldown
             <<" blocked_daily_cap="<<r.blocked_daily_cap
             <<" blocked_min_hold="<<r.blocked_min_hold
             <<" forced_short_holds="<<r.short_holds<<'\n';
}
void guardTests(){
    GuardConfig guard;
    DevD9BConfig cfg;cfg.round_trip_cost_bps=0;
    DevD9BEnvironment env(cfg);
    DevD9BObservation o;o.symbol="TEST";o.close=100;o.decision_key=dateKey("2025-07-01")+600;
    SymbolGuard g;Result r;
    auto a=restrictActions(env.legalActions(o),o,DevD9APosition::FLAT,g,guard,r);
    check(hasAction(a,DevD9AAction::OPEN_LONG),"first entry blocked");
    auto s=env.step(o,DevD9AAction::OPEN_LONG);(void)s;
    g.entries=1;g.entry=o.decision_key;
    ++o.decision_key;
    a=restrictActions(env.legalActions(o),o,DevD9APosition::LONG,g,guard,r);
    check(!hasAction(a,DevD9AAction::CLOSE_LONG),"minimum hold not enforced");
    o.decision_key+=4;
    a=restrictActions(env.legalActions(o),o,DevD9APosition::LONG,g,guard,r);
    check(hasAction(a,DevD9AAction::CLOSE_LONG),"hold restriction not released");
    g.exited=true;g.last_exit=o.decision_key;
    auto flat=std::vector<DevD9AAction>{DevD9AAction::HOLD_FLAT,DevD9AAction::OPEN_LONG};
    a=restrictActions(flat,o,DevD9APosition::FLAT,g,guard,r);
    check(!hasAction(a,DevD9AAction::OPEN_LONG),"cooldown not enforced");
    o.decision_key+=5;
    a=restrictActions(flat,o,DevD9APosition::FLAT,g,guard,r);
    check(hasAction(a,DevD9AAction::OPEN_LONG),"cooldown not released");
    g.entries=2;
    a=restrictActions(flat,o,DevD9APosition::FLAT,g,guard,r);
    check(!hasAction(a,DevD9AAction::OPEN_LONG),"daily entry cap not enforced");
    std::cout<<"D9E_GUARD_TEST PASS min_hold=5 cooldown=5 max_entries=2\n";
}
} // namespace

int main(int argc,char** argv){
 try {
    if(argc!=7)throw std::invalid_argument(
       "Usage: run_dev_d9e_gross_train_net_validate causal.csv TRAIN_START VALIDATION_START OOS_START forecast_on|forecast_off validation_cost_bps");
    const Key train=dateKey(argv[2]),val=dateKey(argv[3]),oos=dateKey(argv[4]);
    check(train<val&&val<oos,"invalid chronological split");
    const std::string mode=argv[5];
    check(mode=="forecast_on"||mode=="forecast_off","invalid forecast mode");
    const double eval_cost=std::stod(argv[6]);
    check(std::isfinite(eval_cost)&&eval_cost>0&&eval_cost<=100,"validation cost must be in (0,100] bps");
    std::cout<<std::fixed<<std::setprecision(10);
    guardTests();
    auto raw=load(argv[1]);Diagnostics diagnostics;
    const auto days=sessions(raw,diagnostics,train,val,oos);
    printDiagnostics(diagnostics);
    check(diagnostics.accepted_train>0&&diagnostics.accepted_val>0,"empty partition");
    const bool forecast=mode=="forecast_on";
    const auto thresholds=forecast?calibrateForecast(days,train,val):ForecastThresholds{};
    std::cout<<"D9E_MODE "<<mode<<" train_cost_bps=0 validation_cost_bps="<<eval_cost
             <<" forecast_calibration_count="<<thresholds.n
             <<" forecast_target_horizon=UNVERIFIED\n";
    std::cout<<"D9E_GUARDS min_hold_minutes=5 cooldown_minutes=5 max_entries_per_symbol_session=2"
             <<" forced_exit=15:15\n";
    struct Trial{double alpha,gamma,epsilon;};
    const std::array<Trial,6> grid{{{.02,.90,.05},{.02,.95,.10},{.05,.90,.05},
                                    {.05,.95,.10},{.10,.90,.05},{.10,.95,.10}}};
    int trial_id=0;
    for(const auto& t:grid){
        ++trial_id;
        DevD9BConfig train_cfg;
        train_cfg.permissions.enable_long=true;
        train_cfg.permissions.enable_short=false;
        train_cfg.round_trip_cost_bps=0;
        train_cfg.enable_predicted_forward_return=forecast;
        train_cfg.forecast_inner_abs=forecast?thresholds.inner:0;
        train_cfg.forecast_outer_abs=forecast?thresholds.outer:0;
        train_cfg.q_alpha=t.alpha;train_cfg.q_gamma=t.gamma;
        train_cfg.epsilon=t.epsilon;train_cfg.seed=17;
        DevD9BSharedQModel model(train_cfg);
        GuardConfig guard;
        const auto tr=guardedReplay(days,train,val,model,train_cfg,true,guard);
        printResult(mode,trial_id,"TRAIN_GROSS",0,tr);
        auto eval_cfg=train_cfg;
        eval_cfg.round_trip_cost_bps=eval_cost;
        const auto validation=guardedReplay(days,val,oos,model,eval_cfg,false,guard);
        printResult(mode,trial_id,"VALIDATION_NET",eval_cost,validation);
        check(validation.score.transaction_cost>=0,"negative validation costs");
        check(std::abs(validation.score.reward-(validation.score.gross_return-validation.score.transaction_cost))<1e-7,
              "validation net reconciliation failed");
    }
    std::cout<<"D9E_COMPLETE training_cost=zero validation_cost=enabled "
                "model_saved=no model_approved=no oos_evaluated=no\n";
 } catch(const std::exception& e){std::cerr<<"D9E_FAIL "<<e.what()<<'\n';return 1;}
}
