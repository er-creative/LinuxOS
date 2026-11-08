#include "devai/benchmark/DevLegacyCheckpointReader.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace ds = devai::strategy;
namespace db = devai::benchmark;

static_assert(ds::DEV_SHARED_INPUT == 148, "Unexpected model input dimensions");
static_assert(ds::DEV_SHARED_HIDDEN == 64, "Unexpected hidden dimensions");
static_assert(ds::DEV_SHARED_OUTPUT == 7, "Unexpected model output dimensions");

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Fn>
void expectFailure(Fn&& operation, const std::string& label) {
    try {
        operation();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("Expected failure: " + label);
}

bool sameParameters(const ds::DevSharedParameters& a,
                    const ds::DevSharedParameters& b) {
    return a.w1 == b.w1 && a.b1 == b.b1 &&
           a.w2 == b.w2 && a.b2 == b.b2;
}

void testMalformedFixtures(const fs::path& directory) {
    const auto write = [&](const std::string& name, const std::string& contents) {
        const auto path = directory / name;
        std::ofstream out(path, std::ios::trunc);
        require(static_cast<bool>(out), "Cannot create fixture");
        out << contents;
        out.close();
        require(static_cast<bool>(out), "Cannot write fixture");
        return path;
    };

    const auto badMagic = write("bad_magic.chk", "CETE_SHARED_DEVAI_V3_FIVE_MIXED_AGENTS\n");
    expectFailure([&] { (void)db::DevLegacyCheckpointReader::read(badMagic); }, "wrong version");

    const auto badLength = write("bad_length.chk", "CETE_SHARED_DEVAI_V1\n999999\n");
    expectFailure([&] { (void)db::DevLegacyCheckpointReader::read(badLength); }, "wrong length");

    const auto truncated = write("truncated.chk", "CETE_SHARED_DEVAI_V1\n9472\n0.1\n");
    expectFailure([&] { (void)db::DevLegacyCheckpointReader::read(truncated); }, "truncation");

    const auto missing = directory / "does_not_exist.chk";
    expectFailure([&] { (void)db::DevLegacyCheckpointReader::read(missing); }, "missing file");
}

void testFrozen(const fs::path& checkpoint) {
    const auto p = db::DevLegacyCheckpointReader::read(checkpoint);
    require(p.w1.size() == 9472 && p.b1.size() == 64 &&
            p.w2.size() == 448 && p.b2.size() == 7,
            "Unexpected checkpoint dimensions");

    ds::DevSharedNeuralNetwork modelA(73117U);
    ds::DevSharedNeuralNetwork modelB(98765U);
    db::DevLegacyCheckpointReader::loadInto(checkpoint, modelA);
    db::DevLegacyCheckpointReader::loadInto(checkpoint, modelB);
    require(sameParameters(modelA.parameters(), p), "Loaded weights differ");
    require(sameParameters(modelA.parameters(), modelB.parameters()),
            "Loaded weights depend on initialization seed");

    // Synthetic input exercises the current V3 forward pass only. It does NOT
    // certify the historical V1 observation encoder or action meanings.
    for (int trial = 0; trial < 3; ++trial) {
        ds::DevSharedInput input{};
        for (std::size_t i = 0; i < input.values.size(); ++i) {
            input.values[i] = (static_cast<double>(static_cast<int>(i % 13) - 6) / 13.0)
                              * static_cast<double>(trial + 1);
        }
        const auto first = modelA.predict(input);
        const auto second = modelA.predict(input);
        const auto independent = modelB.predict(input);
        for (std::size_t j = 0; j < first.size(); ++j) {
            require(std::isfinite(first[j]), "Nonfinite inference output");
            require(first[j] == second[j] && first[j] == independent[j],
                    "Nondeterministic inference output");
        }
    }
    std::cout << "CETE H1.5 V1 STRUCTURAL LOAD PASS\n"
              << "Checkpoint: " << checkpoint << '\n'
              << "Dimensions: 148 -> 64 -> 7\n"
              << "Deterministic current-network inference: PASS\n"
              << "V1/V3 feature/action semantic equivalence: NOT CERTIFIED\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <frozen_V1_checkpoint>\n";
        return 2;
    }
    try {
        const fs::path checkpoint = argv[1];
        const auto tempDir = fs::temp_directory_path() / "cete_h15_legacy_test_fixtures";
        fs::create_directories(tempDir);
        testMalformedFixtures(tempDir);
        testFrozen(checkpoint);
        std::cout << "Malformed checkpoint rejection: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "CETE H1.5 FAILED: " << e.what() << '\n';
        return 1;
    }
}
