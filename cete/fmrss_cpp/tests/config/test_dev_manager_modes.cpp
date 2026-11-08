#include "devai/config/DevDualManagerActivation.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>

static void replaceFirst(std::string& s, const std::string& from, const std::string& to) {
    const auto pos=s.find(from);
    if(pos==std::string::npos) throw std::runtime_error("Missing fixture text: "+from);
    s.replace(pos,from.size(),to);
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream in(argv[1]); if(!in) return 3;
    const std::string base((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    const auto temp=std::filesystem::temp_directory_path()/"cete_modes_fixture.ini";
    const auto check=[&](std::string content,bool expect_valid,devai::config::ExecutionMode expected_mode) {
        {std::ofstream out(temp);out<<content;}
        bool accepted=false;
        try {
            const auto cfg=devai::config::loadDualManagerActivation(temp.string());
            accepted=true;
            assert(cfg.managers[0].mode==expected_mode);
            assert(!cfg.managers[0].execution_authorized && !cfg.managers[1].execution_authorized);
        } catch(const std::runtime_error&) {}
        assert(accepted==expect_valid);
    };
    check(base,true,devai::config::ExecutionMode::TESTING);
    auto live=base;replaceFirst(live,"mode = TESTING","mode = LIVE");
    check(live,true,devai::config::ExecutionMode::LIVE);
    auto forward=base;replaceFirst(forward,"testing_type = PAPER","testing_type = FORWARD");
    check(forward,true,devai::config::ExecutionMode::TESTING);
    auto bad=base;replaceFirst(bad,"mode = TESTING","mode = UNSAFE");
    check(bad,false,devai::config::ExecutionMode::TESTING);
    auto backtest = base;
    replaceFirst(backtest, "testing_type = PAPER", "testing_type = BACKTEST");
    check(backtest, true, devai::config::ExecutionMode::TESTING);
    std::filesystem::remove(temp);
    std::cout
        << "PASS CETE manager modes: "
        << "TESTING/BACKTEST, "
        << "TESTING/FORWARD, "
        << "TESTING/PAPER, "
        << "LIVE, "
        << "invalid mode rejected, "
        << "real broker orders blocked\n";
}
