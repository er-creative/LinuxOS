#include "devai/strategy/dev_ai_strategy/DevHistoricalMinuteValidator.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 4 || (argc - 1) % 2 != 0) {
        std::cerr << "Usage: " << argv[0] << " 'BENCHMARK_SYMBOL' 'BENCHMARK_FILE' 'STOCK_SYMBOL' 'STOCK_FILE' [STOCK_SYMBOL STOCK_FILE ...]\n";
        return 2;
    }
    try {
        devai::strategy::DevHistoricalMinuteValidationConfig config;
        config.benchmark_symbol = argv[1];
        std::vector<devai::strategy::DevHistoricalMinuteInput> inputs;
        for (int i = 1; i + 1 < argc; i += 2) inputs.push_back({argv[i],argv[i+1]});
        const devai::strategy::DevHistoricalMinuteValidator validator(config);
        const auto report = validator.validate(inputs);
        for (const auto& symbol : report.symbols)
            std::cout << symbol.symbol << " rows=" << symbol.rows
                      << " invalid=" << symbol.invalid_rows
                      << " duplicates=" << symbol.duplicate_minutes
                      << " missing=" << symbol.missing_session_minutes
                      << " partial_days=" << symbol.partial_sessions
                      << " complete_days=" << symbol.complete_sessions
                      << " complete5=" << symbol.complete_5m_windows
                      << " complete15=" << symbol.complete_15m_windows << '\n';
        std::cout << "synchronized5=" << report.synchronized_5m_windows
                  << " synchronized15=" << report.synchronized_15m_windows << '\n';
        std::cout << "all_sessions_complete=" << (report.all_sessions_complete ? "yes" : "no") << '\n';
        for (const auto& issue : report.issues) std::cerr << "DIAGNOSTIC: " << issue << '\n';
        std::cout << (report.passed ? "PASS" : "FAIL") << " 5C-3I-D4 real historical minute data validation\n";
        return report.passed ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
