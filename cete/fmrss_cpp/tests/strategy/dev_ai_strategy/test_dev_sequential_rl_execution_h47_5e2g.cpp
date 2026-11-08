#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevDirectionalTraining_F3EF2C5B.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace devai::strategy;
namespace dir=devai::strategy::f3ef2c5b;
void check(bool value,const char* msg) {if(!value)throw std::runtime_error(msg);}
int main() {
    using namespace std::chrono;
    const auto day=sys_days{year{2026}/10/16};
    const std::int64_t t=duration_cast<seconds>(
        day.time_since_epoch()+hours{10}+minutes{30}-minutes{330}).count();
    const auto path=std::filesystem::temp_directory_path()/"cete_h47_5e2g_price_fixture_1min.txt";
    struct Cleanup {std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove(p,ec);}} cleanup{path};
    {
        std::ofstream out(path);
        check(bool(out),"cannot write fixture");
        out<<"Symbol,Date,Time,Open,High,Low,Close,Volume\n";
        out<<"AAA,2026-10-16,10:28:00,98,100,97,99,1000\n";
        out<<"AAA,2026-10-16,10:29:00,100,102,99,101,1000\n";
        out<<"AAA,2026-10-16,10:30:00,103,105,102,104,1000\n";
        out<<"AAA,2026-10-16,10:31:00,106,108,105,107,1000\n";
    }
    DevHistoricalCloseStore store;
    store.loadMinuteFile(path,"AAA");
    check(store.completedClose("AAA",t)==101,"10:30 decision must see 10:29 close");
    check(store.openingPrice("AAA",t)==103,"10:30 execution must use 10:30 open");
    check(store.completedClose("AAA",t+60)==104,"10:31 decision must see 10:30 close");
    check(store.openingPrice("AAA",t+60)==106,"10:31 execution must use 10:31 open");
    bool missing=false;
    try{(void)store.completedClose("AAA",t+180);}catch(const std::runtime_error&){missing=true;}
    check(missing,"missing exact completed candle must fail closed");
    check(dir::mayEnter(t),"10:30 must permit entries");
    check(!dir::mayEnter(t+4*3600+15*60),"14:45 must block entries");
    DevOOSLedgerConfig cfg{};
    cfg.brokerage_rate=.0003;cfg.slippage_rate=.0005;cfg.other_cost_rate=.0003;
    DevRealHistoricalMultiStockOOS ledger(cfg);
    const DevOOSPriceProvider fill=[&](const std::string& sym,std::int64_t at){
        if(sym=="BBB")return 50.0;
        return store.openingPrice(sym,at);
    };
    const auto opened=ledger.execute(t,"AAA",fill);
    check(opened.executed_legs==1 && ledger.state().held_symbol=="AAA","OPEN failed");
    const auto switched=ledger.execute(t+60,"BBB",fill);
    check(switched.executed_legs==2 && ledger.state().held_symbol=="BBB","SWITCH failed");
    check(switched.brokerage>0 && switched.slippage>0 && switched.other_costs>0,"switch costs missing");
    check(std::abs(switched.net_pnl-(switched.gross_pnl-switched.brokerage-switched.slippage-switched.other_costs))<1e-8,"switch net PnL mismatch");
    const auto exit=ledger.execute(t+120,"",fill);
    check(exit.executed_legs==1 && ledger.state().quantity==0,"EXIT failed");
    std::cout<<"H4.7 Stage 5E.2G price causality and ledger rotation: PASSED (fixtures)\n";
}
