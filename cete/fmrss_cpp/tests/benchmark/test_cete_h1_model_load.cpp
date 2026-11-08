
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace ds = devai::strategy;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <frozen_checkpoint>\n";
        return 2;
    }

    try {
        const fs::path checkpoint = argv[1];

        if (!fs::is_regular_file(checkpoint) ||
            fs::file_size(checkpoint) == 0) {
            throw std::runtime_error(
                "Checkpoint missing or empty");
        }

        ds::DevSharedNeuralNetwork model(73117U);
        model.load(checkpoint);

        const auto& p = model.parameters();

        auto checkVector = [](const auto& v) {
            if (v.empty())
                throw std::runtime_error("Empty parameter vector");

            for (double x : v) {
                if (!std::isfinite(x))
                    throw std::runtime_error(
                        "Nonfinite model parameter");
            }
        };

        checkVector(p.w1);
        checkVector(p.b1);
        checkVector(p.w2);
        checkVector(p.b2);

        std::cout
            << "CETE H1 MODEL LOAD PASS\n"
            << "Checkpoint: " << checkpoint << '\n'
            << "w1: " << p.w1.size() << '\n'
            << "b1: " << p.b1.size() << '\n'
            << "w2: " << p.w2.size() << '\n'
            << "b2: " << p.b2.size() << '\n';

        return 0;

    } catch (const std::exception& e) {
        std::cerr
            << "CETE H1 MODEL LOAD FAILED: "
            << e.what() << '\n';
        return 1;
    }
}
