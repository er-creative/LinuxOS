#include "devai/benchmark/DevHistoricalObservationAdapter_H47.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace devai::benchmark::h47;

namespace {
void check(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}

template<class F> void expectInvalid(F&& f, const char* why) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, why);
}

// This fixture intentionally tests the adapter only. It is NOT a production converter.
constexpr std::int64_t testDecisionKey = 100 * 1440 + 555;
constexpr std::int64_t testUtc = 1700000000; // overridden by local-time-consistent fixture below

ManagerFrame makeFrame() {
    ManagerFrame frame{};
    frame.manager_id = "DevManager_001";
    frame.decision_key = testDecisionKey;
    for (const char* name : {"RELIANCE", "SBIN", "ITC", "NTPC", "HDFCBANK"}) {
        devai::strategy::DevHistoricalStockFrame stock{};
        stock.symbol = name;
        stock.one_minute.completed_key = testDecisionKey;
        stock.five_minute.completed_key = testDecisionKey;
        stock.fifteen_minute.completed_key = testDecisionKey;
        frame.stocks.push_back(stock);
    }
    return frame;
}

RankingFrame makeRanking() {
    RankingFrame ranking{};
    ranking.manager_id = "DevManager_001";
    ranking.decision_key = testDecisionKey;
    for (const char* name : {"RELIANCE", "SBIN", "ITC", "NTPC", "HDFCBANK"}) {
        devai::strategy::DevManager001RankedCandidate c{};
        c.symbol = name;
        c.ranking_score = 1.0;
        ranking.selected.push_back(c);
    }
    return ranking;
}

const TimeConverter converter = [](std::int64_t key) {
    // Test-only converter: produce a valid UTC epoch whose IST minute matches key.
    // 20000 days after Unix epoch, at key's IST minute of day.
    const auto minute = key % 1440;
    return 20000LL * 86400 + minute * 60 - 19800;
};
const FeatureEncoder encoder = [](const ManagerFrame&, const RankingFrame&) {
    return std::array<double, devai::benchmark::kInputCount>{};
};
}

int main() {
    try {
        auto frame = makeFrame();
        auto ranking = makeRanking();
        const auto obs = DevHistoricalObservationAdapterH47::convert(
            frame, ranking, converter, encoder);
        check(obs.ranked_top_five[4] == "HDFCBANK", "fifth candidate lost");
        check(obs.completed_candles && obs.causal_features, "observation flags missing");
        check(!obs.terminal, "09:15 should not be terminal");

        auto mismatch = ranking;
        ++mismatch.decision_key;
        expectInvalid([&] { (void)DevHistoricalObservationAdapterH47::convert(
            frame, mismatch, converter, encoder); }, "mismatched timestamp accepted");

        auto duplicate = ranking;
        duplicate.selected[4].symbol = duplicate.selected[0].symbol;
        expectInvalid([&] { (void)DevHistoricalObservationAdapterH47::convert(
            frame, duplicate, converter, encoder); }, "duplicate candidate accepted");

        auto future = frame;
        future.stocks[0].one_minute.completed_key = testDecisionKey + 1;
        expectInvalid([&] { (void)DevHistoricalObservationAdapterH47::convert(
            future, ranking, converter, encoder); }, "future candle accepted");

        auto badClock = [](std::int64_t) { return 20000LL * 86400; };
        expectInvalid([&] { (void)DevHistoricalObservationAdapterH47::convert(
            frame, ranking, badClock, encoder); }, "incorrect timezone conversion accepted");

        std::cout << "CETE H4.7 OBSERVATION ADAPTER TEST PASS\n"
                  << "Five ranked candidates: PASS (fixture)\n"
                  << "Timestamp mismatch: REJECTED\n"
                  << "Duplicate candidate: REJECTED\n"
                  << "Future candle: REJECTED\n"
                  << "Incorrect UTC conversion: REJECTED\n"
                  << "Real historical feed / encoder: NOT CONNECTED\n"
                  << "Actual D9F4C replay: NOT EXECUTED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "H4.7 FAIL: " << e.what() << '\n';
        return 1;
    }
}
