#include "devai/benchmark/DevCandidateModelRegistry.hpp"
#include "devai/benchmark/DevBenchmarkContract.hpp"
#include <exception>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    try {
        using namespace devai::benchmark;
        if(argc==4&&std::string(argv[1])=="freeze") {
            const auto f=DevCandidateModelRegistry::freezeCandidate0(argv[2],argv[3]);
            std::cout<<"Frozen Candidate 0: "<<f.checkpoint<<"\nSHA256: "<<f.sha256<<"\n";
        } else if(argc==3&&std::string(argv[1])=="verify") {
            DevCandidateModelRegistry::verifyCandidate0(argv[2]);
            std::cout<<"Candidate 0 file integrity PASS (not model-load certification)\n";
        } else if(argc==3&&std::string(argv[1])=="universe") {
            auto u=readUniverse(argv[2]);std::cout<<"Universe symbols: "<<u.size()<<"\n";
            for(const auto& s:u)std::cout<<s<<'\n';
            if(u.size()!=10)throw std::runtime_error("expected ten stocks");
        } else {
            std::cerr<<"Usage:\n  "<<argv[0]<<" freeze <actual_sequential_checkpoint> <new_output_directory>\n"
                     <<"  "<<argv[0]<<" verify <output_directory>\n"
                     <<"  "<<argv[0]<<" universe <shares.txt>\n";
            return 2;
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<"H1 ERROR: "<<e.what()<<'\n';return 1;}
}
