#include "devai/strategy/dev_ai_strategy/DevFrozenOOSPolicy.hpp"
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai::strategy;

int main(int argc, char** argv) {
    try {
        if (argc!=6) {
            std::cerr<<"Usage: "<<argv[0]
                <<" <OOS.cache> <agent.chk> <manager.chk> <minute-folder> <output.csv>\n";
            return 2;
        }
        const auto dataset=DevHistoricalTrainingCache::load(argv[1]);
        if (dataset.records.empty()) throw std::runtime_error("empty OOS cache");
        DevHistoricalTrainingDatasetValidator::validate(dataset);
        std::set<std::string> symbols;
        for (const auto& record:dataset.records)
            for (const auto& c:record.candidates) symbols.insert(c.symbol);
        if (symbols.empty()) throw std::runtime_error("OOS cache contains no candidates");
        DevHistoricalCloseStore prices;
        prices.loadMinuteFolder(argv[4],std::vector<std::string>(symbols.begin(),symbols.end()));
        DevFrozenOOSPolicyAdapter policy(argv[2],argv[3]);
        DevOOSLedgerConfig config{};
        config.initial_capital=7000.0;
        config.max_allocation_fraction=0.75;
        // Configure real broker/exchange charges BEFORE treating results as OOS.
        // The defaults here are deliberately not claimed as realistic costs.
        DevHistoricalOOSRunner runner(config);
        auto report=runner.run(dataset,
            [&](const std::string& symbol,std::int64_t t) {
                return prices.completedClose(symbol,t);
            },
            [&](const DevOOSDecisionPoint& point,const DevOOSLedgerState& state) {
                return policy(point,state);
            });
        DevHistoricalOOSRunner::writeCsv(argv[5],report);
        std::cout<<"Initial equity: "<<report.initial_equity
                 <<"\nFinal equity: "<<report.final_equity
                 <<"\nNet P&L: "<<report.net_pnl
                 <<"\nNet return: "<<report.net_return
                 <<"\nMax drawdown: "<<report.max_drawdown
                 <<"\nRotations: "<<report.rotations
                 <<"\nExecuted legs: "<<report.executed_legs
                 <<"\nNOTE: costs require calibration; historical close execution is optimistic.\n";
    } catch (const std::exception& e) {
        std::cerr<<"3.19D FAILED: "<<e.what()<<'\n';
        return 1;
    }
}
