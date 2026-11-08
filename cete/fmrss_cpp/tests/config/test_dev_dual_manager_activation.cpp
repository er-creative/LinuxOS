#include "devai/config/DevDualManagerActivation.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: test_dev_dual_manager_activation config/devmanagers.ini\n"; return 2; }
    const auto cfg = devai::config::loadDualManagerActivation(argv[1]);
    assert(cfg.managers[0].enabled);
    assert(cfg.managers[0].mode == devai::config::ExecutionMode::TESTING);
    assert(cfg.managers[0].testing_type == devai::config::TestingType::PAPER);
    assert(cfg.managers[1].mode == devai::config::ExecutionMode::TESTING);
    assert(cfg.managers[1].testing_type == devai::config::TestingType::BACKTEST);
    assert(!cfg.managers[1].enabled);
    assert(devai::config::mayEvaluate(cfg,"DevManager_001"));
    assert(!devai::config::mayEvaluate(cfg,"DevManager_002"));
    assert(!devai::config::mayEvaluate(cfg,"DevManager_999"));
    assert(cfg.managers[0].agents[0] == "DevAgent_001");
    assert(cfg.managers[1].agents[4] == "DevAgent_010");
    assert(!cfg.managers[0].execution_authorized && !cfg.managers[1].execution_authorized);
    std::cout << devai::config::activationReport(cfg);
    std::cout << "PASS dual-manager activation configuration\n";
}
