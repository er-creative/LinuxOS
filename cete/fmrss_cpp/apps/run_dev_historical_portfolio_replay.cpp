#include "devai/strategy/dev_ai_strategy/DevHistoricalPortfolioReplay.hpp"
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main(int argc,char** argv) {
    if(argc!=5 && argc!=6) {
        std::cerr<<"Usage: "<<argv[0]<<" <historical.cache> <minute_data_folder> <audit.csv> <max_holding_bars> [shared_model_checkpoint]\n";
        return 2;
    }
    try {
        DevHistoricalPortfolioReplayConfig cfg;
        cfg.minute_folder=argv[2];cfg.audit_csv=argv[3];cfg.maximum_holding_bars=std::stoull(argv[4]);
        if(argc==6) cfg.model_checkpoint=argv[5];
        auto dataset=DevHistoricalTrainingCache::load(argv[1]);
        auto stats=DevHistoricalPortfolioReplay(cfg).run(dataset);
        std::cout<<"3.19D-3B-4C HISTORICAL PORTFOLIO REPLAY PASSED\n"
                 <<"Mode: "<<(argc==6?"frozen_shared_model":"hold_only_plumbing_test")<<"\n"
                 <<"Steps: "<<stats.steps<<"\nOpened: "<<stats.opened
                 <<"\nExplicit exits: "<<stats.exited<<"\nRejected: "<<stats.rejected
                 <<"\nHOLD: "<<stats.hold_steps<<"\nEnding equity: "<<stats.ending_equity
                 <<"\nTotal normalized reward: "<<stats.total_reward<<"\nAudit CSV: "<<cfg.audit_csv<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<"3.19D-3B-4C FAILED: "<<e.what()<<'\n';return 1;}
}
