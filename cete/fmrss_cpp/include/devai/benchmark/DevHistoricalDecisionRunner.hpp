#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace devai::benchmark {
constexpr std::size_t H43_STOCKS=10, H43_TOPK=5, H43_FEATURES=148;
enum class H43Side { Long, Short };
enum class H43Action { Wait, Open, Hold, Exit };
struct H43Candle {
 std::string symbol; std::int64_t start_epoch{}, end_epoch{};
 double open{},high{},low{},close{},volume{};
};
struct H43Candidate {
 std::string symbol; double score{};
 std::array<double,H43_FEATURES> observation{};
};
struct H43Decision {
 std::int64_t timestamp{}; H43Side side{}; H43Action action{};
 std::string symbol; double observed_price{}, score{};
 std::array<std::string,H43_TOPK> top5{};
};
// Caller supplies CETE feature/ranking and model inference adapters.
// Every candidate observation must contain ONLY information available at decision time.
using H43Ranker=std::function<std::vector<H43Candidate>(
    H43Side,std::int64_t,const std::vector<H43Candle>&)>;
using H43Policy=std::function<H43Action(
    H43Side,std::int64_t,const H43Candidate&,bool has_position)>;
struct H43Config {
 std::vector<std::string> universe;
 int start_minute_ist{9*60+20}; int last_entry_minute_ist{15*60};
 int flat_minute_ist{15*60+15};
};
class DevHistoricalDecisionRunner {
public:
 explicit DevHistoricalDecisionRunner(H43Config config);
 std::vector<H43Decision> run(const std::vector<H43Candle>& completed_candles,
                              const H43Ranker& ranker,const H43Policy& policy) const;
 static void writeCsv(const std::string& filename,const std::vector<H43Decision>&);
private: H43Config config_;
};
}
