#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace devai::benchmark::h44 {
// Explicit seam for existing CETE runtime. Implement IHistoricalCETEBridge against
// the project's real feature, ranking, shared-network and D9F.4C APIs.
constexpr std::size_t kUniverse = 10;
constexpr std::size_t kTopK = 5;
constexpr std::size_t kFeatureCount = 148;

struct Candle {
    std::string symbol;
    std::int64_t close_time_utc{}; // timestamp of completed candle, seconds
    double open{}, high{}, low{}, close{}, volume{};
    bool complete{false};
};
struct Candidate {
    std::string symbol;
    double score{};
    std::array<double,kFeatureCount> observation{};
};
enum class Direction { Long, Short };
enum class Action { Wait, Open, Hold, Exit };
struct Decision {
    std::int64_t close_time_utc{};
    Direction direction{Direction::Long};
    Action action{Action::Wait};
    std::string symbol;
    double price{};
    std::array<std::string,kTopK> ranked_symbols{};
};
struct Evaluation {
    Direction direction{Direction::Long};
    std::vector<Decision> decisions;
    bool model_predictions_used{false};
    bool execution_replay_used{false};
};

class IHistoricalCETEBridge {
public:
    virtual ~IHistoricalCETEBridge() = default;
    // All data/features must be constructed using information available at t.
    virtual std::vector<Candle> completedCandles(std::int64_t t) = 0;
    virtual std::vector<Candidate> rankTopFive(Direction d,
        const std::vector<Candle>& candles, std::int64_t t) = 0;
    // Agent assessments + final manager authority must be implemented here.
    // Must use the same shared model weights for both manager directions.
    virtual Decision modelDecision(Direction d, std::int64_t t,
        const std::vector<Candidate>& topFive) = 0;
    // Pass decisions to the existing D9F.4C replay engine. It may reject SHORT.
    virtual void executeHistoricalReplay(Direction d,
        const std::vector<Decision>& decisions) = 0;
};

struct RunOptions {
    std::vector<std::string> universe;
    std::vector<std::int64_t> completed_candle_times;
    bool enable_short{false}; // OFF until D9F.4C SHORT is certified
};
class DevActualReplayOrchestrator {
public:
    explicit DevActualReplayOrchestrator(IHistoricalCETEBridge& bridge)
        : bridge_(bridge) {}
    std::vector<Evaluation> run(const RunOptions& options);
private:
    IHistoricalCETEBridge& bridge_;
};
}
