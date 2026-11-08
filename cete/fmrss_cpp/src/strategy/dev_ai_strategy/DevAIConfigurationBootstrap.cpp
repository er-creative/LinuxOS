#include "devai/config/DevAIConfigurationBootstrap.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace devai::config {
DevAIConfigurationBootstrap::DevAIConfigurationBootstrap(
    devai::strategy::DevAISupervisor& supervisor,
    devai::DevManagerRegistry& registry,
    TrainTestApprove train_test_approve)
    : supervisor_(supervisor), registry_(registry),
      train_test_approve_(std::move(train_test_approve)) {
    if (!train_test_approve_) throw std::invalid_argument("training/validation callback required");
}
std::array<std::string, 5> DevAIConfigurationBootstrap::agentIds(const std::string& id) {
    return {id + "_Agent_001", id + "_Agent_002", id + "_Agent_003",
            id + "_Agent_004", id + "_Agent_005"};
}
void DevAIConfigurationBootstrap::apply(const DevAIRuntimeConfig& cfg,
                                         const std::vector<std::string>& default_symbols,
                                         double approved_capital) {
    if (!std::isfinite(approved_capital) || approved_capital <= 0.0)
        throw std::invalid_argument("approved capital must be positive and finite");
    if (cfg.managers.empty()) throw std::invalid_argument("no managers configured");
    struct Planned { DevManagerConfig config; std::vector<std::string> symbols; };
    std::vector<Planned> plan;
    std::set<std::string> ids, directories;
    double fraction_sum = 0.0;
    for (const auto& m : cfg.managers) {
        if (m.manager_id.size() != 14 || m.manager_id.rfind("DevManager_", 0) != 0 ||
            !std::all_of(m.manager_id.begin() + 11, m.manager_id.end(),
                [](unsigned char c) { return c >= '0' && c <= '9'; }) ||
            !ids.insert(m.manager_id).second)
            throw std::invalid_argument("invalid/duplicate manager ID: " + m.manager_id);
        if (m.agents_per_manager != 5)
            throw std::invalid_argument("current supervisor requires exactly 5 agents");
        if (m.trading_segment != "NSE_CASH")
            throw std::invalid_argument("only NSE_CASH supported");
        if (m.model_directory.empty() || !directories.insert(m.model_directory).second)
            throw std::invalid_argument("manager model directories must be unique and nonempty");
        if (!m.retrain_on_universe_change)
            throw std::invalid_argument("universe-change retraining cannot be disabled in this bootstrap");
        if (!std::isfinite(m.capital_allocation_fraction) ||
            m.capital_allocation_fraction < 0.0 || m.capital_allocation_fraction > 1.0)
            throw std::invalid_argument("capital fraction must be within [0,1]");
        if (m.enabled) {
            if (m.capital_allocation_fraction <= 0.0)
                throw std::invalid_argument("enabled manager requires positive allocation");
            fraction_sum += m.capital_allocation_fraction;
        } else if (m.capital_allocation_fraction != 0.0)
            throw std::invalid_argument("disabled manager must have zero allocation");
        const auto symbols = devai::DevManagerRegistry::validate_symbols(
            m.assigned_symbols.empty() ? default_symbols : m.assigned_symbols);
        if (m.enabled && symbols.empty())
            throw std::invalid_argument("enabled manager must have assigned symbols");
        plan.push_back({m, symbols});
    }
    if (fraction_sum > 1.0 + 1e-12)
        throw std::invalid_argument("manager allocations exceed total budget");

    // IMPORTANT: Existing DevAISupervisor has no capital-reallocation API.
    // An existing manager whose capital differs must be reconciled separately.
    for (const auto& p : plan) {
        const auto& m = p.config;
        bool exists = true;
        devai::strategy::DevManagerAccount account;
        try { account = supervisor_.account(m.manager_id); }
        catch (const std::out_of_range&) { exists = false; }
        if (!m.enabled) {
            if (exists) supervisor_.setActive(m.manager_id, false);
            continue; // Disabled managers are not created.
        }
        const auto agents = agentIds(m.manager_id);
        const double capital = approved_capital * m.capital_allocation_fraction;
        if (!exists) {
            const auto created = supervisor_.createManager(p.symbols, agents, capital);
            // Registry IDs are allocated sequentially, not specified by caller.
            if (created.manager_id != m.manager_id) {
                supervisor_.setActive(created.manager_id, false);
                throw std::runtime_error("manager ID mismatch; manual registry reconciliation needed");
            }
            supervisor_.setActive(m.manager_id, false); // fail closed during training
        } else {
            const auto& r = account.registration;
            if (r.manager_id != m.manager_id ||
                r.agents != std::vector<std::string>(agents.begin(), agents.end()) ||
                std::fabs(r.allocated_capital - capital) > 1e-7)
                throw std::runtime_error("existing manager registration/capital mismatch: " + m.manager_id);
        }
        const auto persisted = registry_.load_manager(m.manager_id);
        const bool changed = !exists || persisted.symbols != p.symbols;
        if (changed) {
            supervisor_.setActive(m.manager_id, false);
            supervisor_.assignShares(m.manager_id, p.symbols);
        }
        // Callback MUST train/test/publish if the universe changed, OR verify
        // the previously approved checkpoint on restart. No trading without it.
        // Fail closed even if callback throws.
        supervisor_.setActive(m.manager_id, false);
        if (!train_test_approve_(m.manager_id, p.symbols, m.model_directory))
            throw std::runtime_error("model not approved for universe; manager inactive: " + m.manager_id);
        supervisor_.setActive(m.manager_id, true);
    }
}
} // namespace devai::config
