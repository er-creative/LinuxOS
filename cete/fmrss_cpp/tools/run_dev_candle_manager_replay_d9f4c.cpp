#include "devai/strategy/dev_ai_strategy/reward/DevCandleManagerReplay_D9F4C.hpp"
#include <exception>
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    if (argc != 5 && argc != 6) {
        std::cerr << "Usage: run_dev_candle_manager_replay_d9f4c CACHE MINUTE_FOLDER SHARED_CHECKPOINT OUTPUT.csv [gross|net]\n";
        return 2;
    }
    try {
        devai::strategy::d9f4c::CandleReplayConfig cfg{};
        cfg.cache_file=argv[1]; cfg.minute_folder=argv[2]; cfg.shared_checkpoint=argv[3];
        if (argc==6) {
            const std::string mode=argv[5];
            if (mode=="gross") cfg.mode=devai::strategy::d9f4c::LedgerRewardMode::GROSS_MARKET_REWARD;
            else if (mode!="net") throw std::runtime_error("mode must be gross or net");
        }
        const auto result=devai::strategy::d9f4c::DevCandleManagerReplay::run(cfg);
        devai::strategy::d9f4c::DevCandleManagerReplay::writeCsv(argv[4],result);
        std::cout << "D9F4C_CANDLE_REPLAY_PASS transitions=" << result.steps.size()
                  << " executed_legs=" << result.executed_legs
                  << " final_equity=" << result.final_equity
                  << " model_trained=0 checkpoint_written=0\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "D9F4C_CANDLE_REPLAY_FAIL: " << e.what() << '\n';
        return 1;
    }
}
