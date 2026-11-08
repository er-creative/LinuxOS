#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace devai::strategy {
enum class DevRunMode { ANALYSE, TESTING, GO_LIVE };
enum class DevApprovalMode { MANUAL, AUTO };
enum class DevIntent { HOLD, OPEN_LONG, CLOSE_LONG, OPEN_SHORT, CLOSE_SHORT, FORCE_EXIT };
enum class DevOrderStatus { SUGGESTION_ONLY, SIMULATED, PENDING_APPROVAL, BLOCKED, SUBMITTED, REJECTED };
struct DevGatewayConfig {
 DevRunMode mode{DevRunMode::ANALYSE};
 DevApprovalMode approval{DevApprovalMode::MANUAL};
 bool enable_long{true};
 bool enable_short{false};
 bool broker_connected{false};
 bool live_armed{false};
 bool market_data_fresh{false};
 bool broker_state_reconciled{false};
 bool kill_switch{false};
 int new_entry_cutoff{915};
 int force_exit_minute{915};
 std::set<std::string> allowed_symbols;
 std::uint64_t max_quantity{100};
};
struct DevTradeSuggestion {
 std::string symbol;
 std::int64_t decision_key{0}; // CETE exchange-local completed-minute key; for opening decisions provide actual as-of time
 DevIntent intent{DevIntent::HOLD};
 std::uint64_t quantity{0};
 bool shortable{false};
 bool position_exists{false};
 bool model_approved{false};
 bool risk_approved{false};
 std::string decision_id; // unique id for broker deduplication
 double execution_price{0.0}; // TESTING: causal simulated fill price provided by replay engine
};
struct DevOrderResult {DevOrderStatus status{DevOrderStatus::BLOCKED};std::string reason;std::string broker_order_id;};
struct DevTestCostConfig {
 double brokerage_rate{0.0003}; // illustrative only; verify broker
 double brokerage_cap{20.0}; // INR per executed order
 double sell_stt_rate{0.00025}; // illustrative intraday equity STT; verify current rules
 double exchange_rate{0.0000307};
 double sebi_rate{0.000001};
 double stamp_buy_rate{0.00003};
 double gst_rate{0.18};
 double slippage_bps{2.0};
};
struct DevTestFill {
 std::string symbol, decision_id;
 DevIntent intent{DevIntent::HOLD};
 std::int64_t decision_key{0};
 std::uint64_t quantity{0};
 double price{0.0}, fees{0.0}, realized_pnl{0.0};
};
class DevBrokerAdapter {
public:
 virtual ~DevBrokerAdapter()=default;
 // Must implement broker-side idempotency and order-status reconciliation.
 virtual DevOrderResult submit(const DevTradeSuggestion& order)=0;
};
class DevExecutionGatewayD9C {
public:
 explicit DevExecutionGatewayD9C(DevGatewayConfig config, DevBrokerAdapter* broker=nullptr, DevTestCostConfig costs={});
 DevOrderResult process(const DevTradeSuggestion& suggestion, bool admin_approved=false);
 const DevGatewayConfig& config() const noexcept {return config_;}
 const std::vector<DevTestFill>& testFills() const noexcept {return test_fills_;}
 double testRealizedPnl() const noexcept {return test_realized_pnl_;}
 double testFees() const noexcept {return test_fees_;}
private:
 DevGatewayConfig config_;
 DevBrokerAdapter* broker_;
 DevTestCostConfig costs_;
 struct TestPosition {int side=0; std::uint64_t qty=0; double entry=0, entry_fees=0;};
 std::map<std::string,TestPosition> test_positions_;
 std::vector<DevTestFill> test_fills_;
 double test_realized_pnl_{0},test_fees_{0};
 DevOrderResult simulate(const DevTradeSuggestion& suggestion);
 std::map<std::string,DevOrderResult> processed_;
};
const char* toString(DevOrderStatus status);
}
