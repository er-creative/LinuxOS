#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstddef>

namespace devai {
struct ManagedUniverse {
    std::string manager_id;
    bool active{true};
    std::vector<std::string> symbols;
};

// Supervisor-owned registry. Not an execution/risk authorization interface.
class DevManagerRegistry {
public:
    explicit DevManagerRegistry(std::filesystem::path root);
    ManagedUniverse create_manager(const std::vector<std::string>& symbols);
    ManagedUniverse load_manager(const std::string& manager_id) const;
    void assign_shares(const std::string& manager_id, const std::vector<std::string>& symbols);
    void set_active(const std::string& manager_id, bool active);
    std::vector<ManagedUniverse> list_managers() const;
    static std::vector<std::string> validate_symbols(const std::vector<std::string>& symbols);
private:
    std::filesystem::path root_;
    std::filesystem::path manager_path(const std::string& manager_id) const;
    void save(const ManagedUniverse& universe) const;
};
}
