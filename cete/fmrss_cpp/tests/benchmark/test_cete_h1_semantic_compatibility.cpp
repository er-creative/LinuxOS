#include "devai/benchmark/DevSemanticCompatibilityAudit.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <frozen_V1_checkpoint>\n";
        return 2;
    }
    const auto r = devai::benchmark::DevSemanticCompatibilityAudit::run(argv[1]);
    std::cout << "CETE H1.6 SEMANTIC COMPATIBILITY AUDIT\n"
              << "V1 structural validation: " << (r.checkpoint_structure_valid ? "PASS" : "FAIL") << '\n'
              << "Current feature layout: " << (r.current_layout_valid ? "PASS" : "FAIL") << '\n'
              << "Current manager action mapping: " << (r.manager_action_mapping_valid ? "PASS" : "FAIL") << '\n'
              << "Current deterministic inference: " << (r.deterministic_inference ? "PASS" : "FAIL") << '\n'
              << "Historical V1/V3 semantic equivalence: "
              << (r.historical_semantics_certified ? "CERTIFIED" : "NOT CERTIFIED") << '\n'
              << "Detail: " << r.diagnostic << '\n';
    return r.checkpoint_structure_valid && r.current_layout_valid &&
                   r.manager_action_mapping_valid && r.deterministic_inference &&
                   !r.historical_semantics_certified ? 0 : 1;
}
