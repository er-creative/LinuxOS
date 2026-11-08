#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <random>
#include <vector>

namespace devai::benchmark {
inline constexpr std::size_t PPO_INPUT = 148;
inline constexpr std::size_t PPO_ACTIONS = 7;
inline constexpr std::size_t PPO_HIDDEN = 32;
using PPOState = std::array<double, PPO_INPUT>;
using PPOMask = std::array<bool, PPO_ACTIONS>;

struct PPOConfig {
    double gamma{0.99}, gae_lambda{0.95}, clip_epsilon{0.2};
    double policy_lr{0.0003}, value_lr{0.0005}, entropy_coef{0.01};
    std::size_t epochs{4};
    std::uint32_t seed{73117};
};
struct PPOChoice {
    std::size_t action{};
    double log_probability{};
    double value{};
};
struct PPOStep {
    PPOState state{};
    PPOMask legal{};
    std::size_t action{};
    double old_log_probability{};
    double old_value{};
    double reward{};
    bool terminal{};
    double advantage{};
    double target_value{};
};
class DevMaskablePPO {
public:
    explicit DevMaskablePPO(PPOConfig config = {});
    PPOChoice choose(const PPOState&, const PPOMask&, bool deterministic);
    [[nodiscard]] std::array<double, PPO_ACTIONS> policyLogits(const PPOState&) const;
    double value(const PPOState&) const;
    void computeAdvantages(std::vector<PPOStep>&, double bootstrap_value) const;
    void update(const std::vector<PPOStep>&);
    void save(const std::filesystem::path&) const;
    void load(const std::filesystem::path&);
private:
    PPOConfig config_;
    std::mt19937 rng_;
    // One shared trunk, separate policy and value heads.
    std::array<double,PPO_HIDDEN*PPO_INPUT> trunk_{};
    std::array<double,PPO_HIDDEN> trunk_bias_{};
    std::array<double,PPO_ACTIONS*PPO_HIDDEN> policy_{};
    std::array<double,PPO_ACTIONS> policy_bias_{};
    std::array<double,PPO_HIDDEN> value_head_{};
    double value_bias_{};
    struct Forward {
        std::array<double,PPO_HIDDEN> hidden{};
        std::array<double,PPO_ACTIONS> logits{};
        double value{};
    };
    Forward forward(const PPOState&) const;
    static std::array<double,PPO_ACTIONS> probabilities(
        const std::array<double,PPO_ACTIONS>&, const PPOMask&);
    static void checkState(const PPOState&);
    static void checkMask(const PPOMask&);
};
}
