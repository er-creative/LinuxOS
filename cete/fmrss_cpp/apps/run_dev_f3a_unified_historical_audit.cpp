// F3A: Read-only historical cache -> unified agent/manager model audit.
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevAgentManagerContract.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace ds = devai::strategy;

static void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4)
            throw std::invalid_argument("Usage: run_dev_f3a_unified_historical_audit <cache> --random-smoke | <cache> --checkpoint <shared_model_file>");
        const std::string mode(argv[2]);
        require((mode == "--random-smoke" && argc == 3) ||
                (mode == "--checkpoint" && argc == 4), "Invalid model mode/arguments");

        const auto data = ds::DevHistoricalTrainingCache::load(argv[1]);
        ds::DevHistoricalTrainingDatasetValidator::validate(data);
        require(!data.records.empty(), "Historical cache has no records");

        ds::DevSharedNeuralNetwork shared_model;
        if (mode == "--checkpoint") {
            shared_model.load(argv[3]);
            std::cout << "Model: unified checkpoint loaded; training quality not established\n";
        } else {
            std::cout << "Model: unified RANDOM INITIALIZATION; wiring test only, NOT trained inference\n";
        }
        ds::DevManager manager;
        ds::DevAgentManagerContract contract;
        ds::DevManagerStateEncoder encoder;
        std::size_t agent_evaluations = 0, manager_evaluations = 0, illegal = 0;
        std::int64_t last_time = 0;
        const auto& names = ds::DevTopCandidateSelector::agentNames();

        for (const auto& record : data.records) {
            require(record.numerical_integrity && record.decision_time > last_time,
                    "Invalid or nonchronological historical decision");
            last_time = record.decision_time;
            require(!record.benchmark_symbol.empty() && record.candidates.size() <= ds::DEV_AGENT_SLOT_COUNT,
                    "Invalid benchmark or candidate count");
            std::vector<ds::DevAgentReport> reports;
            reports.reserve(record.candidates.size());
            std::unordered_set<std::string> symbols;
            double previous_score = 0.0;
            std::cout << "\nDecision " << record.decision_time << " | candidates=" << record.candidates.size() << '\n';

            for (std::size_t slot = 0; slot < record.candidates.size(); ++slot) {
                const auto& c = record.candidates[slot];
                const auto& o = c.observation;
                require(c.numerical_integrity && c.slot_index == slot &&
                        c.agent_name == names[slot] && c.candidate_rank == slot + 1 &&
                        !c.symbol.empty() && c.symbol != record.benchmark_symbol &&
                        symbols.insert(c.symbol).second && std::isfinite(c.candidate_score) &&
                        (slot == 0 || c.candidate_score <= previous_score),
                        "F1 candidate identity/rank/score invalid");
                previous_score = c.candidate_score;
                require(o.ready && o.numerical_integrity && o.symbol == c.symbol &&
                        o.benchmark_symbol == record.benchmark_symbol &&
                        o.decision_time == record.decision_time,
                        "Historical observation identity/readiness mismatch");
                for (double x : o.market)
                    require(std::isfinite(x) && x >= -1.00000001 && x <= 1.00000001,
                            "Market percentile outside [-1,1] or nonfinite");
                for (double x : o.availability)
                    require(x == 0.0 || x == 1.0, "Nonbinary availability mask");
                const auto& p = o.portfolio;
                for (double x : {p.position, p.unrealized_return, p.realized_return,
                                 p.equity_return, p.drawdown, p.holding_period})
                    require(std::isfinite(x), "Nonfinite observation portfolio field");

                // Unified network directly predicts each agent: DevAgent::evaluate() uses
                // the *different* DevQNetwork interface, so it is not used here.
                const auto q = shared_model.predictAgent(o);
                require(q.numerical_integrity, "Unified agent Q numerical integrity failed");
                for (double x : q.values) require(std::isfinite(x), "Nonfinite agent Q");
                const auto preferred = q.greedyAction();
                require(ds::isValidAction(preferred), "Invalid preferred agent action");
                ds::DevAgentReport r{};
                r.slot_index = slot;
                r.agent_name = c.agent_name;
                r.assigned = true;
                r.symbol = c.symbol;
                r.decision_time = record.decision_time;
                r.candidate_score = c.candidate_score;
                r.candidate_rank = c.candidate_rank;
                r.q_values = q;
                r.preferred_action = preferred;
                r.preferred_q_value = q.value(preferred);
                r.numerical_integrity = std::isfinite(r.preferred_q_value);
                require(r.numerical_integrity, "Invalid preferred Q value");
                reports.push_back(r);
                ++agent_evaluations;
                std::cout << "  " << r.agent_name << ' ' << std::setw(12) << std::left
                          << r.symbol << std::right << " rank=" << r.candidate_rank
                          << " preferred=" << static_cast<std::size_t>(preferred) << '\n';
            }

            const auto packet = contract.makePacket(record.decision_time, reports);
            contract.validatePacket(packet);
            require(packet.report_count == reports.size(), "Manager packet count mismatch");

            // Explicit flat portfolio, decision-only test. No capital or ledger changes.
            ds::DevManagerPortfolioState portfolio{};
            portfolio.numerical_integrity = true;
            portfolio.portfolio.position = 0.0;
            const auto state = contract.buildManagerState(packet, portfolio, manager);
            manager.validateState(state);
            const auto encoded = encoder.encode(state);
            require(encoded.numerical_integrity && encoded.decision_time == record.decision_time,
                    "Manager encoded state invalid");
            require(encoded.valid_actions[0] && !encoded.valid_actions[1],
                    "Flat portfolio should allow NO_TRADE but forbid KEEP_CURRENT");
            for (std::size_t i = 0; i < ds::DEV_AGENT_SLOT_COUNT; ++i)
                require(encoded.valid_actions[i + 2] == (i < reports.size()),
                        "Manager candidate action mask mismatch");

            const auto manager_q = shared_model.predictManager(encoded);
            require(manager_q.numerical_integrity, "Unified manager Q integrity failed");
            for (double x : manager_q.values) require(std::isfinite(x), "Nonfinite manager Q");
            const auto chosen = manager_q.greedyAction(encoded.valid_actions);
            const auto action_index = static_cast<std::size_t>(chosen);
            if (!encoded.valid_actions[action_index]) ++illegal;
            require(encoded.valid_actions[action_index], "Selected illegal manager action");

            // Explicit NO_TRADE contract audit; this is NOT a claim that the
            // model chose NO_TRADE or that the model's action is execution-safe.
            const auto no_trade = manager.makeNoTradeDecision(state);
            const auto envelope = contract.makeDecisionEnvelope(state, no_trade, manager);
            contract.validateDecisionEnvelope(state, envelope, manager);
            require(envelope.numerical_integrity &&
                    envelope.decision.type == ds::DevManagerDecisionType::NO_TRADE,
                    "NO_TRADE decision envelope invalid");
            ++manager_evaluations;
            std::cout << "  Manager greedy legal action=" << action_index
                      << " | NO_TRADE envelope=VALID | execution=DISABLED\n";
        }

        require(agent_evaluations > 0 && manager_evaluations == data.records.size(),
                "Missing agent or manager evaluations");
        std::cout << "\nF3A UNIFIED HISTORICAL AUDIT: PASSED\n"
                  << "Records evaluated: " << data.records.size() << '\n'
                  << "DevAgent evaluations: " << agent_evaluations << '\n'
                  << "DevManager evaluations: " << manager_evaluations << '\n'
                  << "Unified model objects: 1\n"
                  << "Illegal actions selected: " << illegal << '\n'
                  << "Portfolio mutations: 0\n"
                  << "No training, simulated trades, or live execution performed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "F3A UNIFIED HISTORICAL AUDIT: FAILED: " << ex.what() << '\n';
        return 1;
    }
}
