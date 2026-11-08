#include "devai/benchmark/DevActualReplayOrchestrator.hpp"
#include <iostream>
#include <stdexcept>
using namespace devai::benchmark::h44;
static void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
class FakeBridge final: public IHistoricalCETEBridge {
public:
    int replays=0;
    bool malformed=false;
    std::vector<Candle> completedCandles(std::int64_t t) override {
        std::vector<Candle> out;
        for(int i=0;i<10;++i)
            out.push_back({"STOCK"+std::to_string(i),t,100,101,99,100,1000,true});
        if(malformed) out[0].complete=false;
        return out;
    }
    std::vector<Candidate> rankTopFive(Direction,
        const std::vector<Candle>& cs,std::int64_t) override {
        std::vector<Candidate> out;
        for(int i=0;i<5;++i){Candidate c;c.symbol=cs[i].symbol;c.score=5-i;out.push_back(c);}
        return out;
    }
    Decision modelDecision(Direction d,std::int64_t t,
        const std::vector<Candidate>& cs) override {
        Decision x;x.direction=d;x.close_time_utc=t;x.action=Action::Wait;
        for(int i=0;i<5;++i)x.ranked_symbols[i]=cs[i].symbol;
        return x;
    }
    void executeHistoricalReplay(Direction,
        const std::vector<Decision>& ds) override {
        require(ds.size()==2,"wrong decision count");++replays;
    }
};
int main(){
 try {
    FakeBridge b;DevActualReplayOrchestrator r(b);
    RunOptions o;
    for(int i=0;i<10;++i)o.universe.push_back("STOCK"+std::to_string(i));
    o.completed_candle_times={100,160};
    auto result=r.run(o);
    require(result.size()==1 && b.replays==1 && result[0].execution_replay_used,
            "LONG integration contract failed");
    b.malformed=true;
    bool rejected=false;
    try{(void)r.run(o);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"incomplete candle accepted");
    b.malformed=false;o.enable_short=true;
    result=r.run(o);
    require(result.size()==2 && b.replays==3,"dual direction isolation failed");
    std::cout<<"CETE H4.4 INTEGRATION SEAM TEST PASS\n"
             <<"Completed-candle guards: PASS\n"
             <<"Top-five and decision contracts: PASS\n"
             <<"LONG/SHORT callback separation: PASS (fake bridge only)\n"
             <<"Actual CETE bridge: NOT IMPLEMENTED\n"
             <<"Real historical model inference: NOT VERIFIED\n"
             <<"Real D9F.4C SHORT replay: NOT SUPPORTED\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
