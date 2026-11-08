#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>
namespace devai::strategy::h46 {
enum class Side { LONG, SHORT };
struct ManagerAssignment {
    std::string_view manager;
    Side side;
    std::array<std::string_view,5> agents;
};
inline constexpr ManagerAssignment long_manager{
    "DevManager_001", Side::LONG,
    {"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"}};
inline constexpr ManagerAssignment short_manager{
    "DevManager_002", Side::SHORT,
    {"DevAgent_006","DevAgent_007","DevAgent_008","DevAgent_009","DevAgent_010"}};
inline const ManagerAssignment& assignment(std::string_view manager) {
    if (manager == long_manager.manager) return long_manager;
    if (manager == short_manager.manager) return short_manager;
    throw std::invalid_argument("H4.6: unknown manager (fail closed)");
}
inline std::string_view agentForAction(std::string_view manager, std::size_t action) {
    if (action < 2 || action > 6) throw std::invalid_argument("H4.6: invalid selection action");
    return assignment(manager).agents[action-2];
}
inline bool permits(std::string_view manager, Side side) {
    return assignment(manager).side == side;
}
} // namespace devai::strategy::h46
