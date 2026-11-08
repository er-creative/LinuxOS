#include "devai/strategy/dev_ai_strategy/DevRealHistoricalTrainingDriver.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>
int main(int argc,char** argv) {
    if (argc<4 || argc>6) {
        std::cerr << "Usage: " << argv[0]
                  << " <real_historical.cache> <minute_folder> <shared_model.chk> [epochs] [learning_rate]\n";
        return 2;
    }
    try {
        devai::strategy::DevRealHistoricalTrainingConfig c{};
        c.cache_file=argv[1];c.minute_folder=argv[2];c.checkpoint_file=argv[3];
        if(argc>4)c.epochs=std::stoul(argv[4]);
        if(argc>5)c.learning_rate=std::stod(argv[5]);
        const auto r=devai::strategy::DevRealHistoricalTrainingDriver(c).run();
        std::cout<<"CETE 3.19D-3B: preliminary shared-model training completed\n"
                 <<"Records: "<<r.records<<"  Train decisions: "<<r.train_records
                 <<"  Agent samples: "<<r.agent_samples
                 <<"  Manager samples: "<<r.manager_samples
                 <<"\nMean train loss: "<<r.training_loss
                 <<"  Holdout label loss: "<<r.validation_loss
                 <<"\nBehavior-ledger last equity: "<<r.behavior_final_equity
                 <<"\nShared checkpoint: "<<c.checkpoint_file<<'\n';
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"3.19D-3B FAILED: "<<e.what()<<'\n';return 1;
    }
}
