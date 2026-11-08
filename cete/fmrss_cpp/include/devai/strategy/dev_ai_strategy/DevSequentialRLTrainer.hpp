#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
namespace devai::strategy {
struct DevSequentialRLConfig {
 std::filesystem::path cache, validation_cache, minute_folder, checkpoint;
 std::size_t epochs{8}, replay_capacity{4096}, batch_size{16}, warmup{32}, target_sync{100};
 double learning_rate{0.0001}, gamma{0.99}, epsilon_start{0.30}, epsilon_end{0.05};
 double initial_capital{7000.0};
 double brokerage_rate{0.0}, slippage_rate{0.0}, other_cost_rate{0.0};
 std::uint32_t seed{73117};
 bool allow_intraday_short{false};
 std::set<std::string> shortable_symbols{};
};
struct DevSequentialRLResult {
 std::size_t train_decisions{}, manager_updates{}, agent_updates{}, replay_transitions{};
 double mean_td_loss{}, last_training_equity{}, last_short_training_equity{}, validation_td_loss{};
 std::size_t long_entries{}, short_entries{}, forced_exits{};
};
class DevSequentialRLTrainer {
public:
 explicit DevSequentialRLTrainer(DevSequentialRLConfig cfg);
 [[nodiscard]] DevSequentialRLResult run() const;
private:
 DevSequentialRLConfig cfg_;
};
}
