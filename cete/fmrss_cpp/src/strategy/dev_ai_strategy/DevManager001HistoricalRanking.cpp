#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalRanking.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
DevManager001HistoricalRanking::DevManager001HistoricalRanking(
    DevManager001RankingConfig config, DevManager001StatisticalPolicy strategy_policy)
    : config_(std::move(config)), policy_(std::move(strategy_policy)) {
    if (config_.manager_id.empty() || config_.top_k == 0 || !policy_)
        throw std::invalid_argument("D6 requires manager_id, top_k and an explicit strategy policy");
}
DevManager001RankingFrame DevManager001HistoricalRanking::evaluate(
    const DevHistoricalManagerFrame& frame) const {
    if (frame.manager_id != config_.manager_id || frame.decision_key <= 0)
        throw std::invalid_argument("D6 manager identity or decision boundary mismatch");
    DevManager001RankingFrame out;
    out.manager_id = frame.manager_id;
    out.decision_key = frame.decision_key;
    std::set<std::string> seen;
    for (const auto& stock : frame.stocks) {
        if (stock.symbol.empty() || !seen.insert(stock.symbol).second)
            throw std::invalid_argument("D6 duplicate/empty stock symbol");
        // A decision is triggered by a completed 1m candle. Higher timeframes
        // may be carried forward, but only if completed, fresh and causal.
        const auto key = frame.decision_key;
        const auto five_key = stock.five_minute.completed_key;
        const auto fifteen_key = stock.fifteen_minute.completed_key;
        const bool causal_and_fresh =
            stock.one_minute.completed_key == key &&
            five_key > 0 && five_key <= key && key - five_key < 5 &&
            fifteen_key > 0 && fifteen_key <= key && key - fifteen_key < 15 &&
            // Exchange-local day boundary: no previous-session carry-forward.
            (five_key - 1) / 1440 == (key - 1) / 1440 &&
            (fifteen_key - 1) / 1440 == (key - 1) / 1440;
        if (!causal_and_fresh ||
            !stock.close_1m.ready ||
            !stock.close_5m.ready ||
            !stock.close_15m.ready)
        {
            std::cerr
                << "\n[D6 VALIDATION FAILED]\n"
                << "Symbol: " << stock.symbol << '\n'
                << "Decision key: " << key << '\n'
                << "1m completed key: "
                << stock.one_minute.completed_key << '\n'
                << "5m completed key: " << five_key << '\n'
                << "15m completed key: " << fifteen_key << '\n'
                << "5m age: " << key - five_key << '\n'
                << "15m age: " << key - fifteen_key << '\n'
                << "1m ready: " << stock.close_1m.ready << '\n'
                << "5m ready: " << stock.close_5m.ready << '\n'
                << "15m ready: " << stock.close_15m.ready << '\n'
                << "Causal and fresh: " << causal_and_fresh
                << std::endl;

            throw std::invalid_argument(
                "D6 requires complete, causal, ready stock observations"
            );
        }
        ++out.evaluated;
        const auto assessment = policy_(stock, frame);
        if (!assessment.eligible) { ++out.rejected; continue; }
        if (!std::isfinite(assessment.ranking_score))
            throw std::invalid_argument("D6 strategy returned nonfinite ranking score");
        out.selected.push_back({stock.symbol, assessment.ranking_score, stock});
    }
    std::sort(out.selected.begin(), out.selected.end(),
        [](const DevManager001RankedCandidate& a, const DevManager001RankedCandidate& b) {
            if (a.ranking_score != b.ranking_score) return a.ranking_score > b.ranking_score;
            return a.symbol < b.symbol;
        });
    if (out.selected.size() > config_.top_k) out.selected.resize(config_.top_k);
    return out;
}
DevManager001ReplayReport replayDevManager001Ranking(
    const DevRealHistoricalMultiStockFeed& feed,
    const std::vector<DevHistoricalMinuteInput>& inputs,
    const DevManager001HistoricalRanking& manager,
    const DevManager001RankingConsumer& consumer) {
    if (!consumer) throw std::invalid_argument("D6 ranking consumer required");
    DevManager001ReplayReport report;
    report.feed = feed.replay(inputs, [&](const DevHistoricalManagerFrame& frame) {
        const auto ranked = manager.evaluate(frame);
        ++report.manager_frames;
        if (!ranked.selected.empty()) ++report.frames_with_candidates;
        report.candidate_assignments += ranked.selected.size();
        consumer(ranked);
    });
    return report;
}
} // namespace devai::strategy
