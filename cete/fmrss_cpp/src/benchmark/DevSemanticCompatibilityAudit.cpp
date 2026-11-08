#include "devai/benchmark/DevSemanticCompatibilityAudit.hpp"
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace devai::benchmark {
namespace {
using namespace devai::strategy;

// This parser is intentionally read-only and accepts only the known V1 wire format.
// The H1.5 reader remains the production-facing structural loader.
DevSharedParameters readV1(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open frozen V1 checkpoint");
    std::string magic;
    if (!(in >> magic) || magic != "CETE_SHARED_DEVAI_V1")
        throw std::runtime_error("checkpoint is not CETE_SHARED_DEVAI_V1");
    DevSharedParameters p;
    const std::array<std::size_t, 4> expected{
        DEV_SHARED_HIDDEN * DEV_SHARED_INPUT,
        DEV_SHARED_HIDDEN,
        DEV_SHARED_OUTPUT * DEV_SHARED_HIDDEN,
        DEV_SHARED_OUTPUT
    };
    std::array<std::vector<double>*, 4> vectors{&p.w1, &p.b1, &p.w2, &p.b2};
    for (std::size_t k = 0; k < vectors.size(); ++k) {
        std::size_t n = 0;
        if (!(in >> n) || n != expected[k])
            throw std::runtime_error("V1 parameter vector dimension mismatch");
        auto& v = *vectors[k];
        v.resize(n);
        for (double& x : v)
            if (!(in >> x) || !std::isfinite(x))
                throw std::runtime_error("truncated or non-finite V1 parameter");
    }
    std::string extra;
    if (in >> extra) throw std::runtime_error("unexpected trailing checkpoint content");
    if (!in.eof()) throw std::runtime_error("checkpoint stream failure");
    return p;
}

bool equalFinite(const std::array<double, DEV_SHARED_OUTPUT>& a,
                 const std::array<double, DEV_SHARED_OUTPUT>& b) {
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!std::isfinite(a[i]) || !std::isfinite(b[i]) || a[i] != b[i]) return false;
    return true;
}
}

DevSemanticAuditResult DevSemanticCompatibilityAudit::run(const std::filesystem::path& checkpoint) {
    DevSemanticAuditResult result;
    try {
        const auto params = readV1(checkpoint);
        result.checkpoint_structure_valid = true;

        // Explicit current-runtime contract. This is NOT evidence about old V1 semantics.
        result.current_layout_valid =
            DEV_SHARED_INPUT == 148 && DEV_SHARED_HIDDEN == 64 &&
            DEV_SHARED_OUTPUT == 7 && DEV_Q_INPUT_SIZE == 84 &&
            DEV_MANAGER_INPUT_SIZE == 62 && DEV_Q_OUTPUT_SIZE == 4 &&
            DEV_MANAGER_ACTION_COUNT == 7 &&
            DEV_SHARED_INPUT == DEV_Q_INPUT_SIZE + DEV_MANAGER_INPUT_SIZE + 2;
        if (!result.current_layout_valid)
            throw std::runtime_error("current runtime layout differs from documented H1.6 contract");

        result.manager_action_mapping_valid =
            static_cast<std::size_t>(DevManagerAction::NO_TRADE) == 0 &&
            static_cast<std::size_t>(DevManagerAction::KEEP_CURRENT) == 1 &&
            static_cast<std::size_t>(DevManagerAction::SELECT_AGENT_001) == 2 &&
            static_cast<std::size_t>(DevManagerAction::SELECT_AGENT_002) == 3 &&
            static_cast<std::size_t>(DevManagerAction::SELECT_AGENT_003) == 4 &&
            static_cast<std::size_t>(DevManagerAction::SELECT_AGENT_004) == 5 &&
            static_cast<std::size_t>(DevManagerAction::SELECT_AGENT_005) == 6;
        if (!result.manager_action_mapping_valid)
            throw std::runtime_error("current manager action index contract changed");

        DevSharedNeuralNetwork a(73117U), b(73118U);
        a.setParameters(params);
        b.setParameters(params);
        DevSharedInput agent_role{}, manager_role{};
        for (std::size_t i = 0; i < DEV_Q_INPUT_SIZE; ++i)
            agent_role.values[i] = static_cast<double>(i + 1) / 1000.0;
        agent_role.values[DEV_Q_INPUT_SIZE + DEV_MANAGER_INPUT_SIZE] = 1.0;
        for (std::size_t i = 0; i < DEV_MANAGER_INPUT_SIZE; ++i)
            manager_role.values[DEV_Q_INPUT_SIZE + i] = static_cast<double>(i + 1) / 2000.0;
        manager_role.values[DEV_Q_INPUT_SIZE + DEV_MANAGER_INPUT_SIZE + 1] = 1.0;

        const auto agent_a = a.predict(agent_role);
        const auto agent_b = b.predict(agent_role);
        const auto manager_a = a.predict(manager_role);
        const auto manager_b = b.predict(manager_role);
        result.deterministic_inference = equalFinite(agent_a, agent_b) &&
                                         equalFinite(manager_a, manager_b);
        if (!result.deterministic_inference)
            throw std::runtime_error("non-deterministic inference under current V3 runtime");

        // Historical V1 source, an authoritative feature/action schema, or golden
        // V1 input/output vectors are required before this flag may ever be true.
        result.historical_semantics_certified = false;
        result.diagnostic = "CURRENT RUNTIME CONTRACT PASS; HISTORICAL V1/V3 SEMANTICS NOT CERTIFIED";
    } catch (const std::exception& e) {
        result.diagnostic = e.what();
    }
    return result;
}
}
