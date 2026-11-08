#include "devai/strategy/dev_ai_strategy/diagnostics/DevRankingSourceAudit_D9F4CF.hpp"
#include <iostream>
#include <exception>
int main(int argc,char** argv){
    if(argc<2||argc>4){std::cerr<<"Usage: "<<argv[0]<<" <ten_stock_source_trace.csv> [decision_audit.csv] [expected_universe_size=10]\n";return 2;}
    try{const auto report=devai::strategy::diagnostics::d9f4cf::inspect(argv[1],argc>=3?argv[2]:"",argc>=4?static_cast<std::size_t>(std::stoul(argv[3])):10);
        std::cout<<devai::strategy::diagnostics::d9f4cf::summary(report)<<'\n';for(const auto& x:report.examples)std::cout<<"DETAIL: "<<x<<'\n';
        if(report.score_ties==report.decisions&&report.decisions)std::cout<<"WARNING: Every decision has identical scores across its universe.\n";
        if(report.static_score_decisions+1==report.decisions&&report.decisions>1)std::cout<<"WARNING: Scores never changed between decisions.\n";
        if(report.missing_audit_decisions)std::cout<<"WARNING: Some replay decisions have no matching source trace.\n";
        return report.integrityPass()?0:1;
    }catch(const std::exception& e){std::cerr<<"D9F4CF_ERROR: "<<e.what()<<'\n';return 2;}
}
