#include "devai/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace devai::strategy::d9f2 {
double trainingScoreThreshold(std::vector<double> scores, EligibilityConfig cfg){
 if(scores.empty() || !std::isfinite(cfg.percentile) || cfg.percentile<0 || cfg.percentile>1 || !std::isfinite(cfg.minimum_score))
  throw std::invalid_argument("D9F2 invalid calibration inputs");
 for(double x:scores)if(!std::isfinite(x))throw std::invalid_argument("D9F2 nonfinite training score");
 std::sort(scores.begin(),scores.end());
 const auto idx=static_cast<std::size_t>(std::floor(cfg.percentile*static_cast<double>(scores.size()-1)));
 return std::max(cfg.minimum_score,scores.at(idx));
}
EligibilityResult entryEligibility(bool available,double score,double threshold,bool time_ok,bool budget_ok){
 if(!std::isfinite(score)||!std::isfinite(threshold))throw std::invalid_argument("D9F2 nonfinite score");
 EligibilityResult r;
 r.available=available;r.score_pass=score>=threshold;r.advantage_pass=true;r.time_pass=time_ok;r.budget_pass=budget_ok;
 r.accepted=r.available&&r.score_pass&&r.time_pass&&r.budget_pass;
 return r;
}
EligibilityResult switchEligibility(bool available,double score,double threshold,double advantage,double required,
 bool time_ok,bool budget_ok){
 if(!std::isfinite(advantage)||!std::isfinite(required))throw std::invalid_argument("D9F2 nonfinite advantage");
 auto r=entryEligibility(available,score,threshold,time_ok,budget_ok);
 r.advantage_pass=advantage>=required;
 r.accepted=r.accepted&&r.advantage_pass;
 return r;
}
}
