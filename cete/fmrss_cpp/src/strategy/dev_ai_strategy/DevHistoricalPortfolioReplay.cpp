#include "devai/strategy/dev_ai_strategy/DevHistoricalPortfolioReplay.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevModelPersistence.hpp"
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace devai::strategy {
namespace {
struct Candle { double open{},close{}; };
using CandleMap=std::map<std::int64_t,Candle>;
std::int64_t istEpoch(const std::string& date,const std::string& time) {
    std::tm tm{};std::istringstream ss(date+" "+time);
    ss>>std::get_time(&tm,"%Y-%m-%d %H:%M:%S");
    if(ss.fail()) { tm={};std::istringstream alt(date+" "+time);alt>>std::get_time(&tm,"%Y-%m-%d %H:%M");if(alt.fail()) throw std::runtime_error("Bad date/time: "+date+" "+time); }
    return static_cast<std::int64_t>(timegm(&tm))-19800;
}
std::string istDay(std::int64_t epoch) {
    const std::time_t tt=static_cast<std::time_t>(epoch+19800);
    std::tm tm{};gmtime_r(&tt,&tm);char s[16];std::strftime(s,sizeof(s),"%Y-%m-%d",&tm);return s;
}
CandleMap readCandles(const std::filesystem::path& path,const std::string& symbol) {
    std::ifstream in(path);if(!in) throw std::runtime_error("Cannot open 1m file: "+path.string());
    CandleMap out;std::string line;
    while(std::getline(in,line)) {
        if(line.empty()) continue;
        std::stringstream ss(line);std::vector<std::string> f;std::string v;
        while(std::getline(ss,v,',')) {v.erase(std::remove(v.begin(),v.end(),'\r'),v.end());f.push_back(v);}
        if(f.size()<7 || f[0]=="Symbol" || f[0]=="symbol") continue;
        if(f[0]!=symbol) throw std::runtime_error("Unexpected symbol in "+path.string());
        auto t=istEpoch(f[1],f[2]);double o=std::stod(f[3]),c=std::stod(f[6]);
        if(!std::isfinite(o)||!std::isfinite(c)||o<=0||c<=0) throw std::runtime_error("Invalid price");
        if(!out.emplace(t,Candle{o,c}).second) throw std::runtime_error("Duplicate candle time");
    }
    if(out.empty()) throw std::runtime_error("No candles in "+path.string());
    return out;
}
void overlayPortfolio(DevAIObservation& o,const DevPortfolioRuntimeState& s,double initial_cash) {
    o.portfolio.position = s.position ? static_cast<double>(s.position->side) : 0.0;
    o.portfolio.unrealized_return = 0.0; // Cannot infer from this interface without cost basis and current quote
    o.portfolio.realized_return = s.realized_pnl / initial_cash;
    o.portfolio.equity_return = (s.equity-initial_cash)/initial_cash;
    o.portfolio.drawdown = s.drawdown_fraction;
    o.portfolio.holding_period = s.position ? static_cast<double>((s.time-s.position->opened_at)/60) : 0.0;
}
}
DevHistoricalPortfolioReplay::DevHistoricalPortfolioReplay(DevHistoricalPortfolioReplayConfig config):config_(std::move(config)) {
    if(config_.minute_folder.empty() || config_.audit_csv.empty() || config_.maximum_holding_bars==0 ||
       !std::isfinite(config_.minimum_q_advantage)) throw std::invalid_argument("Invalid replay configuration");
}
DevHistoricalPortfolioReplayStats DevHistoricalPortfolioReplay::run(const DevHistoricalTrainingDataset& dataset) const {
    DevHistoricalTrainingDatasetValidator::validate(dataset);
    if(dataset.records.size()<2) throw std::invalid_argument("At least 2 records needed for causal replay");
    // Require a continuous, same-session 1-minute slice; refuse to silently skip missing minutes.
    for(std::size_t i=1;i<dataset.records.size();++i)
        if(dataset.records[i].decision_time-dataset.records[i-1].decision_time!=60 ||
           istDay(dataset.records[i].decision_time)!=istDay(dataset.records[i-1].decision_time))
            throw std::runtime_error("Replay requires contiguous single-session 1m records");
    std::set<std::string> symbols;
    for(const auto& r:dataset.records) for(const auto& c:r.candidates) symbols.insert(c.symbol);
    // All selected symbols are loaded, including symbols held after they leave the top-five list.
    std::map<std::string,CandleMap> candles;
    for(const auto& symbol:symbols) candles.emplace(symbol,readCandles(config_.minute_folder/(symbol+"_1min.txt"),symbol));
    std::unique_ptr<DevQNetwork> model;
    if(!config_.model_checkpoint.empty()) {
        model=std::make_unique<DevQNetwork>();
        DevModelPersistence persistence;
        if(!persistence.load(*model,config_.model_checkpoint).numerical_integrity)
            throw std::runtime_error("Model checkpoint failed integrity check");
    }
    if(config_.audit_csv.has_parent_path()) std::filesystem::create_directories(config_.audit_csv.parent_path());
    std::ofstream out(config_.audit_csv);
    if(!out) throw std::runtime_error("Cannot open audit CSV");
    out<<"source_observation_time,execution_candle_time,agent,symbol,requested_action,executed,reason,reward,equity,cash,realized_pnl,position_symbol,position_side,terminal\n";
    out<<std::setprecision(15);
    DevPortfolioEnvironment env(config_.portfolio);
    DevHistoricalPortfolioReplayStats stats{};
    const auto& records=dataset.records;
    for (std::size_t i = 1; i < records.size(); ++i) {

        const auto& previous = records[i - 1];
        const auto time = records[i].decision_time;

        DevPortfolioFrame frame;
        frame.time = time;
        frame.session_end = (i + 1 == records.size());

        // The final decision boundary is 15:15 IST.
        // The last available 1-minute candle begins at 15:14.
        // Its closing price represents the 15:15 boundary.

        const bool final_boundary = frame.session_end;

        for (const auto& [symbol, series] : candles) {

            auto it = series.find(time);

            if (it == series.end() && final_boundary) {
                it = series.find(time - 60);
            }

            if (it == series.end()) {
                throw std::runtime_error(
                    "Missing execution candle " +
                    symbol +
                    " at " +
                    std::to_string(time)
                );
            }

            frame.quotes.push_back({
                symbol,
                it->second.open,
                it->second.close
            });
        }
        DevPortfolioDecision decision{};
        const auto& state=env.state();
        if(state.position) {
            const auto held_bars=(time-state.position->opened_at)/60;
            if(held_bars>=static_cast<std::int64_t>(config_.maximum_holding_bars))
                decision={state.position->agent_name,state.position->symbol,DevPortfolioAction::EXIT};
        } else if(model) {
            double best_advantage=config_.minimum_q_advantage;
            for(const auto& candidate:previous.candidates) {
                DevAIObservation observation=candidate.observation;
                overlayPortfolio(observation,state,config_.portfolio.initial_cash);
                auto q=model->predict(observation);
                if(!q.numerical_integrity) throw std::runtime_error("Non-finite model output");
                for(const auto& [action,portfolio_action]:{
                    std::pair{DevAction::LONG,DevPortfolioAction::LONG},
                    std::pair{DevAction::SHORT,DevPortfolioAction::SHORT}}) {
                    const double advantage=q.value(action)-q.value(DevAction::HOLD);
                    if(advantage>best_advantage) {
                        best_advantage=advantage;
                        decision={candidate.agent_name,candidate.symbol,portfolio_action};
                    }
                }
            }
        }
        // Without a checkpoint, HOLD-only is intentional: validates the replay plumbing
        // without inventing a trading strategy or using future outcomes as signals.
        const auto step=env.step(frame,decision);
        ++stats.steps;stats.total_reward+=step.reward;
        if(step.executed) {
            if(decision.action==DevPortfolioAction::EXIT) ++stats.exited;
            else ++stats.opened;
        } else if(decision.action==DevPortfolioAction::HOLD) ++stats.hold_steps;
        else ++stats.rejected;
        const auto action=[](DevPortfolioAction a){switch(a){case DevPortfolioAction::LONG:return "LONG";case DevPortfolioAction::SHORT:return "SHORT";case DevPortfolioAction::EXIT:return "EXIT";default:return "HOLD";}}(decision.action);
        out<<previous.decision_time<<','<<time<<','<<decision.agent_name<<','<<decision.symbol<<','<<action<<','
           <<(step.executed?1:0)<<','<<step.reason<<','<<step.reward<<','<<step.after.equity<<','
           <<step.after.cash<<','<<step.after.realized_pnl<<','
           <<(step.after.position?step.after.position->symbol:"")<<','
           <<(step.after.position?step.after.position->side:0)<<','<<(step.after.terminal?1:0)<<'\n';
    }
    stats.ending_equity=env.state().equity;
    if(!env.state().terminal || env.state().position) throw std::runtime_error("Replay failed terminal liquidation");
    return stats;
}
}
