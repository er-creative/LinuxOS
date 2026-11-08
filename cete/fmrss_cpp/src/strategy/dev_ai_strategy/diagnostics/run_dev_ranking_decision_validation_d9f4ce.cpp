#include "devai/strategy/dev_ai_strategy/diagnostics/DevRankingDecisionValidator_D9F4CE.hpp"
#include <exception>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2) {std::cerr<<"Usage: "<<argv[0]<<" <decision_audit.csv>\n";return 2;}
    try {
        auto s=devai::strategy::d9f4ce::validate(devai::strategy::d9f4ce::readCsv(argv[1]));
        std::cout<<devai::strategy::d9f4ce::report(s);
        return (s.illegal_final || s.raw_not_greedy || s.entry_cutoff_violations || s.deadline_violations)?1:0;
    } catch(const std::exception& e) {std::cerr<<"D9F4CE_ERROR: "<<e.what()<<'\n';return 2;}
}
