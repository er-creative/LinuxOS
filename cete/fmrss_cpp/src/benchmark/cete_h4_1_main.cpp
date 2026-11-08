#include "devai/benchmark/DevReplayBridge.hpp"
#include <iostream>
#include <exception>
int main(int argc,char** argv){if(argc!=3){std::cerr<<"Usage: cete_h4_1 <decisions.csv> <trades.csv>\n";return 2;}try{auto rows=devai::benchmark::DevReplayBridge::readCsv(argv[1]);auto out=devai::benchmark::DevReplayBridge::evaluate(rows);devai::benchmark::DevReplayBridge::writeCsv(argv[2],out);for(const auto& r:out.reports)std::cout<<r.candidate<<' '<<r.manager<<" trades="<<r.trades<<" net="<<r.net<<" drawdown="<<r.max_drawdown_pct<<"%\n";}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
