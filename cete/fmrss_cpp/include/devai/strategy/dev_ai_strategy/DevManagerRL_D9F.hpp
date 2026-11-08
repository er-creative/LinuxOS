#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace devai::strategy::d9f {
enum class Action : std::size_t { WAIT=0, OPEN=1, HOLD=2, EXIT=3, SWITCH=4 };
struct State {
    bool invested=false;
    double candidate_score=0.0; // causal ranking score, not probability
    double held_score=0.0;
    double advantage=0.0;
    int position_age=0;
    int minutes_to_close=0;
    int entries_remaining=0;
    bool candidate_available=false;
    bool forecast_mode=false;
};
struct Config {double alpha=.05,gamma=.95,epsilon=.10;std::uint64_t seed=17;};
class SharedManagerQ final {
public:
    explicit SharedManagerQ(Config config={});
    Action choose(const State&,const std::vector<Action>& legal,bool explore);
    void learn(const State&,Action,double reward,const State&,const std::vector<Action>& next_legal,bool terminal);
    std::size_t states() const noexcept {return q_.size();}
    std::string stateKey(const State&) const;
    double qValue(const State&,Action) const;
private:
    using Values=std::array<double,5>;
    Config cfg_;
    std::unordered_map<std::string,Values> q_;
    std::uint64_t rng_;
    double random01();
};
}
