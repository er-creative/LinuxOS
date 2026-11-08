#include "devai/benchmark/DevHistoricalFeedRankingIntegration_H47.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace devai::benchmark::h47;
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class F> void mustReject(F&& f, const char* why) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, why);
}
constexpr std::int64_t day = 20000;
constexpr std::int64_t base = 100 * 1440;
const TimeConverter historicalTimeConverter = [](std::int64_t key) {
    return day * 86400 + (key % 1440) * 60 - 19800;
};
const FeatureEncoder encoder = [](const ManagerFrame&, const RankingFrame&) {
    return std::array<double, devai::benchmark::kInputCount>{}; // fixture ONLY
};
ManagerFrame frameAt(int minute) {
    ManagerFrame f;
    f.manager_id = "DevManager_001";
    f.decision_key = base + minute;
    for (const char* symbol : {"RELIANCE", "SBIN", "ITC", "NTPC", "HDFCBANK", "INFY"}) {
        devai::strategy::DevHistoricalStockFrame stock;
        stock.symbol = symbol;
        stock.one_minute.completed_key = f.decision_key;
        stock.one_minute.close = 100.0;
        stock.five_minute.completed_key = f.decision_key;
        stock.fifteen_minute.completed_key = f.decision_key;
        f.stocks.push_back(stock);
    }
    return f;
}
RankingFrame rankingAt(std::int64_t key) {
    RankingFrame r;
    r.manager_id = "DevManager_001";
    r.decision_key = key;
    for (const char* symbol : {"RELIANCE", "SBIN", "ITC", "NTPC", "HDFCBANK"}) {
        devai::strategy::DevManager001RankedCandidate c;
        c.symbol = symbol;
        c.ranking_score = 1.0;
        r.selected.push_back(c);
    }
    return r;
}
}
int main() {
    try {
        HistoricalFeedRankingBatch batch;
        auto f1 = frameAt(555);
        auto r1 = rankingAt(f1.decision_key);
        DevHistoricalFeedRankingIntegrationH47::append(batch, f1, r1, historicalTimeConverter, encoder);
        require(batch.observations.size() == 1, "first observation missing");
        require(batch.completed_close_prices.at(batch.observations[0].decision_time_utc).size() == 6,
                "non-top-five price missing");
        auto f2 = frameAt(915);
        auto r2 = rankingAt(f2.decision_key);
        DevHistoricalFeedRankingIntegrationH47::append(batch, f2, r2, historicalTimeConverter, encoder);
        require(batch.observations.back().terminal, "15:15 terminal missing");
        mustReject([&] { DevHistoricalFeedRankingIntegrationH47::append(batch, f1, r1, historicalTimeConverter, encoder); },
                   "out-of-order frame accepted");
        auto bad = f1;
        bad.stocks[0].one_minute.close = -1;
        HistoricalFeedRankingBatch empty;
        mustReject([&] { DevHistoricalFeedRankingIntegrationH47::append(empty, bad, r1, historicalTimeConverter, encoder); },
                   "negative price accepted");
        auto incomplete = r1;
        incomplete.selected.pop_back();
        HistoricalFeedRankingBatch skipped;
        DevHistoricalFeedRankingIntegrationH47::append(skipped, f1, incomplete, historicalTimeConverter, encoder);
        require(skipped.skipped_without_five == 1 && skipped.observations.empty(),
                "incomplete top five was not skipped");
        mustReject([&] { DevHistoricalFeedRankingIntegrationH47::append(skipped, f2, incomplete, historicalTimeConverter, encoder); },
                   "missing terminal top five accepted");
        std::cout << "CETE H4.7 STAGE 2 CONTRACT TEST PASS\n"
                  << "Five ranked slots: PASS (fixture)\n"
                  << "Completed price preservation: PASS (fixture)\n"
                  << "Chronology and terminal checks: PASS\n"
                  << "Invalid price rejection: PASS\n"
                  << "Real feed run: NOT EXECUTED\n"
                  << "Production ranking / feature encoder: NOT VERIFIED\n"
                  << "D9F4C execution: NOT EXECUTED\n";
    } catch (const std::exception& e) {
        std::cerr << "CETE H4.7 STAGE 2 FAIL: " << e.what() << '\n';
        return 1;
    }
}
