#include "devai/strategy/dev_ai_strategy/DevModelPersistence.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

namespace devai::strategy {
namespace {

constexpr std::array<char, 8> MAGIC{'D','E','V','Q','M','O','D','1'};
constexpr std::uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;
constexpr std::uint64_t EXPECTED_PARAMETER_COUNT = 7652ULL;

template <typename T>
void appendScalar(std::vector<std::uint8_t>& bytes, const T value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

template <typename T>
T readScalar(const std::vector<std::uint8_t>& bytes, std::size_t& cursor) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (cursor > bytes.size() || bytes.size() - cursor < sizeof(T))
        throw std::runtime_error("DevModelPersistence: truncated model payload.");
    T value{};
    std::memcpy(&value, bytes.data() + cursor, sizeof(T));
    cursor += sizeof(T);
    return value;
}

std::uint64_t checksum(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t value = FNV_OFFSET;
    for (const auto byte : bytes) {
        value ^= static_cast<std::uint64_t>(byte);
        value *= FNV_PRIME;
    }
    return value;
}

bool finiteVector(const std::vector<double>& values) {
    for (const double value : values)
        if (!std::isfinite(value)) return false;
    return true;
}

bool finiteParameters(const DevQNetworkParameters& p) {
    return finiteVector(p.weights1) && finiteVector(p.biases1) &&
           finiteVector(p.weights2) && finiteVector(p.biases2) &&
           finiteVector(p.weights3) && finiteVector(p.biases3);
}

void appendVector(std::vector<std::uint8_t>& bytes, const std::vector<double>& values) {
    appendScalar<std::uint64_t>(bytes, static_cast<std::uint64_t>(values.size()));
    for (const double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("DevModelPersistence: model contains non-finite parameter.");
        appendScalar<double>(bytes, value);
    }
}

std::vector<double> readVector(const std::vector<std::uint8_t>& bytes,
                               std::size_t& cursor,
                               const std::size_t expected,
                               const char* name) {
    const auto stored = readScalar<std::uint64_t>(bytes, cursor);
    if (stored != expected)
        throw std::runtime_error(std::string("DevModelPersistence: invalid ") + name + " dimension.");
    std::vector<double> values(expected);
    for (double& value : values) {
        value = readScalar<double>(bytes, cursor);
        if (!std::isfinite(value))
            throw std::runtime_error(std::string("DevModelPersistence: non-finite ") + name + " parameter.");
    }
    return values;
}

std::vector<std::uint8_t> serialize(const DevQNetworkParameters& p) {
    if (!finiteParameters(p))
        throw std::invalid_argument("DevModelPersistence: model parameters are not finite.");
    std::vector<std::uint8_t> bytes;
    bytes.reserve(6U * sizeof(std::uint64_t) + EXPECTED_PARAMETER_COUNT * sizeof(double));
    appendVector(bytes, p.weights1); appendVector(bytes, p.biases1);
    appendVector(bytes, p.weights2); appendVector(bytes, p.biases2);
    appendVector(bytes, p.weights3); appendVector(bytes, p.biases3);
    return bytes;
}

DevQNetworkParameters deserialize(const std::vector<std::uint8_t>& bytes) {
    std::size_t cursor = 0;
    DevQNetworkParameters p;
    p.weights1 = readVector(bytes, cursor, DEV_Q_HIDDEN1_SIZE * DEV_Q_INPUT_SIZE, "weights1");
    p.biases1  = readVector(bytes, cursor, DEV_Q_HIDDEN1_SIZE, "biases1");
    p.weights2 = readVector(bytes, cursor, DEV_Q_HIDDEN2_SIZE * DEV_Q_HIDDEN1_SIZE, "weights2");
    p.biases2  = readVector(bytes, cursor, DEV_Q_HIDDEN2_SIZE, "biases2");
    p.weights3 = readVector(bytes, cursor, DEV_Q_OUTPUT_SIZE * DEV_Q_HIDDEN2_SIZE, "weights3");
    p.biases3  = readVector(bytes, cursor, DEV_Q_OUTPUT_SIZE, "biases3");
    if (cursor != bytes.size())
        throw std::runtime_error("DevModelPersistence: unexpected trailing payload bytes.");
    return p;
}

template <typename T>
void writeBinary(std::ostream& stream, const T value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!stream) throw std::runtime_error("DevModelPersistence: failed writing model file.");
}

template <typename T>
T readBinary(std::istream& stream) {
    T value{};
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!stream) throw std::runtime_error("DevModelPersistence: truncated model header.");
    return value;
}

struct ModelFile {
    DevModelPersistenceMetadata metadata{};
    std::vector<std::uint8_t> payload;
};

ModelFile readModelFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("DevModelPersistence: unable to open model file: " + path.string());

    std::array<char, MAGIC.size()> magic{};
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!stream || magic != MAGIC)
        throw std::runtime_error("DevModelPersistence: invalid model-file magic.");

    ModelFile file;
    file.metadata.format_version = readBinary<std::uint32_t>(stream);
    const auto reserved = readBinary<std::uint32_t>(stream);
    file.metadata.input_size = readBinary<std::uint64_t>(stream);
    file.metadata.hidden1_size = readBinary<std::uint64_t>(stream);
    file.metadata.hidden2_size = readBinary<std::uint64_t>(stream);
    file.metadata.output_size = readBinary<std::uint64_t>(stream);
    file.metadata.parameter_count = readBinary<std::uint64_t>(stream);
    file.metadata.payload_bytes = readBinary<std::uint64_t>(stream);
    file.metadata.payload_checksum = readBinary<std::uint64_t>(stream);

    if (reserved != 0U)
        throw std::runtime_error("DevModelPersistence: invalid reserved header field.");
    if (file.metadata.format_version != DevModelPersistence::FORMAT_VERSION)
        throw std::runtime_error("DevModelPersistence: unsupported model format version.");
    if (file.metadata.input_size != DEV_Q_INPUT_SIZE ||
        file.metadata.hidden1_size != DEV_Q_HIDDEN1_SIZE ||
        file.metadata.hidden2_size != DEV_Q_HIDDEN2_SIZE ||
        file.metadata.output_size != DEV_Q_OUTPUT_SIZE ||
        file.metadata.parameter_count != EXPECTED_PARAMETER_COUNT)
        throw std::runtime_error("DevModelPersistence: model architecture does not match frozen Dev Q-network.");
    if (file.metadata.payload_bytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        throw std::runtime_error("DevModelPersistence: payload is too large.");

    file.payload.resize(static_cast<std::size_t>(file.metadata.payload_bytes));
    if (!file.payload.empty()) {
        stream.read(reinterpret_cast<char*>(file.payload.data()),
                    static_cast<std::streamsize>(file.payload.size()));
        if (!stream) throw std::runtime_error("DevModelPersistence: truncated model payload.");
    }

    char trailing{};
    if (stream.read(&trailing, 1))
        throw std::runtime_error("DevModelPersistence: unexpected trailing file bytes.");
    if (!stream.eof())
        throw std::runtime_error("DevModelPersistence: model-file read failure.");
    if (checksum(file.payload) != file.metadata.payload_checksum)
        throw std::runtime_error("DevModelPersistence: payload checksum mismatch.");

    const auto parameters = deserialize(file.payload);
    file.metadata.numerical_integrity = finiteParameters(parameters);
    if (!file.metadata.numerical_integrity)
        throw std::runtime_error("DevModelPersistence: model failed numerical-integrity validation.");
    return file;
}

} // namespace

DevModelPersistenceMetadata DevModelPersistence::save(
    const DevQNetwork& network,
    const std::filesystem::path& path) const {
    if (path.empty())
        throw std::invalid_argument("DevModelPersistence: save path must not be empty.");
    if (network.parameterCount() != EXPECTED_PARAMETER_COUNT)
        throw std::runtime_error("DevModelPersistence: unexpected Dev Q-network parameter count.");

    const auto payload = serialize(network.parameters());
    DevModelPersistenceMetadata m;
    m.format_version = FORMAT_VERSION;
    m.input_size = DEV_Q_INPUT_SIZE;
    m.hidden1_size = DEV_Q_HIDDEN1_SIZE;
    m.hidden2_size = DEV_Q_HIDDEN2_SIZE;
    m.output_size = DEV_Q_OUTPUT_SIZE;
    m.parameter_count = network.parameterCount();
    m.payload_bytes = payload.size();
    m.payload_checksum = checksum(payload);
    m.numerical_integrity = true;

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) throw std::runtime_error("DevModelPersistence: unable to create model directory: " + ec.message());
    }

    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) throw std::runtime_error("DevModelPersistence: unable to create temporary model file.");
        stream.write(MAGIC.data(), static_cast<std::streamsize>(MAGIC.size()));
        if (!stream) throw std::runtime_error("DevModelPersistence: failed writing model magic.");
        writeBinary(stream, m.format_version); writeBinary<std::uint32_t>(stream, 0U);
        writeBinary(stream, m.input_size); writeBinary(stream, m.hidden1_size);
        writeBinary(stream, m.hidden2_size); writeBinary(stream, m.output_size);
        writeBinary(stream, m.parameter_count); writeBinary(stream, m.payload_bytes);
        writeBinary(stream, m.payload_checksum);
        stream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        stream.flush();
        if (!stream) throw std::runtime_error("DevModelPersistence: failed writing model payload.");
    }

    (void)readModelFile(temporary);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("DevModelPersistence: unable to publish model file: " + ec.message());
    }
    return m;
}

DevModelPersistenceMetadata DevModelPersistence::load(
    DevQNetwork& network,
    const std::filesystem::path& path) const {
    const auto file = readModelFile(path);
    network.setParameters(deserialize(file.payload));
    return file.metadata;
}

DevModelPersistenceMetadata DevModelPersistence::inspect(
    const std::filesystem::path& path) const {
    return readModelFile(path).metadata;
}

} // namespace devai::strategy
