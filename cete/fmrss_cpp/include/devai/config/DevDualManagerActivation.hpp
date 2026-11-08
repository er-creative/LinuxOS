#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace devai::config {
enum class Direction { LONG, SHORT };
enum class ExecutionMode { TESTING, LIVE };
enum class TestingType { BACKTEST, FORWARD, PAPER };
struct ManagerActivation {
    std::string id;
    bool enabled = false;
    ExecutionMode mode = ExecutionMode::TESTING;
    TestingType testing_type = TestingType::PAPER;
    Direction direction = Direction::LONG;
    double capital = 7000.0;
    double allocation_percent = 75.0;
    std::size_t top_k = 5;
    std::string universe_file;
    std::array<std::string, 5> agents{};
    // Planning state, not permission to submit broker orders.
    bool agents_active = false;
    bool execution_authorized = false;
};
struct DualManagerActivation {
    std::array<ManagerActivation, 2> managers;
    std::string shared_model;
    std::string technical_supervisor;
    std::string portfolio_supervisor;
    double portfolio_capital = 14000.0;
    bool live_short_execution = false;
};
DualManagerActivation loadDualManagerActivation(const std::string& path);
std::string modeName(ExecutionMode mode);
std::string testingTypeName(TestingType type);
std::string activationReport(const DualManagerActivation& config);
// Refuses new strategy decisions when disabled; never replaces risk/exit handling.
bool mayEvaluate(const DualManagerActivation& config, const std::string& manager_id);
} // namespace devai::config
