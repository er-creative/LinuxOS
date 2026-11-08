#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalSharedRLReplay_H47_5E2C.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace devai::strategy {

// Validates historical inference outputs, NOT trade execution.
// Refuses to label a test-only model run as validated production inference.
struct DevHistoricalInferenceProvenance_H47_5E2E {
    std::string checkpoint_id;
    std::string encoder_id;
    std::string cache_fingerprint;
    std::string ranking_source_id;
    bool checkpoint_validated{false};
    bool production_encoder_verified{false};
    bool real_cache_verified{false};
    bool real_ranking_verified{false};
    [[nodiscard]] bool complete() const noexcept {
        return !checkpoint_id.empty() && !encoder_id.empty() &&
            !cache_fingerprint.empty() && !ranking_source_id.empty() &&
            checkpoint_validated && production_encoder_verified &&
            real_cache_verified && real_ranking_verified;
    }
};
struct DevHistoricalInferenceAuditSummary_H47_5E2E {
    std::size_t minutes{}, inferred_minutes{}, agent_reports{},
        blocked_minutes{}, flat_minutes{};
    std::int64_t first_minute{}, last_minute{};
};

// Deliberately separate from DevManager trading authorization.
// Inference scores cannot be treated as prices, profits, or fills.
class DevHistoricalInferenceAudit_H47_5E2E final {
public:
    [[nodiscard]] static DevHistoricalInferenceAuditSummary_H47_5E2E validate(
        const std::vector<DevHistoricalInferenceRow_H47_5E2C>& rows,
        std::int64_t flat_boundary,
        const DevHistoricalInferenceProvenance_H47_5E2E& provenance) {
        if (!provenance.complete())
            throw std::invalid_argument("5E2E: unverified checkpoint/encoder/cache/ranking");
        if (rows.empty() || flat_boundary<=0 || flat_boundary%60)
            throw std::invalid_argument("5E2E: empty replay/invalid cutoff");
        DevHistoricalInferenceAuditSummary_H47_5E2E result{};
        result.first_minute=rows.front().decision_time;
        result.last_minute=rows.back().decision_time;
        result.minutes=rows.size();
        for(std::size_t n=0;n<rows.size();++n) {
            const auto& row=rows[n];
            if(row.decision_time<=0 || row.decision_time%60 ||
               (n && row.decision_time!=rows[n-1].decision_time+60))
                throw std::runtime_error("5E2E: missing/duplicate/out-of-order minute");
            if(row.decision_time>=flat_boundary) {
                if(row.status!=DevHistoricalCycleStatus_H47_5E2C::MandatoryFlatBoundary ||
                   row.has_inference)
                    throw std::runtime_error("5E2E: inference after mandatory flat");
                ++result.flat_minutes;
                continue;
            }
            if(!row.has_inference) {
                if(row.status!=DevHistoricalCycleStatus_H47_5E2C::NoFreshRanking)
                    throw std::runtime_error("5E2E: unexplained blocked cycle");
                ++result.blocked_minutes;
                continue;
            }
            if(row.status!=DevHistoricalCycleStatus_H47_5E2C::InferenceComplete ||
               !row.inference.complete ||
               row.inference.decision_time!=row.decision_time ||
               row.inference.ranking_time!=row.ranking_time ||
               row.ranking_time<=0 || row.ranking_time%900 ||
               row.ranking_time>row.decision_time ||
               row.decision_time-row.ranking_time>=900)
                throw std::runtime_error("5E2E: stale/incomplete inference");
            std::unordered_set<std::string> symbols;
            for(std::size_t i=0;i<DEV_AGENT_SLOT_COUNT;++i) {
                const auto& report=row.inference.reports[i];
                if(report.agent_name!=DevTopCandidateSelector::agentNames()[i] ||
                   report.symbol.empty() ||
                   !symbols.insert(report.symbol).second ||
                   report.decision_time!=row.decision_time ||
                   report.candidate_rank!=i+1 ||
                   !report.decision.valid ||
                   report.decision.selected_action>=benchmark::kAgentActionCount)
                    throw std::runtime_error("5E2E: malformed agent report");
                for(double value:report.decision.action_values)
                    if(!std::isfinite(value))
                        throw std::runtime_error("5E2E: nonfinite model output");
                ++result.agent_reports;
            }
            ++result.inferred_minutes;
        }
        return result;
    }

    static void writeAgentCsv(
        const std::string& filename,
        const std::vector<DevHistoricalInferenceRow_H47_5E2C>& rows,
        std::int64_t flat_boundary,
        const DevHistoricalInferenceProvenance_H47_5E2E& provenance) {
        (void)validate(rows,flat_boundary,provenance);
        std::ofstream out(filename,std::ios::trunc);
        if(!out) throw std::runtime_error("5E2E: cannot open ledger");
        out<<"decision_epoch,ranking_epoch,agent_slot,agent_name,symbol,model_candidate,"
              "selected_action,action_0,action_1,action_2,action_3,action_4,action_5,action_6\n";
        out<<std::setprecision(17);
        for(const auto& row:rows) {
            if(!row.has_inference)continue;
            for(std::size_t i=0;i<DEV_AGENT_SLOT_COUNT;++i) {
                const auto& r=row.inference.reports[i];
                out<<row.decision_time<<','<<row.ranking_time<<','<<i+1<<','
                   <<r.agent_name<<','<<r.symbol<<','
                   <<static_cast<int>(row.inference.candidate)<<','
                   <<r.decision.selected_action;
                for(double v:r.decision.action_values)out<<','<<v;
                out<<'\n';
            }
        }
        if(!out)throw std::runtime_error("5E2E: ledger write failed");
    }
};
} // namespace devai::strategy
