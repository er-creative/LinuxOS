#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace devai::benchmark {
struct DevBenchmarkContract {
    std::string contract_version{"CETE_H1_V1"};
    std::vector<std::string> candidates{"candidate_0_sequential_td", "maskable_ppo", "qr_dqn", "discrete_cql"};
    std::vector<std::string> universe{}; // Explicitly supply exact 10 symbols from shares.txt
    std::string data_start_utc{}, data_end_utc{}; // Required: fixed evaluation window
    std::string feature_schema_id{}, feature_schema_sha256{}; // Required before benchmarking
    std::string split_manifest_sha256{}; // Required: chronological walk-forward splits
    std::string execution_model_id{}; // Required: completed-candle/fill rules
    std::string costs_model_id{}; // Required: taxes, brokerage, slippage
    double capital_per_manager{7000.0}, allocation_fraction{0.75};
    std::uint32_t seed{73117};
    bool allow_long{true}, allow_short{true}, live_orders{false};
    void validate() const;
    [[nodiscard]] std::string toJson() const;
    void write(const std::filesystem::path& path) const;
};
[[nodiscard]] std::vector<std::string> readUniverse(const std::filesystem::path& path);
}
