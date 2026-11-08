#include "devai/strategy/dev_ai_strategy/DevHybridMathematicalSignal.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai;
int main() {
 statistics::MathematicalStateFeatures s{};
 s.stock_symbol="NTPC"; s.decision_time=1784089980;
 s.numerical_integrity=true; s.fully_ready=true;
 s.one_minute_ready=s.five_minute_ready=s.fifteen_minute_ready=true;
 auto& a=s.one_minute.return_statistics;
 auto& b=s.five_minute.return_statistics;
 auto& c=s.fifteen_minute.return_statistics;
 a.ready=b.ready=c.ready=true;
 a.rolling_mean=.001; a.rolling_standard_deviation=.002;
 a.z_score=-2.; b.z_score=-.3; c.z_score=.1;
 auto signal=strategy::makeHybridMathematicalSignal(s);
 assert(signal.ready && signal.buy_candidate && !signal.exit_candidate);
 assert(signal.opportunity_score>0 && signal.opportunity_score<=1);
 a.z_score=0.; signal=strategy::makeHybridMathematicalSignal(s);
 assert(signal.ready && !signal.buy_candidate && signal.exit_candidate);
 s.fully_ready=false; signal=strategy::makeHybridMathematicalSignal(s);
 assert(!signal.ready && !signal.buy_candidate && !signal.exit_candidate);
 s.fully_ready=true; a.z_score=std::nan("");
 assert(!strategy::makeHybridMathematicalSignal(s).ready);
 bool rejected=false;
 try { strategy::DevHybridSignalConfig cfg{}; cfg.entry_z_threshold=1.;
       (void)strategy::makeHybridMathematicalSignal(s,cfg); }
 catch(const std::invalid_argument&) { rejected=true; }
 assert(rejected);
 std::cout << "[CETE][H47][5E2K6A] mathematical signal tests PASS\n";
}
