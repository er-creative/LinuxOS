#include "devai/benchmark/DevLegacyCheckpointReader.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace devai::benchmark {
namespace {

void readVector(std::istream& in, std::vector<double>& values,
                std::size_t expected, const char* name) {
    std::size_t count = 0;
    if (!(in >> count) || count != expected) {
        throw std::runtime_error(std::string("V1 checkpoint: invalid length for ") + name);
    }
    values.resize(count);
    for (double& value : values) {
        if (!(in >> value) || !std::isfinite(value)) {
            throw std::runtime_error(std::string("V1 checkpoint: invalid value in ") + name);
        }
    }
}

} // namespace

devai::strategy::DevSharedParameters DevLegacyCheckpointReader::read(
    const std::filesystem::path& checkpoint) {
    namespace ds = devai::strategy;
    std::ifstream in(checkpoint);
    if (!in) throw std::runtime_error("V1 checkpoint: cannot open file");

    std::string magic;
    if (!(in >> magic) || magic != kMagic) {
        throw std::runtime_error("V1 checkpoint: magic/version mismatch");
    }

    ds::DevSharedParameters p;
    readVector(in, p.w1, ds::DEV_SHARED_HIDDEN * ds::DEV_SHARED_INPUT, "w1");
    readVector(in, p.b1, ds::DEV_SHARED_HIDDEN, "b1");
    readVector(in, p.w2, ds::DEV_SHARED_OUTPUT * ds::DEV_SHARED_HIDDEN, "w2");
    readVector(in, p.b2, ds::DEV_SHARED_OUTPUT, "b2");

    // Strictly reject appended tokens: do not silently accept a mixed or
    // partially corrupted checkpoint.
    std::string trailing;
    if (in >> trailing) {
        throw std::runtime_error("V1 checkpoint: unexpected trailing data");
    }
    if (in.bad()) throw std::runtime_error("V1 checkpoint: read error");
    return p;
}

void DevLegacyCheckpointReader::loadInto(
    const std::filesystem::path& checkpoint,
    devai::strategy::DevSharedNeuralNetwork& network) {
    const auto parameters = read(checkpoint);
    network.setParameters(parameters);
}

} // namespace devai::benchmark
