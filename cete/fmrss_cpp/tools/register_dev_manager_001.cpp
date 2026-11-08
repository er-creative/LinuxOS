#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr const char* kManagerId = "DevManager_001";
constexpr const char* kBenchmark = "NIFTY%2050";

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

struct Universe {
    std::vector<std::string> stocks;
    bool benchmark_found{false};
};

Universe read_universe(const fs::path& file) {
    std::ifstream input(file);
    if (!input) throw std::runtime_error("Cannot open universe file: " + file.string());

    Universe result;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        if (line == kBenchmark) {
            if (result.benchmark_found)
                throw std::runtime_error("Duplicate NIFTY%2050 benchmark at line " + std::to_string(line_number));
            result.benchmark_found = true;
            continue;
        }
        result.stocks.push_back(line);
    }
    if (input.bad()) throw std::runtime_error("Error reading universe file: " + file.string());
    if (!result.benchmark_found)
        throw std::runtime_error("Required benchmark NIFTY%2050 not found in universe file");
    if (result.stocks.size() != 10)
        throw std::runtime_error("Expected exactly 10 stock entries, found " + std::to_string(result.stocks.size()));

    const auto validated = devai::DevManagerRegistry::validate_symbols(result.stocks);
    if (validated.size() != result.stocks.size())
        throw std::runtime_error("Duplicate stock symbols in universe file");
    result.stocks = validated;
    return result;
}

bool same_universe(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.size() != b.size()) return false;
    auto sorted_a = a;
    auto sorted_b = b;
    std::sort(sorted_a.begin(), sorted_a.end());
    std::sort(sorted_b.begin(), sorted_b.end());
    return sorted_a == sorted_b;
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <config/shares.txt> <existing-or-new-registry-directory>\n";
        return 2;
    }

    try {
        const fs::path universe_file = argv[1];
        const fs::path registry_root = argv[2];
        const Universe universe = read_universe(universe_file);

        // Inspect before constructing DevManagerRegistry, whose constructor creates its root.
        const fs::path manager_dir = registry_root / kManagerId;
        const bool manager_exists = fs::exists(manager_dir);
        if (manager_exists && !fs::is_directory(manager_dir))
            throw std::runtime_error("Manager path is not a directory: " + manager_dir.string());

        // Refuse a different pre-existing manager; create_manager() allocates the next ID.
        if (!manager_exists && fs::exists(registry_root)) {
            for (const auto& entry : fs::directory_iterator(registry_root)) {
                if (entry.is_directory() && entry.path().filename().string().rfind("DevManager_", 0) == 0)
                    throw std::runtime_error("Registry already contains a manager; refusing to allocate an unexpected manager ID");
            }
        }

        devai::DevManagerRegistry registry(registry_root);
        if (manager_exists) {
            const auto current = registry.load_manager(kManagerId);
            if (!same_universe(current.symbols, universe.stocks))
                throw std::runtime_error("DevManager_001 already exists with a different universe; no changes made");
            if (current.active) {
                throw std::runtime_error(
                    "DevManager_001 is already registry-active. Refusing to change its state implicitly; "
                    "disable it using the supervised manager lifecycle before proceeding.");
            }
            std::cout << "Existing registration verified: " << kManagerId << '\n';
        } else {
            // Current registry implementation initially marks new managers active.
            // Run only while all CETE trading services are stopped.
            const auto created = registry.create_manager(universe.stocks);
            if (created.manager_id != kManagerId)
                throw std::runtime_error("Unexpected manager ID created: " + created.manager_id);
            registry.set_active(kManagerId, false);
            std::cout << "Registered: " << kManagerId << '\n';
        }

        const auto stored = registry.load_manager(kManagerId);
        if (stored.active || !same_universe(stored.symbols, universe.stocks))
            throw std::runtime_error("Post-registration verification failed");

        std::cout << "Stocks: " << stored.symbols.size() << '\n'
                  << "Benchmark (not registered as a stock): " << kBenchmark << '\n'
                  << "Registry active: NO\n"
                  << "Model trained/approved: NOT VERIFIED\n"
                  << "Trading started: NO\n"
                  << "Registry path: " << manager_dir.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Registration error: " << error.what() << '\n';
        return 1;
    }
}
