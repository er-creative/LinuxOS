#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include <cassert>
#include <iostream>
#include <vector>
using namespace devai::strategy;
int main() {
  std::vector<DevCandidateScore> stocks;
  for (int i=0;i<10;++i) stocks.push_back({"STOCK"+std::to_string(i), 123456, double(10-i), true, true});
  DevTopCandidateSelector selector(5);
  auto x=selector.select(stocks,"NIFTY%2050");
  assert(x.selected_candidate_count==5);
  for (int i=0;i<5;++i) {assert(x.assignments[i].candidate_rank==std::size_t(i+1));assert(x.assignments[i].symbol==stocks[i].symbol);}
  assert(x.assignments[4].agent_name=="DevAgent_005");
  assert(x.assignments[4].candidate_rank==5);
  assert(x.assignments[4].symbol==stocks[4].symbol);
  stocks.pop_back();
  x=selector.select(stocks,"NIFTY%2050");
  assert(x.assignments[4].assigned && x.selected_candidate_count==5);
  std::cout << "Stage 5D five-agent LONG-only selector tests PASS\n";
}
