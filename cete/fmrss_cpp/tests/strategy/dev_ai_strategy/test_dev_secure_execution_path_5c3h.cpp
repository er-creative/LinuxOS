#include "devai/execution/DevSecureExecutionPath.hpp"
#include "devai/approval/DevModelApprovalVerifier.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include "cete/supervision/DevinderjeetSupervisor.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "cete_5c3h_secure_execution_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root / "registry");
        auto manager = registry.create_manager({"RELIANCE", "HDFCBANK"});
        registry.set_active(manager.manager_id, false);
        cete::supervision::DevinderjeetSupervisor central;
        // No trusted approval authority is supplied: fail closed.
        devai::approval::DevModelApprovalVerifier verifier(
            root / "approvals", root / "models", root / "oos", {});
        devai::runtime::DevApprovedRuntimeGateway gateway(registry, central, verifier);
        int accepted = 0, rejected = 0;
        devai::execution::DevSecureExecutionPath path(gateway,
            [&](const std::string&, const std::string&, bool ok) {
                if (ok) ++accepted; else ++rejected;
            });
        assert(!path.eligible(manager.manager_id, "RELIANCE"));
        assert(!path.eligible(manager.manager_id, "NIFTY%2050"));
        cete::supervision::DevSupervisedOrder order{};
        order.manager_id = manager.manager_id;
        order.symbol = "RELIANCE";
        bool blocked = false;
        try { (void)path.submit(order); }
        catch (const std::runtime_error&) { blocked = true; }
        assert(blocked);
        assert(accepted == 0 && rejected == 1);
        fs::remove_all(root);
        std::cout << "CETE 5C-3H secure execution path PASS\n";
    } catch (...) {
        fs::remove_all(root);
        throw;
    }
}
