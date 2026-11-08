// CETE H4.7 Stage 5E.2I: audit top-five assignment stability within
// Asia/Kolkata (IST, UTC+05:30) 30-minute blocks anchored at 09:15 IST.
// Build: g++ -std=c++17 -O2 -Wall -Wextra -pedantic audit_ranking_cadence.cpp -o audit_ranking_cadence
// Run:   ./audit_ranking_cadence output/h4_7/stage5e2h/ranking_audit.log
// Exit:  0 = pass, 1 = cadence violation, 2 = invalid input/incomplete data.
// This audits a text log; it does not modify CETE ranking or RL behavior.
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {
constexpr std::int64_t kIstOffsetSeconds = 5 * 3600 + 30 * 60;
constexpr std::int64_t kBlockSeconds = 30 * 60;
constexpr std::int64_t kDaySeconds = 86400;
constexpr std::int64_t kSessionOpen = 9 * 3600 + 15 * 60;
constexpr std::int64_t kSessionClose = 15 * 3600 + 15 * 60;
using Symbols = std::array<std::string, 5>;
struct Row { int rank; int agent; std::string symbol; };
struct Block { std::int64_t first_epoch; Symbols symbols; };

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    auto q = a / b;
    if (a % b < 0) --q;
    return q;
}

std::string show(const Symbols& symbols) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i) out << ", ";
        out << symbols[i];
    }
    return out.str() + ']';
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <ranking_audit.log>\n";
        return 2;
    }
    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "ERROR: cannot open log: " << argv[1] << '\n';
        return 2;
    }
    const std::regex header(R"(^Decision epoch:\s*([0-9]+)\s*\|)");
    const std::regex row(R"(^\s*([1-5])\s+DevAgent_([0-9]{3})\s+(\S+)\s+([-+]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*$)");
    std::map<std::int64_t, std::vector<Row>> records;
    std::int64_t current = 0;
    bool in_record = false;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        std::smatch match;
        if (std::regex_search(line, match, header)) {
            try {
                current = std::stoll(match[1].str());
            } catch (const std::exception&) {
                std::cerr << "ERROR: invalid epoch on line " << line_number << '\n';
                return 2;
            }
            if (!records.emplace(current, std::vector<Row>{}).second) {
                std::cerr << "ERROR: repeated decision epoch " << current << '\n';
                return 2;
            }
            in_record = true;
        } else if (in_record && std::regex_match(line, match, row)) {
            records.at(current).push_back({std::stoi(match[1].str()),
                                           std::stoi(match[2].str()), match[3].str()});
        }
    }
    if (records.empty()) {
        std::cerr << "ERROR: no ranking decision records found\n";
        return 2;
    }

    std::map<std::int64_t, Symbols> complete;
    for (const auto& [epoch, rows] : records) {
        if (rows.size() != 5) continue;
        Symbols symbols{};
        std::set<std::string> unique;
        bool valid = true;
        for (const auto& r : rows) {
            if (r.rank < 1 || r.rank > 5 || r.agent != r.rank ||
                !symbols[static_cast<std::size_t>(r.rank - 1)].empty()) {
                valid = false;
                break;
            }
            symbols[static_cast<std::size_t>(r.rank - 1)] = r.symbol;
            unique.insert(r.symbol);
        }
        if (valid && unique.size() == 5) complete.emplace(epoch, symbols);
    }
    if (complete.size() != records.size()) {
        std::cerr << "ERROR: " << records.size() - complete.size()
                  << " incomplete/invalid ranking record(s); audit cannot pass\n";
        return 2;
    }

    std::map<std::int64_t, Block> blocks;
    std::size_t changes = 0, violations = 0;
    Symbols previous{};
    bool have_previous = false;
    for (const auto& [epoch, symbols] : complete) {
        // Match the production controller: 09:15 IST session anchor.
        const auto local = epoch + kIstOffsetSeconds;
        const auto day = floor_div(local, kDaySeconds);
        const auto seconds = local - day * kDaySeconds;
        if (epoch % 60 != 0 || seconds < kSessionOpen || seconds > kSessionClose) {
            std::cerr << "ERROR: decision outside minute-aligned IST session: " << epoch << '\n';
            return 2;
        }
        const auto elapsed = seconds - kSessionOpen;
        const auto slot = elapsed == kSessionClose - kSessionOpen ? elapsed - 1 : elapsed;
        const auto key = day * 100 + slot / kBlockSeconds;
        const auto [it, inserted] = blocks.emplace(key, Block{epoch, symbols});
        if (!inserted && it->second.symbols != symbols) {
            ++violations;
            if (violations <= 8) {
                std::cout << "  VIOLATION epoch=" << epoch
                          << " block_first_epoch=" << it->second.first_epoch
                          << "\n    expected " << show(it->second.symbols)
                          << "\n    observed " << show(symbols) << '\n';
            }
        }
        if (have_previous && previous != symbols) ++changes;
        previous = symbols;
        have_previous = true;
    }
    std::cout << "Complete decisions: " << complete.size()
              << "; 30-minute IST blocks: " << blocks.size()
              << "; consecutive changes: " << changes << '\n';
    std::cout << "Intra-block ranking changes: " << violations << '\n';
    if (violations) {
        std::cout << "FAIL: top-five assignments changed inside a 30-minute IST block\n";
        return 1;
    }
    std::cout << "PASS: top-five assignments stable within each observed 30-minute IST block\n";
    return 0;
}
