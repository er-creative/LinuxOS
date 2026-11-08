#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace devai::config {
struct DevManagerConfig {
    std::string manager_id;
    bool enabled{false};
    // Empty: use default_symbols supplied by the application at startup.
    // No fixed universe-size limit; symbols are the source of truth.
    std::vector<std::string> assigned_symbols;
    std::size_t agents_per_manager{5}; // Current DevAISupervisor API uses array<...,5>.
    // Private to this manager. Never reused for another manager.
    std::string model_directory;
    bool retrain_on_universe_change{true};
    double capital_allocation_fraction{0.0};
    std::string trading_segment{"NSE_CASH"};
};
struct DevAIRuntimeConfig {
    std::vector<DevManagerConfig> managers{
        {"DevManager_001", true, {}, 5, "models/DevManager_001", true, 1.0, "NSE_CASH"},
        {"DevManager_002", false, {}, 5, "models/DevManager_002", true, 0.0, "NSE_CASH"}
    };
};
} // namespace devai::config
