#include "devai/strategy/dev_ai_strategy/DevRealHistoricalCacheGenerator.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"

#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace devai::strategy;

struct TestStats { std::size_t checks{0}; std::size_t failures{0}; };

void require(bool condition, const std::string& text, TestStats& s)
{
    ++s.checks;
    if (condition) std::cout << "[PASS] " << text << '\n';
    else { ++s.failures; std::cerr << "[FAIL] " << text << '\n'; }
}

template <class F>
void expectThrow(F&& f, const std::string& text, TestStats& s)
{
    bool threw=false; try { f(); } catch (const std::exception&) { threw=true; }
    require(threw, text, s);
}

devai::ranking::CrossSectionalUniverseSnapshot makeSnapshot(
    std::int64_t t,
    std::vector<std::string> symbols)
{
    devai::ranking::CrossSectionalUniverseSnapshot x{};
    x.decision_time=t;
    x.benchmark_symbol="NIFTY%2050";
    for (const auto& symbol : symbols)
    {
        devai::ranking::CrossSectionalMathematicalState s{};
        s.symbol=symbol;
        s.benchmark_symbol=x.benchmark_symbol;
        s.decision_time=t;
        s.numerical_integrity=true;
        x.stocks.push_back(s);
    }
    x.stock_count=x.stocks.size();
    x.numerical_integrity=true;
    return x;
}

DevHistoricalSnapshotSequence makeSnapshots()
{
    DevHistoricalSnapshotSequence snapshots;
    snapshots.push_back(makeSnapshot(1000,{"AAA","BBB","CCC"}));
    snapshots.push_back(makeSnapshot(1060,{"AAA","BBB","CCC"}));
    snapshots.push_back(makeSnapshot(1120,{"AAA","BBB","CCC"}));
    return snapshots;
}

DevTopCandidateSelection selectionFor(
    const devai::ranking::CrossSectionalUniverseSnapshot& s)
{
    std::vector<DevCandidateScore> candidates;
    for (std::size_t i=0; i<s.stocks.size(); ++i)
    {
        DevCandidateScore c{};
        c.symbol=s.stocks[i].symbol;
        c.decision_time=s.decision_time;
        c.score=1.0-static_cast<double>(i)*0.1;
        c.eligible = (s.decision_time==1000 ? i<2 :
                      s.decision_time==1060 ? i<1 : false);
        c.numerical_integrity=true;
        candidates.push_back(c);
    }
    DevTopCandidateSelector selector;
    return selector.select(candidates,s.benchmark_symbol);
}

DevAIObservation observationFor(
    const devai::ranking::CrossSectionalUniverseSnapshot& s,
    const std::string& symbol,
    const DevPortfolioState& portfolio)
{
    DevAIObservation o{};
    o.agent_name="Dev";
    o.symbol=symbol;
    o.benchmark_symbol=s.benchmark_symbol;
    o.decision_time=s.decision_time;
    o.market.fill(0.0);
    o.availability.fill(true);
    o.portfolio=portfolio;
    o.numerical_integrity=true;
    o.ready=true;
    return o;
}

std::vector<DevHistoricalOutcome> outcomesFor(
    const devai::ranking::CrossSectionalUniverseSnapshot& s,
    const std::string&)
{
    DevHistoricalOutcome o{};
    o.horizon_bars=5;
    if (s.decision_time==1000)
    {
        o.outcome_time=1300;
        o.forward_return=0.01;
        o.available=true;
    }
    o.numerical_integrity=true;
    return {o};
}

} // namespace

int main()
{
    TestStats stats;
    std::cout << "\n======================================================================\n"
              << "DEVAI PHASE 3.15B — REAL HISTORICAL CACHE GENERATOR\n"
              << "======================================================================\n";

    DevRealHistoricalCacheGenerator generator;
    const auto snapshots=makeSnapshots();

    const auto result=generator.generateWithObservationProvider(
        snapshots, selectionFor, observationFor, {}, outcomesFor);

    require(result.stats.input_snapshot_count==3,
            "Generator consumes all chronological CETE replay snapshots",stats);
    require(result.dataset.records.size()==3,
            "Generator creates one cache record per CETE snapshot",stats);
    require(result.dataset.records[0].candidates.size()==2,
            "Generator preserves two qualifying candidates",stats);
    require(result.dataset.records[1].candidates.size()==1,
            "Generator preserves one qualifying candidate",stats);
    require(result.dataset.records[2].candidates.empty(),
            "Generator preserves zero-candidate decision without backfill",stats);
    require(result.stats.empty_candidate_records==1,
            "Generator counts empty-candidate decision records",stats);
    require(result.stats.generated_candidate_count==3,
            "Generator counts generated candidate observations",stats);
    require(result.dataset.records[0].candidates[0].agent_name=="DevAgent_001" &&
            result.dataset.records[0].candidates[1].agent_name=="DevAgent_002",
            "Generator preserves frozen DevAgent slot identities",stats);
    require(result.dataset.records[0].candidates[0].observation.decision_time==1000,
            "Generated observation remains causal at current decision time",stats);
    require(result.dataset.records[0].candidates[0].outcomes[0].outcome_time>1000,
            "Future outcome remains separate and matures after origin time",stats);
    require(result.stats.numerical_integrity,
            "Generator reports numerical integrity",stats);

    const auto path=std::filesystem::temp_directory_path()/"devai_phase_3_15b_test.cache";
    DevHistoricalTrainingCache::save(path,result.dataset);
    const auto loaded=DevHistoricalTrainingCache::load(path);
    require(loaded.records.size()==3,
            "Generated real-cache dataset survives Phase-3.15A save/load",stats);
    require(loaded.records[0].candidates[0].symbol=="AAA",
            "Persisted generator output preserves selected symbol",stats);
    std::filesystem::remove(path);

    expectThrow([&]{
        auto bad=snapshots; bad[0].numerical_integrity=false;
        (void)generator.generateWithObservationProvider(bad,selectionFor,observationFor);
    },"Generator rejects failed snapshot numerical integrity",stats);

    expectThrow([&]{
        auto bad=snapshots; bad[1].decision_time=900;
        (void)generator.generateWithObservationProvider(bad,selectionFor,observationFor);
    },"Generator rejects non-chronological CETE snapshots",stats);

    expectThrow([&]{
        auto badSelection=[](const auto& s){ auto x=selectionFor(s); x.decision_time+=60; return x; };
        (void)generator.generateWithObservationProvider(snapshots,badSelection,observationFor);
    },"Generator rejects selection from another decision time",stats);

    expectThrow([&]{
        auto badSelection=[](const auto& s){ auto x=selectionFor(s); if(x.selected_candidate_count){x.assignments[0].symbol="MISSING";} return x; };
        (void)generator.generateWithObservationProvider(snapshots,badSelection,observationFor);
    },"Generator rejects selected symbol absent from CETE snapshot",stats);

    expectThrow([&]{
        auto badObs=[](const auto& s,const std::string& symbol,const DevPortfolioState& p){ auto o=observationFor(s,symbol,p); o.decision_time+=60; return o; };
        (void)generator.generateWithObservationProvider(snapshots,selectionFor,badObs);
    },"Phase-3.15A validator rejects observation-time leakage/mismatch",stats);

    expectThrow([&]{
        auto badOutcome=[](const auto& s,const std::string&){ DevHistoricalOutcome o{}; o.horizon_bars=5; o.outcome_time=s.decision_time; o.forward_return=0.1; o.available=true; o.numerical_integrity=true; return std::vector<DevHistoricalOutcome>{o}; };
        (void)generator.generateWithObservationProvider(snapshots,selectionFor,observationFor,{},badOutcome);
    },"Phase-3.15A validator rejects non-future available outcome",stats);

    expectThrow([&]{
        (void)generator.generateWithObservationProvider(snapshots,{},observationFor);
    },"Generator requires explicit Phase-3.11 selection provider",stats);

    std::cout << "\n======================================================================\n"
              << "PHASE 3.15B REAL HISTORICAL CACHE GENERATOR SUMMARY\n"
              << "======================================================================\n"
              << "Checks   : " << stats.checks << '\n'
              << "Failures : " << stats.failures << '\n'
              << "======================================================================\n"
              << (stats.failures==0 ? "DEVAI PHASE 3.15B REAL HISTORICAL CACHE GENERATOR: PASSED\n"
                                    : "DEVAI PHASE 3.15B REAL HISTORICAL CACHE GENERATOR: FAILED\n");
    return stats.failures==0 ? 0 : 1;
}
