#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalInferenceAudit_H47_5E2E.hpp"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace devai::strategy {

// Integrates existing Stage 5E.2B -> 5E.2C -> 5E.2E components.
// Real model/checkpoint, 148-feature encoder, cache, masks and rankings MUST
// be supplied by the caller. This code does not synthesize any of them.
struct DevHistoricalSharedRLRunConfig_H47_5E2F {
    std::int64_t begin_epoch{}, end_epoch{}, flat_epoch{};
    std::filesystem::path agent_csv;
    DevHistoricalInferenceProvenance_H47_5E2E provenance;
};

class DevHistoricalSharedRLExecutable_H47_5E2F final {
public:
    DevHistoricalSharedRLExecutable_H47_5E2F(
        std::shared_ptr<const benchmark::IDevRLModel> selected_model,
        DevRLRequestProvider_H47_5E2B verified_request_provider,
        DevHistoricalRankingProvider_H47_5E2C verified_ranking_provider)
        : selected_model_(std::move(selected_model)),
          request_provider_(std::move(verified_request_provider)),
          ranking_provider_(std::move(verified_ranking_provider)) {
        if(!selected_model_ || !request_provider_ || !ranking_provider_)
            throw std::invalid_argument("5E2F: real model, requests and rankings required");
    }

    [[nodiscard]] DevHistoricalInferenceAuditSummary_H47_5E2E run(
        const DevHistoricalSharedRLRunConfig_H47_5E2F& cfg) const {
        if(!cfg.provenance.complete())
            throw std::invalid_argument("5E2F: unverified real-data/model provenance");
        if(cfg.agent_csv.empty() || cfg.begin_epoch<=0 || cfg.end_epoch<=cfg.begin_epoch ||
           cfg.flat_epoch<=cfg.begin_epoch || cfg.flat_epoch>=cfg.end_epoch ||
           cfg.begin_epoch%60 || cfg.end_epoch%60 || cfg.flat_epoch%60)
            throw std::invalid_argument("5E2F: invalid output/session configuration");
        // A single selected model object is used by all five agents.
        const DevSelectedRLInference_H47_5E2B inference(
            selected_model_,request_provider_);
        const DevHistoricalSharedRLReplay_H47_5E2C replay(
            inference,ranking_provider_);
        const auto rows=replay.run(cfg.begin_epoch,cfg.end_epoch,cfg.flat_epoch);
        const auto summary=DevHistoricalInferenceAudit_H47_5E2E::validate(
            rows,cfg.flat_epoch,cfg.provenance);
        // Audit before writing: no misleading partial CSV on invalid replay.
        DevHistoricalInferenceAudit_H47_5E2E::writeAgentCsv(
            cfg.agent_csv.string(),rows,cfg.flat_epoch,cfg.provenance);
        return summary;
    }
private:
    std::shared_ptr<const benchmark::IDevRLModel> selected_model_;
    DevRLRequestProvider_H47_5E2B request_provider_;
    DevHistoricalRankingProvider_H47_5E2C ranking_provider_;
};
} // namespace devai::strategy
