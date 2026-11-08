#pragma once
#include <vector>
#include <cstddef>
namespace devai::strategy::d9f2 {
struct EligibilityConfig { double percentile=0.80; double minimum_score=0.0; };
struct EligibilityResult { bool accepted=false; bool available=false; bool score_pass=false; bool advantage_pass=false; bool time_pass=false; bool budget_pass=false; };
// No forward returns or future candles may be passed to this API.
double trainingScoreThreshold(std::vector<double> scores, EligibilityConfig cfg={});
EligibilityResult entryEligibility(bool candidate_available, double candidate_score,
  double threshold, bool time_ok, bool budget_ok);
EligibilityResult switchEligibility(bool candidate_available, double candidate_score,
  double threshold, double score_advantage, double required_advantage,
  bool time_ok, bool budget_ok);
}
