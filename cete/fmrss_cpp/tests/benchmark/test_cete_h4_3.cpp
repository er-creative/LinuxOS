#include "devai/benchmark/DevHistoricalDecisionRunner.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::benchmark;
void require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
int main(){try{
 H43Config cfg;cfg.universe={"RELIANCE","HDFCBANK","ICICIBANK","SBIN","BHARTIARTL","AXISBANK","HCLTECH","ITC","NTPC","ADANIPORTS"};
 DevHistoricalDecisionRunner runner(cfg);
 // 2026-10-30 09:20 and 15:15 IST represented as epoch seconds, UTC conversion.
 auto epoch=[](int istminute){return 1793318400LL+(istminute-330)*60;}; // arbitrary UTC day base
 std::vector<H43Candle> candles;
 for(int minute:{560,915})for(const auto& symbol:cfg.universe)
  candles.push_back({symbol,epoch(minute)-60,epoch(minute),100,101,99,100,1000});
 auto ranker=[&](H43Side,std::int64_t,const std::vector<H43Candle>& batch){
  require(batch.size()==10,"incomplete cross-section");
  std::vector<H43Candidate> top;
  for(int i=0;i<5;++i){H43Candidate c;c.symbol=cfg.universe[i];c.score=5-i;top.push_back(c);}
  return top;
 };
 auto policy=[](H43Side,std::int64_t,const H43Candidate& c,bool held){
  return held?H43Action::Hold:(c.score==5?H43Action::Open:H43Action::Wait);
 };
 auto rows=runner.run(candles,ranker,policy);
 require(rows.size()==4,"expected 2 opens and 2 forced exits");
 require(rows[0].action==H43Action::Open && rows[1].action==H43Action::Open,
  "missing opens");
 require(rows[2].action==H43Action::Exit && rows[3].action==H43Action::Exit,
  "missing forced exits");
 bool rejected=false;
 auto missing=candles;missing.pop_back();
 try{runner.run(missing,ranker,policy);}catch(const std::runtime_error&){rejected=true;}
 require(rejected,"incomplete cross-section accepted");
 rejected=false;auto invalid=candles;invalid[0].end_epoch+=120;
 try{runner.run(invalid,ranker,policy);}catch(const std::runtime_error&){rejected=true;}
 require(rejected,"invalid candle accepted");
 std::cout<<"CETE H4.3 HISTORICAL DECISION RUNNER PASS\n"
  <<"Completed-candle and universe guards: PASS\n"
  <<"Top-five ranking and manager isolation: PASS\n"
  <<"Forced 15:15 exits: PASS\n"
  <<"Incomplete/invalid data rejection: PASS\n"
  <<"Real CETE feature/model adapters: NOT CONNECTED\n"
  <<"Actual historical OOS performance: NOT CERTIFIED\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
