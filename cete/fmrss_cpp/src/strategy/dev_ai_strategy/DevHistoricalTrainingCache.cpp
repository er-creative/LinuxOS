#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"

#include <cmath>
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace devai::strategy
{
namespace
{
constexpr char MAGIC[8] = {'D','E','V','H','C','A','C','3'};
constexpr std::uint64_t MAX_STRING = 1ULL << 20;
constexpr std::uint64_t MAX_RECORDS = 100000000ULL;
constexpr std::uint64_t MAX_OUTCOMES = 1024ULL;

bool finite(double v) noexcept { return std::isfinite(v); }

template <typename T>
void writePod(std::ostream& out, const T& value)
{
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!out) throw std::runtime_error("Historical cache write failed.");
}

template <typename T>
T readPod(std::istream& in)
{
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) throw std::runtime_error("Historical cache is truncated/corrupt.");
    return value;
}

void writeBool(std::ostream& out, bool v) { const std::uint8_t x = v ? 1U : 0U; writePod(out, x); }
bool readBool(std::istream& in)
{
    const auto x = readPod<std::uint8_t>(in);
    if (x > 1U) throw std::runtime_error("Historical cache contains invalid boolean.");
    return x == 1U;
}

void writeString(std::ostream& out, const std::string& s)
{
    writePod(out, static_cast<std::uint64_t>(s.size()));
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
    if (!out) throw std::runtime_error("Historical cache string write failed.");
}

std::string readString(std::istream& in)
{
    const auto n = readPod<std::uint64_t>(in);
    if (n > MAX_STRING) throw std::runtime_error("Historical cache string is unreasonable.");
    std::string s(static_cast<std::size_t>(n), '\0');
    in.read(s.data(), static_cast<std::streamsize>(n));
    if (!in) throw std::runtime_error("Historical cache string is truncated.");
    return s;
}

void writeObservation(std::ostream& out, const DevAIObservation& o)
{
    writeString(out, o.agent_name);
    writeString(out, o.symbol);
    writeString(out, o.benchmark_symbol);
    writePod(out, o.decision_time);
    for (double v : o.market) writePod(out, v);
    for (bool v : o.availability) writeBool(out, v);
    writePod(out, o.portfolio.position);
    writePod(out, o.portfolio.unrealized_return);
    writePod(out, o.portfolio.realized_return);
    writePod(out, o.portfolio.equity_return);
    writePod(out, o.portfolio.drawdown);
    writePod(out, o.portfolio.holding_period);
    writeBool(out, o.numerical_integrity);
    writeBool(out, o.ready);
}

DevAIObservation readObservation(std::istream& in)
{
    DevAIObservation o{};
    o.agent_name = readString(in);
    o.symbol = readString(in);
    o.benchmark_symbol = readString(in);
    o.decision_time = readPod<std::int64_t>(in);
    for (double& v : o.market) v = readPod<double>(in);
    for (auto& v : o.availability) v = readBool(in);
    o.portfolio.position = readPod<double>(in);
    o.portfolio.unrealized_return = readPod<double>(in);
    o.portfolio.realized_return = readPod<double>(in);
    o.portfolio.equity_return = readPod<double>(in);
    o.portfolio.drawdown = readPod<double>(in);
    o.portfolio.holding_period = readPod<double>(in);
    o.numerical_integrity = readBool(in);
    o.ready = readBool(in);
    return o;
}

void validateObservation(const DevAIObservation& o,
                         std::int64_t t,
                         const std::string& benchmark,
                         const std::string& symbol)
{
    if (!o.ready || !o.numerical_integrity)
        throw std::invalid_argument("Cached observation must be ready and numerically valid.");
    if (o.decision_time != t || o.symbol != symbol || o.benchmark_symbol != benchmark)
        throw std::invalid_argument("Cached observation identity/time mismatch.");
    for (double v : o.market)
        if (!finite(v)) throw std::invalid_argument("Cached observation contains non-finite market value.");
    const double p[] = {o.portfolio.position, o.portfolio.unrealized_return,
                        o.portfolio.realized_return, o.portfolio.equity_return,
                        o.portfolio.drawdown, o.portfolio.holding_period};
    for (double v : p)
        if (!finite(v)) throw std::invalid_argument("Cached observation contains non-finite portfolio value.");
}
} // namespace

void DevHistoricalTrainingDatasetValidator::validateCandidate(
    const DevHistoricalCandidateRecord& c,
    std::int64_t t,
    const std::string& benchmark,
    std::size_t expected_slot)
{
    if (!c.numerical_integrity) throw std::invalid_argument("Candidate integrity is false.");
    if (c.slot_index != expected_slot || c.slot_index >= DEV_AGENT_SLOT_COUNT)
        throw std::invalid_argument("Historical candidate slots must be contiguous from zero.");
    if (c.agent_name != DevTopCandidateSelector::agentNames().at(c.slot_index))
        throw std::invalid_argument("Historical candidate agent identity mismatch.");
    if (c.symbol.empty() || c.symbol == benchmark)
        throw std::invalid_argument("Historical candidate symbol is invalid.");
    if (!finite(c.candidate_score)) throw std::invalid_argument("Historical candidate score is non-finite.");
    if (c.candidate_rank != c.slot_index + 1U)
        throw std::invalid_argument("Historical candidate rank does not match slot.");
    validateObservation(c.observation, t, benchmark, c.symbol);

    std::unordered_set<std::size_t> horizons;
    for (const auto& o : c.outcomes)
    {
        if (o.horizon_bars == 0 || !horizons.insert(o.horizon_bars).second)
            throw std::invalid_argument("Historical outcome horizon is invalid/duplicated.");
        if (!o.numerical_integrity)
            throw std::invalid_argument("Historical outcome integrity is false.");
        if (o.available)
        {
            if (o.outcome_time <= t || !finite(o.forward_return))
                throw std::invalid_argument("Available future outcome is invalid.");
        }
        else if (o.outcome_time != 0 || o.forward_return != 0.0)
            throw std::invalid_argument("Unavailable outcome must use zero placeholders.");
    }
}

void DevHistoricalTrainingDatasetValidator::validateRecord(const DevHistoricalDecisionRecord& r)
{
    if (!r.numerical_integrity || r.decision_time <= 0 || r.benchmark_symbol.empty())
        throw std::invalid_argument("Historical decision record identity/integrity is invalid.");
    if (r.candidates.size() > DEV_AGENT_SLOT_COUNT)
        throw std::invalid_argument("Historical decision record contains more than five candidates.");
    if (r.universe_stock_count < r.candidates.size())
        throw std::invalid_argument("Universe count cannot be smaller than candidate count.");

    std::unordered_set<std::string> symbols;
    for (std::size_t i = 0; i < r.candidates.size(); ++i)
    {
        validateCandidate(r.candidates[i], r.decision_time, r.benchmark_symbol, i);
        if (!symbols.insert(r.candidates[i].symbol).second)
            throw std::invalid_argument("Duplicate historical candidate symbol.");
    }
}

void DevHistoricalTrainingDatasetValidator::validate(const DevHistoricalTrainingDataset& d)
{
    if (d.format_version != DEV_HISTORICAL_CACHE_FORMAT_VERSION || !d.numerical_integrity)
        throw std::invalid_argument("Historical training dataset metadata/integrity is invalid.");
    std::int64_t previous = 0;
    std::string benchmark;
    for (const auto& r : d.records)
    {
        validateRecord(r);
        if (r.decision_time <= previous) throw std::invalid_argument("Historical records are not strictly chronological.");
        if (benchmark.empty()) benchmark = r.benchmark_symbol;
        if (r.benchmark_symbol != benchmark) throw std::invalid_argument("Historical dataset benchmark changed.");
        previous = r.decision_time;
    }
}

void DevHistoricalTrainingCache::save(const std::filesystem::path& path,
                                      const DevHistoricalTrainingDataset& d)
{
    DevHistoricalTrainingDatasetValidator::validate(d);
    if (path.empty()) throw std::invalid_argument("Historical cache path is empty.");
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    auto tmp = path; tmp += ".tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open historical cache temporary file.");
    out.write(MAGIC, sizeof(MAGIC));
    writePod(out, d.format_version);
    writeBool(out, d.numerical_integrity);
    writePod(out, static_cast<std::uint64_t>(d.records.size()));
    for (const auto& r : d.records)
    {
        writePod(out, r.decision_time); writeString(out, r.benchmark_symbol);
        writePod(out, static_cast<std::uint64_t>(r.universe_stock_count));
        writeBool(out, r.numerical_integrity);
        writePod(out, static_cast<std::uint64_t>(r.candidates.size()));
        for (const auto& c : r.candidates)
        {
            writePod(out, static_cast<std::uint64_t>(c.slot_index));
            writeString(out, c.agent_name); writeString(out, c.symbol);
            writePod(out, c.candidate_score);
            writePod(out, static_cast<std::uint64_t>(c.candidate_rank));
            writeBool(out, c.numerical_integrity); writeObservation(out, c.observation);
            writePod(out, static_cast<std::uint64_t>(c.outcomes.size()));
            for (const auto& o : c.outcomes)
            {
                writePod(out, static_cast<std::uint64_t>(o.horizon_bars));
                writePod(out, o.outcome_time); writePod(out, o.forward_return);
                writeBool(out, o.available); writeBool(out, o.numerical_integrity);
            }
        }
    }
    out.flush(); if (!out) throw std::runtime_error("Historical cache flush failed."); out.close();
    std::error_code ec; std::filesystem::remove(path, ec); ec.clear();
    std::filesystem::rename(tmp, path, ec);
    if (ec) { std::filesystem::remove(tmp); throw std::runtime_error("Historical cache publish failed: " + ec.message()); }
}

DevHistoricalTrainingDataset DevHistoricalTrainingCache::load(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open historical cache.");
    char magic[8]{}; in.read(magic, sizeof(magic));
    if (!in || !std::equal(std::begin(magic), std::end(magic), std::begin(MAGIC)))
        throw std::runtime_error("Historical cache magic mismatch.");
    DevHistoricalTrainingDataset d{};
    d.format_version = readPod<std::uint32_t>(in); d.numerical_integrity = readBool(in);
    const auto nr = readPod<std::uint64_t>(in);
    if (nr > MAX_RECORDS) throw std::runtime_error("Historical cache record count is unreasonable.");
    d.records.reserve(static_cast<std::size_t>(nr));
    for (std::uint64_t ri=0; ri<nr; ++ri)
    {
        DevHistoricalDecisionRecord r{};
        r.decision_time=readPod<std::int64_t>(in); r.benchmark_symbol=readString(in);
        r.universe_stock_count=static_cast<std::size_t>(readPod<std::uint64_t>(in));
        r.numerical_integrity=readBool(in);
        const auto nc=readPod<std::uint64_t>(in);
        if (nc>DEV_AGENT_SLOT_COUNT) throw std::runtime_error("Historical cache candidate count exceeds five.");
        for (std::uint64_t ci=0; ci<nc; ++ci)
        {
            DevHistoricalCandidateRecord c{};
            c.slot_index=static_cast<std::size_t>(readPod<std::uint64_t>(in));
            c.agent_name=readString(in); c.symbol=readString(in); c.candidate_score=readPod<double>(in);
            c.candidate_rank=static_cast<std::size_t>(readPod<std::uint64_t>(in));
            c.numerical_integrity=readBool(in); c.observation=readObservation(in);
            const auto no=readPod<std::uint64_t>(in);
            if (no>MAX_OUTCOMES) throw std::runtime_error("Historical cache outcome count is unreasonable.");
            for (std::uint64_t oi=0; oi<no; ++oi)
            {
                DevHistoricalOutcome o{};
                o.horizon_bars=static_cast<std::size_t>(readPod<std::uint64_t>(in));
                o.outcome_time=readPod<std::int64_t>(in); o.forward_return=readPod<double>(in);
                o.available=readBool(in); o.numerical_integrity=readBool(in); c.outcomes.push_back(o);
            }
            r.candidates.push_back(std::move(c));
        }
        d.records.push_back(std::move(r));
    }
    if (in.peek() != std::char_traits<char>::eof()) throw std::runtime_error("Historical cache has trailing bytes.");
    DevHistoricalTrainingDatasetValidator::validate(d);
    return d;
}

} // namespace devai::strategy
