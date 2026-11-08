#pragma once

#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include <cstdint>
#include <filesystem>

namespace devai::strategy {

struct DevModelPersistenceMetadata {
    std::uint32_t format_version{0};
    std::uint64_t input_size{0};
    std::uint64_t hidden1_size{0};
    std::uint64_t hidden2_size{0};
    std::uint64_t output_size{0};
    std::uint64_t parameter_count{0};
    std::uint64_t payload_bytes{0};
    std::uint64_t payload_checksum{0};
    bool numerical_integrity{false};
};

class DevModelPersistence {
public:
    static constexpr std::uint32_t FORMAT_VERSION = 1U;

    [[nodiscard]] DevModelPersistenceMetadata save(
        const DevQNetwork& network,
        const std::filesystem::path& path) const;

    [[nodiscard]] DevModelPersistenceMetadata load(
        DevQNetwork& network,
        const std::filesystem::path& path) const;

    [[nodiscard]] DevModelPersistenceMetadata inspect(
        const std::filesystem::path& path) const;
};

} // namespace devai::strategy
