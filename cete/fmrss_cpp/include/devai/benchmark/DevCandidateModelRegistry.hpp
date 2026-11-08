#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
namespace devai::benchmark {
struct DevFrozenCandidate {
    std::filesystem::path checkpoint;
    std::string sha256;
    std::uintmax_t bytes{};
    std::string source_path;
    std::string source_direction_metadata; // Informational only; NOT certification
};
class DevCandidateModelRegistry {
public:
    [[nodiscard]] static std::string sha256(const std::filesystem::path& path);
    [[nodiscard]] static DevFrozenCandidate freezeCandidate0(
        const std::filesystem::path& source, const std::filesystem::path& directory);
    static void verifyCandidate0(const std::filesystem::path& directory);
};
}
