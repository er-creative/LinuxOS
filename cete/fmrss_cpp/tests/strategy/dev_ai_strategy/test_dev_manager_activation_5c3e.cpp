#include "devai/activation/DevManagerActivationController.hpp"
#include "cete/supervision/DevinderjeetSupervisor.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "cete_5c3e_activation_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root);
        const auto m = registry.create_manager({"RELIANCE", "HDFCBANK"});
        registry.set_active(m.manager_id, false);
        cete::supervision::DevinderjeetSupervisor central;
        central.registerManager({m.manager_id,
            {"DevAgent_001", "DevAgent_002", "DevAgent_003", "DevAgent_004", "DevAgent_005"}, 1000.0});
        bool authorize = true;
        devai::strategy::DevAISupervisor supervisor(registry, central,
            [&](const std::string&, const std::string&) { return authorize; });
        bool approved = false;
        bool enabled = true;
        devai::activation::DevManagerActivationController controller(
            supervisor, registry,
            [&](const std::string& id, const std::vector<std::string>& symbols) {
                return approved && id == m.manager_id && symbols == m.symbols;
            },
            [&](const std::string& id) { return enabled && id == m.manager_id; });
        auto denied = [&]() {
            try { controller.activate(m.manager_id); return false; }
            catch (const std::exception&) { return !registry.load_manager(m.manager_id).active; }
        };
        assert(denied()); // no model approval
        approved = true;
        enabled = false;
        assert(denied()); // configuration disabled
        enabled = true;
        authorize = false;
        assert(denied()); // supervisor authorization denied
        authorize = true;
        controller.activate(m.manager_id);
        assert(registry.load_manager(m.manager_id).active);
        controller.deactivate(m.manager_id);
        assert(!registry.load_manager(m.manager_id).active);
        // No checker provided: fail closed even if manager exists.
        devai::activation::DevManagerActivationController default_denied(supervisor, registry);
        try { default_denied.activate(m.manager_id); assert(false); }
        catch (const std::runtime_error&) {}
        assert(!registry.load_manager(m.manager_id).active);
        fs::remove_all(root);
        std::cout << "CETE 5C-3E controlled activation PASS\n";
        return 0;
    } catch (...) { fs::remove_all(root); throw; }
}
