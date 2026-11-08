#pragma once
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cete::replay {
struct DecisionStatus {
    bool allowed = false; // historical evaluation permitted; NOT an OPEN authorization
    bool forwardLabelEligible = false; // offline only; false in hybrid mode
    std::string reason;
    bool openEligible = false; // data/timing only; manager and risk controls still decide
    bool current1m = false;
    bool current5m = false;
    bool current15m = false;
};
struct UniverseStatus {
    bool ready = false;
    std::string reason;
    std::vector<std::string> eligibleStocks;
    std::map<std::string, DecisionStatus> byStock;
};
struct HybridConfig {
    std::size_t historical1m = 1;
    std::size_t historical5m = 1;
    std::size_t historical15m = 1;
    bool requireCurrent1mForOpen = true;
};
class ReplayEligibilityGate {
public:
    // Legacy report-only mode (preserved for compatibility; retains conservative behavior).
    void load(const std::string& dailyCsv, const std::string& decisionCsv);
    // Hybrid mode: Phase 3A is required to reconstruct causal candle availability.
    void loadHybrid(const std::string& dailyCsv, const std::string& decisionCsv,
                    const std::string& phase3aDirectory, HybridConfig config = {});
    DecisionStatus check(const std::string& date, const std::string& time,
                         const std::string& symbol) const;
    UniverseStatus evaluate(const std::string& date, const std::string& time,
                            const std::vector<std::string>& universe,
                            std::size_t minCandidates = 5) const;
    std::size_t decisionRows() const noexcept { return decisions_.size(); }
    std::size_t dailyRows() const noexcept { return daily_.size(); }
private:
    struct Daily { bool decisionEligible=false; bool fullSessionEligible=false;
                   bool structural=false; bool mismatch=false; std::string reason; };
    struct Decision { bool eligible=false; bool forward=false; bool benchmark=false;
                      bool oneMinute=false; bool five=false; bool fifteen=false;
                      bool warmup=false; std::string reason; };
    struct Session { std::set<int> missing[3]; bool present[3]={false,false,false}; };
    static int tfIndex(int tf);
    bool candlePresent(const std::string& date, const std::string& symbol,
                       int tf, int startMinute) const;
    std::size_t availableCount(const std::string& date, const std::string& time,
                               const std::string& symbol, int tf) const;
    DecisionStatus hybridCheck(const std::string& date, const std::string& time,
                               const std::string& symbol) const;
    std::map<std::string,Daily> daily_;
    std::map<std::string,Decision> decisions_;
    std::map<std::string,Session> sessions_;
    std::set<std::string> dates_;
    HybridConfig config_;
    bool hybrid_ = false;
};
} // namespace cete::replay
