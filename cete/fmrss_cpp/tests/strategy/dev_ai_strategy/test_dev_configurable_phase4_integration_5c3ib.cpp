#include "devai/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai;

static std::vector<std::string> symbols(std::size_t n) {
    std::vector<std::string> s;
    for (std::size_t i = 0; i < n; ++i) s.push_back("STOCK_" + std::to_string(i));
    return s;
}
static bool rejects(const std::function<void()>& f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}
int main() {
    constexpr std::int64_t t = 1800000000; // divisible by 300
    std::size_t calls = 0;
    auto ranking = [&](DevStrategyKind, const std::vector<std::string>& universe,
                       std::int64_t when) {
        assert(when == t);
        std::vector<DevRankedStock> result;
        for (std::size_t i = 0; i < universe.size(); ++i)
            result.push_back({universe[i], static_cast<double>(i), t - 300});
        return result;
    };
    auto model = [&](const std::string& model_id, const std::string& manager,
                     const std::string& agent, const DevRankedStock& stock,
                     std::int64_t when) {
        assert(!model_id.empty() && !manager.empty() && !agent.empty() && when == t);
        ++calls;
        return DevModelAgentReport{manager, agent, stock.symbol, 0.7, 0.2, 0.1};
    };
    struct Case { const char* name; DevStrategyKind kind; std::size_t n, k; };
    const Case cases[] = {
        {"DevManager_001", DevStrategyKind::MathematicalStatistical, 10, 5},
        {"DevManager_002", DevStrategyKind::PriceAction, 100, 10},
        {"DevManager_003", DevStrategyKind::MathematicalStatistical, 50, 7}
    };
    for (const auto& x : cases) {
        DevConfigurablePhase4Integration adapter(
            {x.name, x.kind, symbols(x.n), x.k, "model-for-manager", 300}, ranking, model);
        const auto frame = adapter.evaluate(t);
        assert(frame.manager_id == x.name && frame.selected.size() == x.k);
        assert(frame.reports.size() == x.k);
        assert(frame.selected[0].symbol == "STOCK_" + std::to_string(x.n - 1));
        assert(frame.reports[0].agent_id == "DevAgent_001");
        assert(frame.reports[x.k - 1].agent_id ==
               "DevAgent_" + std::string(x.k < 10 ? "00" : "0") + std::to_string(x.k));
        assert(rejects([&] { (void)adapter.evaluate(t + 1); }));
    }
    assert(calls == 22);
    assert(rejects([&] {
        DevConfigurablePhase4Integration bad(
            {"M", DevStrategyKind::Custom, symbols(2), 3, "model", 300}, ranking, model);
    }));
    auto future = [&](DevStrategyKind, const std::vector<std::string>& universe, std::int64_t) {
        std::vector<DevRankedStock> out;
        for (const auto& s : universe) out.push_back({s, 1.0, t + 300});
        return out;
    };
    DevConfigurablePhase4Integration invalid(
        {"M", DevStrategyKind::Custom, symbols(2), 1, "model", 300}, future, model);
    assert(rejects([&] { (void)invalid.evaluate(t); }));
    std::cout << "PASS 5C-3I-B configurable multi-manager integration contract\n";
}
