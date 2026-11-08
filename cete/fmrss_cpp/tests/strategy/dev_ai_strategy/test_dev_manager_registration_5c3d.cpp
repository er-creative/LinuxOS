#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main() {
    const auto root = fs::temp_directory_path() / "cete_5c3d_registration_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root);
        const std::vector<std::string> symbols = {
            "RELIANCE", "HDFCBANK", "ICICIBANK", "SBIN", "BHARTIARTL",
            "AXISBANK", "HCLTECH", "ITC", "NTPC", "ADANIPORTS"
        };
        const auto created = registry.create_manager(symbols);
        assert(created.manager_id == "DevManager_001");
        registry.set_active(created.manager_id, false);
        const auto loaded = registry.load_manager("DevManager_001");
        assert(loaded.symbols == symbols);
        assert(!loaded.active);
        assert(fs::is_regular_file(root / "DevManager_001" / "shares.txt"));
        assert(fs::is_regular_file(root / "DevManager_001" / "manager.conf"));
        assert(registry.list_managers().size() == 1);
        fs::remove_all(root);
        std::cout << "CETE 5C-3D manager registration PASS\n";
        return 0;
    } catch (const std::exception& e) {
        fs::remove_all(root);
        std::cerr << "CETE 5C-3D test FAIL: " << e.what() << '\n';
        return 1;
    }
}
