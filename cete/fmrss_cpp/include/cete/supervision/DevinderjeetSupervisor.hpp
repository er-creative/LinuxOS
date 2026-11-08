#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace cete::supervision {
// Cash-equity supervisor: LONG entries and EXIT only. SELL means exit an owned long.
enum class DevSupervisedAction { BUY, SELL, EXIT, HOLD };
enum class DevOrderStatus { FILLED, REJECTED, HELD };
struct DevSupervisorConfig {
    double initial_capital{7000.0};
    double global_max_exposure_fraction{0.75};
    double transaction_cost_bps{5.0};
    double slippage_bps{2.0};
    std::size_t max_open_positions{1};
};
struct DevManagerRegistration {
    std::string manager_id;
    std::vector<std::string> agents;
    double allocated_capital{0.0};
};
struct DevSupervisedOrder {
    std::string order_id;
    std::string manager_id;
    std::string agent_id;
    std::string symbol;
    DevSupervisedAction action{DevSupervisedAction::HOLD};
    std::int64_t timestamp{0};
    std::int64_t shares{0};
    double market_price{0.0};
};
struct DevOrderReport {
    std::string order_id, manager_id, agent_id, symbol, reason;
    DevSupervisedAction action{DevSupervisedAction::HOLD};
    DevOrderStatus status{DevOrderStatus::REJECTED};
    std::int64_t timestamp{0}, shares{0};
    double execution_price{0.0}, fee{0.0}, realized_pnl{0.0};
};
struct DevSupervisedPosition {
    std::string manager_id, agent_id, symbol;
    std::int64_t shares{0};
    double entry_price{0.0};
    double entry_fee{0.0};
};
struct DevManagerAccount {
    DevManagerRegistration registration;
    double cash{0.0};
    double realized_pnl{0.0};
    std::map<std::string, DevSupervisedPosition> positions;
};
struct DevSupervisorSnapshot {
    double cash{0.0}, equity{0.0}, gross_exposure{0.0}, realized_pnl{0.0};
    std::size_t open_positions{0};
    bool halted{false};
};
class DevinderjeetSupervisor {
public:
    explicit DevinderjeetSupervisor(DevSupervisorConfig config = {});
    void registerManager(const DevManagerRegistration& manager);
    DevOrderReport execute(const DevSupervisedOrder& order);
    void updateMark(const std::string& symbol, double close_price);
    DevSupervisorSnapshot snapshot() const;
    DevManagerAccount managerAccount(const std::string& manager_id) const;
    std::vector<DevOrderReport> audit() const;
    void setHalted(bool halted);
private:
    DevSupervisorSnapshot snapshotUnlocked() const;
    DevOrderReport executeUnlocked(const DevSupervisedOrder& order);
    DevSupervisorConfig config_;
    mutable std::mutex mutex_;
    std::map<std::string, DevManagerAccount> managers_;
    std::map<std::string, double> marks_;
    std::map<std::string, DevOrderReport> order_results_;
    std::vector<DevOrderReport> audit_;
    bool halted_{false};
};
}
