#pragma once
#include "devai/benchmark/DevRLModelInterface.hpp"
#include "devai/cache/HistoricalMathematicalFeatureCache.hpp"
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace devai::strategy {

// The actual production encoder must be supplied. It MUST match the
// selected model's 148-dimensional training feature contract exactly.
using DevHistoricalFeatureEncoder_H47_5E2D = std::function<
    std::array<double, benchmark::kInputCount>(
        const statistics::MathematicalStateFeatures&)>;
// Legal masks are portfolio-context dependent and must not be guessed here.
using DevHistoricalAgentMask_H47_5E2D = std::function<
    std::array<bool, benchmark::kOutputCount>(
        const std::string&, std::int64_t, std::size_t)>;

class DevCachedHistoricalRLRequest_H47_5E2D final {
public:
    using State = statistics::MathematicalStateFeatures;
    using States = cache::HistoricalMathematicalFeatureCache::States;

    DevCachedHistoricalRLRequest_H47_5E2D(
        States states,
        std::string symbol,
        std::string benchmark_symbol,
        DevHistoricalFeatureEncoder_H47_5E2D encoder,
        DevHistoricalAgentMask_H47_5E2D mask)
        : states_(std::move(states)), symbol_(std::move(symbol)),
          benchmark_(std::move(benchmark_symbol)), encoder_(std::move(encoder)),
          mask_(std::move(mask)) {
        if(symbol_.empty() || benchmark_.empty() || symbol_==benchmark_ ||
           !encoder_ || !mask_)
            throw std::invalid_argument("5E2D: invalid symbol/encoder/mask");
    }

    [[nodiscard]] benchmark::DevRLRequest operator()(
        const std::string& symbol, std::int64_t t, std::size_t slot) const {
        if(symbol!=symbol_ || t<=0 || t%60 || slot>=5)
            throw std::invalid_argument("5E2D: request identity mismatch");
        const auto it=states_.find(t);
        if(it==states_.end())
            throw std::runtime_error("5E2D: missing historical cache state");
        const auto& s=it->second;
        if(s.stock_symbol!=symbol_ || s.benchmark_symbol!=benchmark_ ||
           s.decision_time!=t || !s.numerical_integrity || !s.fully_ready ||
           !s.one_minute_ready || !s.five_minute_ready || !s.fifteen_minute_ready)
            throw std::runtime_error("5E2D: stale/incomplete historical state");
        benchmark::DevRLRequest request{};
        request.role=benchmark::DevRLRole::Agent;
        request.features=encoder_(s);
        for(double v:request.features)
            if(!std::isfinite(v))
                throw std::runtime_error("5E2D: nonfinite encoded feature");
        request.legal_actions=mask_(symbol,t,slot);
        bool legal=false;
        for(std::size_t i=0;i<benchmark::kAgentActionCount;++i)
            legal=legal || request.legal_actions[i];
        for(std::size_t i=benchmark::kAgentActionCount;i<benchmark::kOutputCount;++i)
            if(request.legal_actions[i])
                throw std::runtime_error("5E2D: manager-only action in agent mask");
        if(!legal) throw std::runtime_error("5E2D: empty legal action mask");
        return request;
    }

    // Load the REAL cache using its existing manifest, source fingerprint,
    // and exact requested timestamps. Never bypass fingerprint validation.
    [[nodiscard]] static States loadVerified(
        const cache::HistoricalMathematicalFeatureCache& cache,
        const std::string& symbol,
        const std::string& benchmark,
        std::int64_t end_time,
        std::uint64_t fingerprint,
        const std::vector<std::int64_t>& requested) {
        if(requested.empty()) throw std::invalid_argument("5E2D: empty request window");
        States out;
        if(!cache.load(symbol,benchmark,end_time,fingerprint,requested,out))
            throw std::runtime_error("5E2D: cache miss/stale fingerprint; run 5B replay");
        return out;
    }
private:
    States states_;
    std::string symbol_,benchmark_;
    DevHistoricalFeatureEncoder_H47_5E2D encoder_;
    DevHistoricalAgentMask_H47_5E2D mask_;
};

// Multi-symbol provider for the existing 5E2B five-agent bridge.
// Every symbol must be loaded independently from its verified cache.
class DevHistoricalMultiSymbolRLRequests_H47_5E2D final {
public:
    void add(std::string symbol, DevCachedHistoricalRLRequest_H47_5E2D provider) {
        if(!providers_.emplace(std::move(symbol),std::move(provider)).second)
            throw std::invalid_argument("5E2D: duplicate symbol");
    }
    [[nodiscard]] benchmark::DevRLRequest operator()(
        const std::string& symbol, std::int64_t t, std::size_t slot) const {
        const auto it=providers_.find(symbol);
        if(it==providers_.end()) throw std::runtime_error("5E2D: uncached symbol");
        return it->second(symbol,t,slot);
    }
private:
    std::map<std::string,DevCachedHistoricalRLRequest_H47_5E2D> providers_;
};
} // namespace devai::strategy
