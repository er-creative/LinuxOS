#pragma once
#include "devai/statistics/MathematicalStateFeatures.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace devai::cache {
class HistoricalMathematicalFeatureCache {
public:
    using State = devai::statistics::MathematicalStateFeatures;
    using States = std::map<std::int64_t, State>;
    explicit HistoricalMathematicalFeatureCache(std::filesystem::path root);
    // Fingerprints the contents of all six stock/benchmark 1m/5m/15m files.
    static std::uint64_t fingerprint(const std::vector<std::filesystem::path>& sources);
    // A hit requires complete coverage of the requested timestamps, including
    // negative/absent states (recorded in the manifest's requested timestamp list).
    bool load(const std::string& stock, const std::string& benchmark,
              std::int64_t end_time, std::uint64_t source_fingerprint,
              const std::vector<std::int64_t>& requested,
              States& out) const;
    void save(const std::string& stock, const std::string& benchmark,
              std::int64_t end_time, std::uint64_t source_fingerprint,
              const std::vector<std::int64_t>& requested,
              const States& states) const;
private:
    std::filesystem::path root_;
    std::filesystem::path pathFor(const std::string& stock, const std::string& benchmark,
                                  std::int64_t end_time) const;
};
}
