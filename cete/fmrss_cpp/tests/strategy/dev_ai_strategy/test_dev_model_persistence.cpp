#include "devai/strategy/dev_ai_strategy/DevModelPersistence.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using namespace devai::strategy;

struct TestStats { std::size_t checks{0}; std::size_t failures{0}; };

void require(bool condition, const std::string& description, TestStats& stats) {
    ++stats.checks;
    if (condition) std::cout << "[PASS] " << description << '\n';
    else { ++stats.failures; std::cerr << "[FAIL] " << description << '\n'; }
}

template <typename Function>
void expectThrow(Function&& function, const std::string& description, TestStats& stats) {
    bool threw = false;
    try { function(); } catch (const std::exception&) { threw = true; }
    require(threw, description, stats);
}

bool equalParameters(const DevQNetworkParameters& a, const DevQNetworkParameters& b) {
    return a.weights1 == b.weights1 && a.biases1 == b.biases1 &&
           a.weights2 == b.weights2 && a.biases2 == b.biases2 &&
           a.weights3 == b.weights3 && a.biases3 == b.biases3;
}

std::array<double, DEV_Q_INPUT_SIZE> patternedInput() {
    std::array<double, DEV_Q_INPUT_SIZE> input{};
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<double>(static_cast<int>(i % 17U) - 8) / 10.0;
    return input;
}

bool equalQ(const DevQValues& a, const DevQValues& b) {
    return a.numerical_integrity && b.numerical_integrity && a.values == b.values;
}

std::vector<char> readBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Test: unable to read file.");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void writeBytes(const std::filesystem::path& path, const std::vector<char>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Test: unable to write file.");
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
} // namespace

int main() {
    TestStats stats;
    std::cout << "\n======================================================================\n"
              << "DEVAI PHASE 3.9 — MODEL PERSISTENCE TEST\n"
              << "======================================================================\n";

    const auto dir = std::filesystem::temp_directory_path() / "devai_phase_3_9_model_persistence";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir);
    const auto model_path = dir / "dev_q_network.devmodel";

    DevQNetworkConfig source_config; source_config.random_seed = 20261005U;
    DevQNetworkConfig destination_config; destination_config.random_seed = 987654U;
    DevQNetwork source(source_config);
    DevQNetwork destination(destination_config);

    require(source.parameterCount() == 7652U, "Source network has frozen 7,652 parameters", stats);
    require(DEV_Q_INPUT_SIZE == 84U, "Frozen model input remains 84 values", stats);
    require(DEV_Q_OUTPUT_SIZE == 4U, "Frozen model output remains four Dev actions", stats);
    require(!equalParameters(source.parameters(), destination.parameters()),
            "Independent seeds begin with different model parameters", stats);

    const auto input = patternedInput();
    const auto source_q = source.predict(input);
    require(source_q.numerical_integrity, "Source inference is numerically valid before save", stats);

    DevModelPersistence persistence;
    const auto saved = persistence.save(source, model_path);
    require(std::filesystem::exists(model_path), "Model file is created", stats);
    require(std::filesystem::file_size(model_path) > 0U, "Model file is non-empty", stats);
    require(saved.format_version == DevModelPersistence::FORMAT_VERSION, "Saved model records current format version", stats);
    require(saved.input_size == 84U && saved.hidden1_size == 64U && saved.hidden2_size == 32U && saved.output_size == 4U,
            "Saved model records frozen 84-64-32-4 architecture", stats);
    require(saved.parameter_count == 7652U, "Saved model records exact parameter count", stats);
    require(saved.payload_bytes > 0U && saved.payload_checksum != 0U, "Saved model records payload and checksum", stats);
    require(saved.numerical_integrity, "Saved model metadata has numerical integrity", stats);

    const auto inspected = persistence.inspect(model_path);
    require(inspected.payload_checksum == saved.payload_checksum &&
            inspected.parameter_count == saved.parameter_count &&
            inspected.input_size == saved.input_size,
            "Inspect reproduces persisted model metadata", stats);

    const auto loaded = persistence.load(destination, model_path);
    require(loaded.numerical_integrity, "Loaded model has numerical integrity", stats);
    require(equalParameters(source.parameters(), destination.parameters()),
            "Load restores every Q-network parameter exactly", stats);
    require(equalQ(source_q, destination.predict(input)),
            "Loaded model reproduces source Q-values exactly", stats);
    require(source_q.greedyAction() == destination.predict(input).greedyAction(),
            "Loaded model reproduces source greedy action exactly", stats);

    auto modified = source.parameters();
    modified.biases3[0] += 0.125; modified.biases3[1] -= 0.250;
    modified.biases3[2] += 0.375; modified.biases3[3] -= 0.500;
    source.setParameters(modified);
    const auto modified_q = source.predict(input);
    (void)persistence.save(source, model_path);

    DevQNetworkConfig zero_config; zero_config.zero_initialize = true;
    DevQNetwork reloaded(zero_config);
    (void)persistence.load(reloaded, model_path);
    require(equalParameters(source.parameters(), reloaded.parameters()),
            "Overwrite persists current modified model parameters", stats);
    require(equalQ(modified_q, reloaded.predict(input)),
            "Overwritten model reproduces current Q-values", stats);

    expectThrow([&]{ DevQNetwork n; (void)persistence.load(n, dir / "missing.devmodel"); },
                "Missing model file is rejected", stats);

    auto corrupt = readBytes(model_path);
    require(corrupt.size() > 80U, "Saved artifact is large enough for corruption test", stats);
    corrupt.back() = static_cast<char>(static_cast<unsigned char>(corrupt.back()) ^ 0x5AU);
    const auto corrupt_path = dir / "corrupt.devmodel";
    writeBytes(corrupt_path, corrupt);
    DevQNetwork corruption_destination;
    const auto before_corrupt = corruption_destination.parameters();
    expectThrow([&]{ (void)persistence.load(corruption_destination, corrupt_path); },
                "Corrupted model payload is rejected", stats);
    require(equalParameters(before_corrupt, corruption_destination.parameters()),
            "Rejected corrupted load cannot partially mutate destination network", stats);

    auto truncated = readBytes(model_path);
    truncated.resize(truncated.size() / 2U);
    const auto truncated_path = dir / "truncated.devmodel";
    writeBytes(truncated_path, truncated);
    expectThrow([&]{ (void)persistence.inspect(truncated_path); },
                "Truncated model file is rejected", stats);

    auto bad_magic = readBytes(model_path);
    bad_magic[0] = 'X';
    const auto bad_magic_path = dir / "bad_magic.devmodel";
    writeBytes(bad_magic_path, bad_magic);
    expectThrow([&]{ (void)persistence.inspect(bad_magic_path); },
                "Invalid model-file magic is rejected", stats);

    auto bad_version = readBytes(model_path);
    const std::uint32_t unsupported = DevModelPersistence::FORMAT_VERSION + 1U;
    std::memcpy(bad_version.data() + 8, &unsupported, sizeof(unsupported));
    const auto bad_version_path = dir / "bad_version.devmodel";
    writeBytes(bad_version_path, bad_version);
    expectThrow([&]{ (void)persistence.inspect(bad_version_path); },
                "Unsupported model format version is rejected", stats);

    expectThrow([&]{ (void)persistence.save(source, {}); }, "Empty save path is rejected", stats);
    require(equalParameters(modified, source.parameters()), "Persistence operations never mutate source network", stats);

    std::filesystem::remove_all(dir, ec);
    std::cout << "\n======================================================================\n"
              << "PHASE 3.9 MODEL PERSISTENCE TEST SUMMARY\n"
              << "======================================================================\n"
              << "Checks   : " << stats.checks << '\n'
              << "Failures : " << stats.failures << '\n'
              << "======================================================================\n";
    if (stats.failures == 0U) {
        std::cout << "DEVAI PHASE 3.9 MODEL PERSISTENCE: PASSED\n";
        return 0;
    }
    std::cout << "DEVAI PHASE 3.9 MODEL PERSISTENCE: FAILED\n";
    return 1;
}
