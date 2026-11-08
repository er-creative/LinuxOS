#include "devai/benchmark/DevWalkForwardBenchmark.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"Usage: cete_h4 <oos_decisions.csv> <report.csv>\n";return 2;}
 try{
    auto rows=devai::benchmark::readWalkForwardCSV(argv[1]);
    auto reports=devai::benchmark::evaluateWalkForward(rows,{});
    devai::benchmark::writeWalkForwardCSV(argv[2],reports);
    std::cout<<"CETE H4 evaluation complete: "<<reports.size()<<" candidate/manager reports\n";
    for(const auto& r:reports)std::cout<<r.candidate<<' '<<r.manager<<" net="<<r.net_pnl
                                  <<" drawdown="<<r.max_drawdown_pct<<"%\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"CETE H4 ERROR: "<<e.what()<<'\n';return 1;}
}
