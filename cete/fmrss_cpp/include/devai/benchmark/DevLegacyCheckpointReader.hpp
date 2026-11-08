#pragma once

#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"

#include <filesystem>
#include <string_view>

namespace devai::benchmark {

// Structural compatibility only: this reader does NOT certify that V1 and V3
// feature ordering, normalization, action semantics or training match.
class DevLegacyCheckpointReader final {
public:
    static constexpr std::string_view kMagic = "CETE_SHARED_DEVAI_V1";

    [[nodiscard]] static devai::strategy::DevSharedParameters read(
        const std::filesystem::path& checkpoint);

    // Loads only validated V1 parameters into the existing network object.
    // Never modifies the checkpoint or changes V3 save/load behavior.
    static void loadInto(
        const std::filesystem::path& checkpoint,
        devai::strategy::DevSharedNeuralNetwork& network);
};

} // namespace devai::benchmark
