#pragma once
#include "devai/config/DevAIRuntimeConfig.hpp"
#include "devai/strategy/dev_ai_strategy/DevAISupervisor.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

namespace devai::report {
struct ManagerStatus {
    std::string manager_id;
    bool registered{false};
    bool active{false};
    std::size_t agent_count{0};
    std::vector<std::string> symbols;
    std::string model_directory;
};
struct ManagerStatusSnapshot {
    std::size_t configured_managers{0};
    std::size_t registered_managers{0};
    std::size_t active_managers{0};
    std::size_t active_agents{0};
    std::size_t active_symbol_assignments{0};
    std::vector<ManagerStatus> managers;
};
class DevAIManagerStatusReport final {
public:
    static ManagerStatusSnapshot collect(
        const devai::config::DevAIRuntimeConfig& config,
        const devai::DevManagerRegistry& registry,
        const devai::strategy::DevAISupervisor& supervisor);
    static void print(const ManagerStatusSnapshot& snapshot, std::ostream& out);
};
} // namespace devai::report
