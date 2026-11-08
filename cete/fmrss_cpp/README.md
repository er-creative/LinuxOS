# CETE H4.7 Stage 5E.2B — C++ only

Complete replacement C++ files based on the uploaded source archive. No Python scripts.
Includes full PPO header/source with read-only policyLogits(), model adapter,
five-agent bridge and regression test. No model checkpoint or live trading is enabled.

## Install
1. Back up existing PPO header/source, 5E2B headers/test and CMakeLists.txt.
2. From the project root, run:
   unzip -o ~/Downloads/CETE_H47_Stage5E2B_CPP_ONLY.zip -d .
3. CMake: if an existing test_dev_selected_rl_inference_h47_5e2b target exists,
   ADD src/benchmark/DevMaskablePPO.cpp to its add_executable source list.
   Otherwise append CMake_STAGE5E2B_APPEND.txt contents once.
4. cmake -S . -B build -G Ninja
5. cmake --build build --target test_dev_selected_rl_inference_h47_5e2b -j 2
6. ctest --test-dir build -R '^dev_selected_rl_inference_h47_5e2b$' --output-on-failure

The regression test includes a real C++ PPO forward pass with RANDOM INITIAL WEIGHTS,
not a trained checkpoint. Production use requires validated checkpoint loading,
causal features, masks, model selection, and manager execution gates.
