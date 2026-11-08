#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
using namespace devai::strategy;
int main() {
    DevSharedNeuralNetwork shared(42);
    DevSharedInput agent{};
    agent.values[DEV_Q_INPUT_SIZE+DEV_MANAGER_INPUT_SIZE]=1.0;
    DevSharedInput manager{};
    manager.values[DEV_Q_INPUT_SIZE+DEV_MANAGER_INPUT_SIZE+1]=1.0;
    const auto beforeAgent=shared.predict(agent);
    const auto beforeManager=shared.predict(manager);
    const auto before=shared.parameters().w1;
    const auto loss=shared.train(agent,2,1.0,0.001);
    assert(std::isfinite(loss));
    assert(shared.parameters().w1!=before || shared.predict(agent)[2]!=beforeAgent[2]);
    const auto afterManager=shared.predict(manager);
    // Same model is used for both roles; an agent update can influence manager outputs.
    for (double x:afterManager) assert(std::isfinite(x));
    const auto path=std::filesystem::temp_directory_path()/"cete_shared_alignment_test.chk";
    shared.save(path);
    DevSharedNeuralNetwork restored(123);
    restored.load(path);
    assert(restored.predict(agent)==shared.predict(agent));
    assert(restored.predict(manager)==shared.predict(manager));
    std::filesystem::remove(path);
    std::cout<<"CETE shared DevAI neural network foundation: PASSED\n";
}
