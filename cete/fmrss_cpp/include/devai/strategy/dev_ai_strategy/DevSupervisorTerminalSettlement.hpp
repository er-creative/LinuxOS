#pragma once
#include "devai/strategy/dev_ai_strategy/DevinderjeetSupervisor.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace devai::strategy {
// At the terminal decision boundary, use only already-completed candle CLOSES.
// The caller supplies last completed candles, never the terminal boundary's OPEN.
struct DevCompletedTerminalMark {
    std::int64_t candle_open_time{};
    double close{};
};
struct DevSupervisorSettlementResult {
    std::vector<DevOrderReport> reports;
    DevSupervisorSnapshot after;
};
class DevSupervisorTerminalSettlement {
public:
    static DevSupervisorSettlementResult settle(
        DevinderjeetSupervisor& supervisor,
        const std::vector<std::string>& manager_ids,
        std::int64_t terminal_boundary,
        const std::map<std::string, DevCompletedTerminalMark>& completed_marks);
};
}
