#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.hpp"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai::strategy;

int main(int argc, char** argv) {
    try {
        // TOP_K BENCHMARK_SYMBOL BENCHMARK_FILE STOCK_SYMBOL STOCK_FILE [STOCK_SYMBOL STOCK_FILE ...]
        if (argc < 6 || (argc - 4) % 2 != 0) {
            std::cerr << "Usage: " << argv[0]
                      << " TOP_K BENCHMARK_SYMBOL BENCHMARK_FILE STOCK_SYMBOL STOCK_FILE"
                         " [STOCK_SYMBOL STOCK_FILE ...]\n"
                      << "Historical validation only; past-only diagnostic is NOT a forecast or trade.\n";
            return 2;
        }
        const std::size_t top_k = std::stoul(argv[1]);
        const std::string benchmark_symbol = argv[2];
        if (top_k == 0) throw std::invalid_argument("TOP_K must be positive");
        std::vector<DevHistoricalMinuteInput> inputs{{benchmark_symbol, argv[3]}};
        for (int i = 4; i < argc; i += 2)
            inputs.push_back({argv[i], argv[i + 1]});

        DevHistoricalFeedConfig feed_config;
        feed_config.manager_id = "DevManager_001";
        feed_config.benchmark_symbol = benchmark_symbol;
        // Known NSE special session. Do not assume a regular session for this date.
        feed_config.sessions.session_overrides["2025-10-21"] = {13 * 60 + 45, 14 * 60 + 45};
        DevRealHistoricalMultiStockFeed feed(feed_config);

        DevD7StrategyConfig strategy_config;
        strategy_config.manager_id = feed_config.manager_id;
        strategy_config.min_expected_return = 0.0;
        DevManager001StatisticalStrategyD7 strategy(strategy_config, diagnosticPastOnlyForecast);
        // EXPLICIT 1-minute cadence; the test below also verifies the output.
        DevManager001HistoricalStrategyD7B manager({feed_config.manager_id, top_k, 1}, strategy);

        std::int64_t previous_key = 0;
        std::size_t callbacks = 0, consecutive = 0, non_five_minute = 0;
        std::size_t carried_five = 0, carried_fifteen = 0, selected_checked = 0;
        std::size_t displayed = 0;
        const auto report = manager.replay(feed, inputs, [&](const DevManager001RankingFrame& frame) {
            const auto key = frame.decision_key;
            if (frame.manager_id != feed_config.manager_id || key <= 0 ||
                (previous_key != 0 && key <= previous_key))
                throw std::runtime_error("Invalid manager identity or non-increasing decision key");
            if (previous_key != 0 && key == previous_key + 1) ++consecutive;
            if (key % 5 != 0) ++non_five_minute;
            previous_key = key;
            ++callbacks;
            if (frame.evaluated != inputs.size() - 1 ||
                frame.rejected > frame.evaluated || frame.selected.size() > top_k)
                throw std::runtime_error("Manager evaluated count/Top-K mismatch");
            for (const auto& c : frame.selected) {
                const auto& o = c.observations;
                if (o.one_minute.completed_key != key ||
                    o.five_minute.completed_key > key || o.fifteen_minute.completed_key > key ||
                    key - o.five_minute.completed_key >= 5 ||
                    key - o.fifteen_minute.completed_key >= 15 ||
                    (o.five_minute.completed_key - 1) / 1440 != (key - 1) / 1440 ||
                    (o.fifteen_minute.completed_key - 1) / 1440 != (key - 1) / 1440 ||
                    !o.close_1m.ready || !o.close_5m.ready || !o.close_15m.ready)
                    throw std::runtime_error("Noncausal/stale/unready selected observation: " + c.symbol);
                if (o.five_minute.completed_key < key) ++carried_five;
                if (o.fifteen_minute.completed_key < key) ++carried_fifteen;
                ++selected_checked;
            }
            if (displayed < 8) {
                std::cout << "decision_key=" << key
                          << " evaluated=" << frame.evaluated
                          << " rejected=" << frame.rejected
                          << " selected=" << frame.selected.size() << '\n';
                ++displayed;
            }
        });

        for (const auto& symbol : report.feed.validation.symbols)
            std::cout << symbol.symbol << " rows=" << symbol.rows
                      << " missing_session_minutes=" << symbol.missing_session_minutes << '\n';
        std::cout << "D4_pass=" << (report.feed.validation.passed ? "yes" : "no")
                  << " D5_eligible=" << report.feed.eligible_decisions
                  << " skipped_incomplete=" << report.feed.skipped_incomplete
                  << " skipped_warmup=" << report.feed.skipped_warmup
                  << " observed=" << report.observed_frames
                  << " decisions=" << report.decision_frames
                  << " callbacks=" << callbacks
                  << " consecutive_1m_pairs=" << consecutive
                  << " non_5m_boundaries=" << non_five_minute
                  << " carried_5m_selected=" << carried_five
                  << " carried_15m_selected=" << carried_fifteen
                  << " selected_checked=" << selected_checked
                  << " evaluated=" << report.evaluated
                  << " rejected=" << report.rejected
                  << " selected_assignments=" << report.selected << '\n';

        if (!report.feed.validation.passed || report.decision_frames == 0 ||
            callbacks != report.decision_frames ||
            report.observed_frames != report.decision_frames ||
            report.feed.eligible_decisions != report.decision_frames ||
            report.evaluated != report.decision_frames * (inputs.size() - 1) ||
            consecutive == 0 || non_five_minute == 0 ||
            selected_checked == 0 || carried_five == 0 || carried_fifteen == 0)
            throw std::runtime_error("D7C real-data 1-minute acceptance criteria not met");

        std::cout << "PASS 5C-3I-D7C real historical one-minute manager replay"
                     " (DIAGNOSTIC ONLY; NOT LIVE TRADING)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL D7C real historical replay: " << e.what() << '\n';
        return 1;
    }
}
