#pragma once
#include <string>
#include <vector>
#include <filesystem>
namespace devai::benchmark {
struct ReplayEvent {std::string candidate,manager,symbol,date,time,side,action; double price{}; bool legal{},completed{},past_only{};};
struct ReplayTrade {std::string candidate,manager,symbol,side,entry_time,exit_time;double quantity{},gross{},fees{},net{};};
struct ReplayReport {std::string candidate,manager;double equity{},net{},max_drawdown_pct{};unsigned trades{};};
struct ReplayOutput {std::vector<ReplayTrade> trades;std::vector<ReplayReport> reports;};
class DevReplayBridge {
public:
 static std::vector<ReplayEvent> readCsv(const std::filesystem::path&);
 static ReplayOutput evaluate(const std::vector<ReplayEvent>&);
 static void writeCsv(const std::filesystem::path&,const ReplayOutput&);
};
}
