#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevModelPersistence.hpp"
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace ds = devai::strategy;

struct RewardRow {
    std::int64_t decision_time{};
    std::string agent, symbol;
    std::size_t horizon{};
    bool available{};
    double long_reward{}, short_reward{};
};
using Key=std::tuple<std::int64_t,std::string,std::string,std::size_t>;

std::map<Key,RewardRow> loadRewards(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot read reward CSV: "+path.string());
    std::string line;
    if (!std::getline(in,line) ||
        line!="decision_time,agent,symbol,horizon_bars,available,entry_time,exit_time,entry_open,exit_close,gross_return,long_reward_rupees,short_reward_rupees,reason")
        throw std::runtime_error("Unexpected CSV header; refusing to guess columns");
    std::map<Key,RewardRow> rows;
    while(std::getline(in,line)) {
        if(line.empty()) continue;
        std::stringstream ss(line);
        std::vector<std::string> cols;
        std::string col;
        while(std::getline(ss,col,',')) cols.push_back(col);
        if(cols.size()!=13) throw std::runtime_error("Malformed reward CSV row");
        RewardRow r;
        r.decision_time=std::stoll(cols[0]); r.agent=cols[1]; r.symbol=cols[2];
        r.horizon=std::stoull(cols[3]); r.available=(cols[4]=="1");
        r.long_reward=std::stod(cols[10]); r.short_reward=std::stod(cols[11]);
        if(!std::isfinite(r.long_reward)||!std::isfinite(r.short_reward))
            throw std::runtime_error("Non-finite reward");
        const Key key{r.decision_time,r.agent,r.symbol,r.horizon};
        if(!rows.emplace(key,r).second) throw std::runtime_error("Duplicate reward row");
    }
    return rows;
}

struct Item {
    std::int64_t time{};
    ds::DevAIObservation observation;
    double long_target{}, short_target{};
};

int main(int argc,char** argv) {
    if(argc!=7) {
        std::cerr<<"Usage: "<<argv[0]
          <<" <enriched.cache> <rewards.csv> <output_warmstart.model> <horizon_bars> <epochs> <train_fraction>\n"
          <<"Example: ... data/real_historical_outcomes.cache data/rewards.csv model/dev_warmstart.model 5 20 0.70\n";
        return 2;
    }
    try {
        const auto dataset=ds::DevHistoricalTrainingCache::load(argv[1]);
        ds::DevHistoricalTrainingDatasetValidator::validate(dataset);
        const auto rewards=loadRewards(argv[2]);
        const std::filesystem::path output=argv[3];
        const auto horizon=static_cast<std::size_t>(std::stoull(argv[4]));
        const auto epochs=std::stoull(argv[5]);
        const double train_fraction=std::stod(argv[6]);
        if(horizon==0 || epochs==0 || epochs>10000 ||
           !(train_fraction>=0.5 && train_fraction<=0.85))
            throw std::invalid_argument("Invalid horizon/epochs/train_fraction");
        std::vector<Item> items;
        std::size_t missing=0;
        for(const auto& rec:dataset.records) {
            for(const auto& c:rec.candidates) {
                const Key key{rec.decision_time,c.agent_name,c.symbol,horizon};
                auto it=rewards.find(key);
                if(it==rewards.end() || !it->second.available) { ++missing; continue; }
                bool verified=false;
                for(const auto& o:c.outcomes)
                    if(o.horizon_bars==horizon && o.available && o.numerical_integrity &&
                       o.outcome_time>rec.decision_time) verified=true;
                if(!verified) throw std::runtime_error("Reward row lacks valid cache outcome");
                // Normalize rewards by fixed starting capital to keep Q targets scaled.
                items.push_back({rec.decision_time,c.observation,
                                 it->second.long_reward/7000.0,
                                 it->second.short_reward/7000.0});
            }
        }
        if(missing) std::cout<<"Unavailable/missing candidate rewards: "<<missing<<"\n";
        std::sort(items.begin(),items.end(),
                  [](const Item&a,const Item&b){return a.time<b.time;});
        std::vector<std::int64_t> times;
        for(const auto& item:items)
            if(times.empty() || times.back()!=item.time) times.push_back(item.time);
        // Require enough distinct decision times for a meaningful chronological split.
        if(times.size()<100)
            throw std::runtime_error(
                "INSUFFICIENT HISTORY: need >=100 distinct decision timestamps. "
                "The 3-record smoke cache is NOT a training dataset.");
        const auto split_idx=static_cast<std::size_t>(times.size()*train_fraction);
        if(split_idx==0 || split_idx>=times.size())
            throw std::runtime_error("Empty chronological split");
        const std::int64_t split_time=times[split_idx];
        const std::int64_t purge_seconds=static_cast<std::int64_t>(horizon)*60;
        std::vector<ds::DevQTrainingSample> train;
        std::vector<Item> validation;
        for(const auto& item:items) {
            if(item.time+purge_seconds<=split_time) {
                // Independent counterfactual action targets. This is an OFFLINE
                // CONTEXTUAL-BANDIT WARM START, NOT DQN / RL training.
                train.push_back({item.observation,ds::DevAction::HOLD,0.0});
                train.push_back({item.observation,ds::DevAction::LONG,item.long_target});
                train.push_back({item.observation,ds::DevAction::SHORT,item.short_target});
            } else if(item.time>=split_time) validation.push_back(item);
        }
        if(train.size()<300 || validation.size()<30)
            throw std::runtime_error("Too few train/validation samples after horizon purge");
        ds::DevQNetwork network;
        ds::DevQTrainingConfig config;
        const std::size_t batch_size=32;
        for(std::size_t epoch=0;epoch<epochs;++epoch) {
            double loss=0;std::size_t steps=0;
            for(std::size_t i=0;i<train.size();i+=batch_size) {
                const auto end=std::min(train.size(),i+batch_size);
                std::vector<ds::DevQTrainingSample> batch(train.begin()+i,train.begin()+end);
                const auto result=network.trainBatch(batch,config);
                if(!result.numerical_integrity) throw std::runtime_error("Training numerical failure");
                loss+=result.mean_loss; ++steps;
            }
            std::cout<<"Epoch "<<epoch+1<<"/"<<epochs
                     <<" mean_training_loss="<<std::setprecision(9)<<loss/steps<<"\n";
        }
        double val_mse=0;std::size_t n=0;
        for(const auto& item:validation) {
            const auto q=network.predict(item.observation);
            if(!q.numerical_integrity) throw std::runtime_error("Invalid validation Q values");
            for(const auto [action,target] :
                {std::pair{ds::DevAction::HOLD,0.0},
                 std::pair{ds::DevAction::LONG,item.long_target},
                 std::pair{ds::DevAction::SHORT,item.short_target}}) {
                const double diff=q.value(action)-target;
                val_mse+=diff*diff; ++n;
            }
        }
        val_mse/=static_cast<double>(n);
        if(!std::isfinite(val_mse)) throw std::runtime_error("Invalid validation MSE");
        if(output.has_parent_path()) std::filesystem::create_directories(output.parent_path());
        const ds::DevModelPersistence persistence;
        const auto metadata=persistence.save(network,output);
        if(!metadata.numerical_integrity) throw std::runtime_error("Checkpoint integrity failure");
        ds::DevQNetwork verified_network;
        const auto loaded=persistence.load(verified_network,output);
        if(!loaded.numerical_integrity) throw std::runtime_error("Checkpoint reload failure");
        std::cout<<"3.19D-3B-4A CONTEXTUAL-BANDIT WARM START PASSED\n"
                 <<"Training Q samples: "<<train.size()
                 <<"\nValidation observations: "<<validation.size()
                 <<"\nChronological split time: "<<split_time
                 <<"\nPurged horizon seconds: "<<purge_seconds
                 <<"\nValidation MSE: "<<val_mse
                 <<"\nONE shared model checkpoint: "<<output<<"\n"
                 <<"WARNING: Not full RL; not a profitability or live-trading validation.\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"3.19D-3B-4A FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
