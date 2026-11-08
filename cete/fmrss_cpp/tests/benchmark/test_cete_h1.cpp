#include "devai/benchmark/DevCandidateModelRegistry.hpp"
#include "devai/benchmark/DevBenchmarkContract.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
int main() {
    using namespace devai::benchmark;
    const auto root=fs::temp_directory_path()/"cete_h1_native_test";
    fs::remove_all(root);fs::create_directories(root);
    try {
        auto source=root/"sequential_rl.chk";
        {std::ofstream f(source,std::ios::binary);f<<"fixture-not-a-real-model";}
        auto frozen=DevCandidateModelRegistry::freezeCandidate0(source,root/"frozen");
        assert(frozen.sha256.size()==64);
        DevCandidateModelRegistry::verifyCandidate0(root/"frozen");
        bool rejected=false;
        try {(void)DevCandidateModelRegistry::freezeCandidate0(source,root/"frozen");}catch(const std::exception&){rejected=true;}
        assert(rejected);
        {std::ofstream f(frozen.checkpoint,std::ios::app|std::ios::binary);f<<"tamper";}
        rejected=false;
        try {DevCandidateModelRegistry::verifyCandidate0(root/"frozen");}catch(const std::exception&){rejected=true;}
        assert(rejected);
        DevBenchmarkContract contract;
        contract.universe={"A","B","C","D","E","F","G","H","I","J"};
        rejected=false;
        try{contract.validate();}catch(const std::exception&){rejected=true;}
        assert(rejected); // must not silently invent missing evaluation settings
        contract.data_start_utc="2024-01-01T00:00:00Z";
        contract.data_end_utc="2025-01-01T00:00:00Z";
        contract.feature_schema_id="test";contract.feature_schema_sha256="test_hash";
        contract.split_manifest_sha256="test_split";contract.execution_model_id="test_fill";
        contract.costs_model_id="test_cost";
        contract.validate();contract.write(root/"contract.json");
        assert(fs::is_regular_file(root/"contract.json"));
        fs::remove_all(root);
        std::cout<<"H1 native C++ tests PASS (fixture-only; real model load not tested)\n";
        return 0;
    }catch(...){fs::remove_all(root);throw;}
}
