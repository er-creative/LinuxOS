#include "devai/benchmark/DevRealHistoricalExecution_H47.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace devai::benchmark::h47;
using namespace devai::strategy;

// config/shares.txt contains ten tradable stocks AND the NIFTY benchmark.
// Return only tradable stocks; the benchmark is loaded separately below.
static std::vector<std::string> symbolsFromFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot read universe: " + path.string());
    }

    constexpr const char* benchmark = "NIFTY%2050";
    std::vector<std::string> stocks;
    std::set<std::string> seen;
    bool benchmark_found = false;
    std::string line;

    while (std::getline(in, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) line.resize(comment);

        const auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;
        const auto end = line.find_first_of(", \t\r", start);
        const std::string symbol = line.substr(
            start, end == std::string::npos ? std::string::npos : end - start);

        if (symbol == "SYMBOL" || symbol == "symbol") continue;
        if (!seen.insert(symbol).second) {
            throw std::runtime_error("Duplicate symbol: " + symbol);
        }
        if (symbol == benchmark) {
            benchmark_found = true;
        } else {
            stocks.push_back(symbol);
        }
    }

    if (!benchmark_found) {
        throw std::runtime_error("H4.7: NIFTY%2050 benchmark missing from shares.txt");
    }
    if (stocks.size() != 10) {
        throw std::runtime_error(
            "H4.7: Expected exactly 10 tradable stocks; found " +
            std::to_string(stocks.size()));
    }
    return stocks;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " /path/to/minute config/shares.txt\n";
        return 2;
    }

    try {
        const fs::path dir(argv[1]);
        const auto universe = symbolsFromFile(argv[2]);
        const std::string benchmark = "NIFTY%2050";

        std::vector<DevHistoricalMinuteInput> inputs;
        for (const auto& symbol : universe) {
            const auto path = dir / (symbol + "_1min.txt");
            if (!fs::is_regular_file(path)) {
                throw std::runtime_error("Missing: " + path.string());
            }
            inputs.push_back({symbol, path.string()});
        }

        const auto benchmark_path = dir / (benchmark + "_1min.txt");
        if (!fs::is_regular_file(benchmark_path)) {
            throw std::runtime_error(
                "Missing benchmark: " + benchmark_path.string());
        }
        inputs.push_back({benchmark, benchmark_path.string()});

        DevHistoricalFeedConfig cfg;
        cfg.manager_id = "DevManager_001";
        cfg.benchmark_symbol = benchmark;
        cfg.rolling_window = 20;
        cfg.minimum_history = 20;
        DevRealHistoricalMultiStockFeed feed(cfg);

        // DIAGNOSTIC ONLY: causal z-score ranking, NOT the established
        // production strategy and NOT a forward-return forecast.
        DevManager001StatisticalPolicy diagnostic_policy =
            [](const DevHistoricalStockFrame& stock,
               const DevHistoricalManagerFrame&) {
                DevManager001Assessment assessment;
                assessment.eligible =
                    stock.close_1m.ready &&
                    stock.close_5m.ready &&
                    stock.close_15m.ready;
                assessment.ranking_score = -stock.close_1m.zscore;
                assessment.reason = "DIAGNOSTIC_ONLY_NEGATIVE_ZSCORE";
                return assessment;
            };

        DevManager001HistoricalRanking manager(
            {"DevManager_001", 5}, diagnostic_policy);

        // DIAGNOSTIC ONLY: observed statistics, zero-padded to H2 width.
        // NOT the production feature encoder for the trained RL model.
        FeatureEncoder diagnostic_encoder =
            [](const ManagerFrame& frame, const RankingFrame& ranking) {
                std::array<double, devai::benchmark::kInputCount> features{};
                std::size_t idx = 0;
                for (const auto& candidate : ranking.selected) {
                    const auto& s = candidate.observations;
                    for (double value : {
                             s.close_1m.mean,
                             s.close_1m.standard_deviation,
                             s.close_1m.zscore,
                             s.close_5m.zscore,
                             s.close_15m.zscore,
                             candidate.ranking_score}) {
                        if (idx < features.size()) features[idx++] = value;
                    }
                }
                (void)frame;
                return features;
            };

        const auto result = DevRealHistoricalExecutionH47::run(
            feed, inputs, manager, diagnostic_encoder);

        std::cout << "CETE H4.7 STAGE 3 REAL HISTORICAL FEED RUN\n"
                  << "Feed validation: completed (inspect report for issues)\n"
                  << "Tradable stocks: " << universe.size() << '\n'
                  << "Benchmark: " << benchmark << '\n'
                  << "Feed eligible decisions: "
                  << result.feed_report.eligible_decisions << '\n'
                  << "Days observed: " << result.days.size() << '\n'
                  << "Incomplete days: " << result.incomplete_days << '\n'
                  << "Validated observations: " << result.total_observations << '\n'
                  << "Ranking: DIAGNOSTIC z-score only (NOT production strategy)\n"
                  << "Encoder: DIAGNOSTIC only (NOT trained-model features)\n"
                  << "Shared RL inference: NOT RUN\n"
                  << "D9F4C execution: NOT RUN\n";

        for (const auto& day : result.days) {
            if (day.batch.observations.empty()) continue;
            const auto& first = day.batch.observations.front();
            std::cout << "Day " << day.exchange_day
                      << " observations=" << day.batch.observations.size()
                      << " firstTop5=";
            for (const auto& symbol : first.ranked_top_five) {
                std::cout << symbol << ' ';
            }
            std::cout << "terminal="
                      << day.batch.observations.back().terminal << '\n';
        }
        return result.incomplete_days == 0 ? 0 : 3;
    } catch (const std::exception& e) {
        std::cerr << "CETE H4.7 STAGE 3 FAIL: " << e.what() << '\n';
        return 1;
    }
}
