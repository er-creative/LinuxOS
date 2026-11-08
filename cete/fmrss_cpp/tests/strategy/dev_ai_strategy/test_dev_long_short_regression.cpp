// CETE / DevAI Phase 3.19D-LS: deterministic shared-cash LONG + SHORT tests.
// Tests the actual DevRealHistoricalMultiStockOOS implementation, not a mock.
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace devai::strategy;
namespace {
int checks = 0, failures = 0;
void check(bool condition, const std::string& label) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "[FAIL] " << label << '\n'; }
    else std::cout << "[PASS] " << label << '\n';
}
void near(double actual, double expected, const std::string& label, double eps=1e-7) {
    check(std::isfinite(actual) && std::fabs(actual-expected)<=eps,
          label + " (actual=" + std::to_string(actual) +
          ", expected=" + std::to_string(expected) + ")");
}
template<class F> void mustThrow(F&& f, const std::string& label) {
    bool threw=false;
    try { f(); } catch (const std::exception&) { threw=true; }
    check(threw,label);
}
// UTC epoch for an IST wall clock time; avoids dependence on machine timezone.
std::int64_t ist(int hour,int minute) {
    using namespace std::chrono;
    const auto date=sys_days{year{2026}/10/9};
    return duration_cast<seconds>(date.time_since_epoch()+hours{hour}+minutes{minute}-minutes{330}).count();
}
DevOOSLedgerConfig config(double initial=7000.0, double allocation=.75) {
    DevOOSLedgerConfig c{};
    c.initial_capital=initial;
    c.max_allocation_fraction=allocation;
    c.short_square_off_hour_ist=15;
    c.short_square_off_minute_ist=15;
    c.allow_intraday_short=true;
    return c;
}
DevOOSPriceProvider constant(double p) {
    return [p](const std::string&,std::int64_t) { return p; };
}
void reconcile(const DevOOSLedgerState& s, double marked_price, const std::string& label) {
    near(s.cash + static_cast<double>(s.quantity)*marked_price, s.equity,
         label+": signed equity reconciliation");
    check(s.cash-s.restricted_short_proceeds>=-1e-7,
          label+": restricted proceeds are not spendable");
}
}
int main() {
    std::cout << "\nCETE PHASE 3.19D-LS — LONG + SHORT REGRESSION\n";
    try {
        // 1. LONG entry, hold, exit, whole shares and realized P&L.
        {
            DevRealHistoricalMultiStockOOS ledger(config());
            auto r=ledger.execute(ist(10,0),"RELIANCE",constant(100.0));
            check(ledger.state().quantity==52,"LONG entry: 52 whole shares within 75% of 7000");
            near(ledger.state().cash,1800.0,"LONG entry: cash deducted");
            near(ledger.state().equity,7000.0,"LONG entry: equity unchanged before costs");
            check(r.executed_legs==1,"LONG entry: one execution leg");
            reconcile(ledger.state(),100.0,"LONG entry");
            r=ledger.execute(ist(10,1),"RELIANCE",constant(110.0));
            check(r.executed_legs==0,"LONG hold: no unnecessary fill");
            near(ledger.state().equity,7520.0,"LONG hold: positive mark-to-market");
            r=ledger.execute(ist(10,2),"",constant(110.0));
            check(ledger.state().quantity==0 && ledger.state().held_symbol.empty(),
                  "LONG exit: flat position");
            near(ledger.state().cash,7520.0,"LONG exit: realized proceeds");
            near(ledger.state().realized_net_pnl,520.0,"LONG exit: realized profit");
            reconcile(ledger.state(),0.0,"LONG exit");
        }
        // 2. SHORT entry: proceeds restricted; equity remains capital.
        {
            DevRealHistoricalMultiStockOOS ledger(config());
            auto r=ledger.execute(ist(10,0),"SHORT:SBIN",constant(100.0));
            check(ledger.state().quantity==-52,"SHORT entry: signed whole-share quantity");
            near(ledger.state().cash,12200.0,"SHORT entry: sale proceeds booked");
            near(ledger.state().restricted_short_proceeds,5200.0,
                 "SHORT entry: proceeds restricted");
            near(ledger.state().equity,7000.0,"SHORT entry: proceeds are not profit");
            check(r.executed_legs==1,"SHORT entry: one execution leg");
            reconcile(ledger.state(),100.0,"SHORT entry");
            const auto portfolio=DevRealHistoricalMultiStockOOS::portfolioAt(
                ledger.state(),100.0,ist(10,1),7000.0);
            check(portfolio.numerical_integrity,"SHORT portfolio: numerical integrity");
            check(portfolio.current_symbol=="SBIN","SHORT portfolio: correct symbol");
            r=ledger.execute(ist(10,1),"SHORT:SBIN",constant(90.0));
            check(r.executed_legs==0,"SHORT hold: no unnecessary fill");
            near(ledger.state().equity,7520.0,"SHORT hold: falling price increases equity");
            r=ledger.execute(ist(10,2),"",constant(90.0));
            near(ledger.state().cash,7520.0,"SHORT cover: correct cash balance");
            near(ledger.state().realized_net_pnl,520.0,"SHORT cover: realized profit");
            near(ledger.state().restricted_short_proceeds,0.0,"SHORT cover: proceeds released");
            check(ledger.state().quantity==0,"SHORT cover: flat");
            reconcile(ledger.state(),0.0,"SHORT cover");
        }
        // 3. SHORT loss and drawdown.
        {
            DevRealHistoricalMultiStockOOS ledger(config());
            (void)ledger.execute(ist(10,0),"SHORT:SBIN",constant(100.0));
            auto r=ledger.execute(ist(10,1),"SHORT:SBIN",constant(110.0));
            near(ledger.state().equity,6480.0,"SHORT loss: rising price reduces equity");
            check(r.drawdown>0.0,"SHORT loss: drawdown registered");
            (void)ledger.execute(ist(10,2),"",constant(110.0));
            near(ledger.state().realized_net_pnl,-520.0,"SHORT loss: realized loss");
        }
        // 4. Reversal LONG -> SHORT, then SHORT -> LONG; close first.
        {
            DevRealHistoricalMultiStockOOS ledger(config());
            (void)ledger.execute(ist(10,0),"SBIN",constant(100.0));
            auto r=ledger.execute(ist(10,1),"SHORT:SBIN",constant(100.0));
            check(r.executed_legs==2,"LONG to SHORT: close and open legs");
            check(ledger.state().quantity<0,"LONG to SHORT: short established");
            reconcile(ledger.state(),100.0,"LONG to SHORT");
            r=ledger.execute(ist(10,2),"SBIN",constant(100.0));
            check(r.executed_legs==2,"SHORT to LONG: cover and buy legs");
            check(ledger.state().quantity>0,"SHORT to LONG: long established");
            near(ledger.state().restricted_short_proceeds,0.0,"SHORT to LONG: collateral released");
            reconcile(ledger.state(),100.0,"SHORT to LONG");
        }
        // 5. Allocation and cash protection with a large unit price.
        {
            DevRealHistoricalMultiStockOOS ledger(config());
            (void)ledger.execute(ist(10,0),"SHORT:SBIN",constant(6000.0));
            check(ledger.state().quantity==0,"SHORT: no fractional share or over-allocation");
            near(ledger.state().cash,7000.0,"SHORT: no fictitious sale proceeds");
        }
        // 6. Cost accounting: both entry and exit charged.
        {
            auto c=config();
            c.brokerage_rate=.001;
            c.slippage_rate=.001;
            c.other_cost_rate=.001;
            DevRealHistoricalMultiStockOOS ledger(c);
            auto entry=ledger.execute(ist(10,0),"SHORT:SBIN",constant(100.0));
            check(ledger.state().quantity<0,"SHORT with fees: entry accepted");
            near(entry.brokerage+entry.slippage+entry.other_costs,
                 -static_cast<double>(ledger.state().quantity)*100.0*.003,
                 "SHORT entry: configured costs charged");
            const auto qty=-ledger.state().quantity;
            auto exit=ledger.execute(ist(10,1),"",constant(90.0));
            const double expected=qty*10.0 - qty*(100.0+90.0)*.003;
            near(ledger.state().realized_net_pnl,expected,
                 "SHORT round-trip: net realized P&L after costs");
            near(ledger.state().equity,7000.0+expected,
                 "SHORT round-trip: equity includes both legs' costs");
            check(exit.executed_legs==1,"SHORT round-trip: one cover leg");
        }
        // 7. Short must square off at cutoff, even if policy requests hold.
        {
            DevRealHistoricalMultiStockOOS ledger(config());
            (void)ledger.execute(ist(15,13),"SHORT:SBIN",constant(100.0));
            (void)ledger.execute(ist(15,14),"SHORT:SBIN",constant(100.0));
            auto r=ledger.execute(ist(15,15),"SHORT:SBIN",constant(100.0));
            check(r.executed_legs==1,"Cutoff: held SHORT forcibly covered");
            check(ledger.state().quantity==0,"Cutoff: no SHORT remains");
            r=ledger.execute(ist(15,16),"SHORT:SBIN",constant(100.0));
            check(ledger.state().quantity==0 && r.executed_legs==0,
                  "Cutoff: new SHORT blocked");
        }
        // 8. Short disabled and input validation.
        {
            auto c=config(); c.allow_intraday_short=false;
            DevRealHistoricalMultiStockOOS ledger(c);
            mustThrow([&]{ (void)ledger.execute(ist(10,0),"SHORT:SBIN",constant(100.0)); },
                      "Short-disabled configuration rejects SHORT");
            check(ledger.state().quantity==0,"Rejected SHORT leaves ledger unchanged");
            mustThrow([&]{ (void)ledger.execute(ist(10,0),"SBIN",constant(-1.0)); },
                      "Invalid market price rejected");
        }
        // 9. Strict chronology.
        {
            DevRealHistoricalMultiStockOOS ledger(config());
            (void)ledger.execute(ist(10,0),"SBIN",constant(100.0));
            mustThrow([&]{ (void)ledger.execute(ist(10,0),"",constant(100.0)); },
                      "Duplicate timestamp rejected");
            mustThrow([&]{ (void)ledger.execute(ist(9,59),"",constant(100.0)); },
                      "Backward timestamp rejected");
        }
    } catch (const std::exception& e) {
        ++failures;
        std::cerr << "[UNEXPECTED] " << e.what() << '\n';
    }
    std::cout << "\nChecks: " << checks << "  Failures: " << failures << '\n';
    std::cout << "CETE PHASE 3.19D-LS: " << (failures?"FAILED":"PASSED") << '\n';
    return failures ? 1 : 0;
}
