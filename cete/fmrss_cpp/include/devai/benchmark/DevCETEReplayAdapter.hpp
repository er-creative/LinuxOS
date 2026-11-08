#pragma once
#include "devai/strategy/dev_ai_strategy/reward/DevRealLedgerReplay_D9F4C.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace devai::benchmark {
struct CETEReplayRow {
 std::string candidate, manager, symbol;
 std::int64_t timestamp{};
 std::size_t action{}, legs{};
 double equity_before{}, equity_after{}, gross{}, brokerage{}, slippage{}, other_costs{}, net{}, drawdown{};
 bool terminal{};
};
struct CETEReplaySummary {
 std::size_t decisions{}, executed_legs{};
 double initial_equity{}, final_equity{}, gross{}, costs{}, net{}, max_drawdown_pct{};
};
class DevCETEReplayAdapter final {
public:
 static std::vector<CETEReplayRow> convert(const devai::strategy::d9f4c::ReplayResult& replay,
                                          const std::string& candidate,
                                          const std::string& manager = "DevManager_001");
 static CETEReplaySummary summarize(const std::vector<CETEReplayRow>& rows);
 static void writeCsv(const std::filesystem::path&, const std::vector<CETEReplayRow>&);
};
}
