#pragma once
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace devai::benchmark {

// The same model object is shared by both managers and their agents.
// A role selects the legal output interpretation; it never selects a separate model.
enum class DevRLRole : std::uint8_t { Agent, Manager };
enum class DevRLCandidate : std::uint8_t { SequentialTD = 0, MaskablePPO = 1, QRDQN = 2, DiscreteCQL = 3 };

inline constexpr std::size_t kInputCount = devai::strategy::DEV_SHARED_INPUT;
inline constexpr std::size_t kOutputCount = devai::strategy::DEV_SHARED_OUTPUT;
inline constexpr std::size_t kAgentActionCount = devai::strategy::DEV_Q_OUTPUT_SIZE;
inline constexpr std::size_t kManagerActionCount = devai::strategy::DEV_MANAGER_ACTION_COUNT;
static_assert(kInputCount == 148 && kOutputCount == 7);
static_assert(kAgentActionCount == 4 && kManagerActionCount == 7);

struct DevRLRequest {
    DevRLRole role{DevRLRole::Manager};
    std::array<double, kInputCount> features{};
    std::array<bool, kOutputCount> legal_actions{};
};

struct DevRLDecision {
    std::array<double, kOutputCount> action_values{};
    std::size_t selected_action{0};
    bool valid{false};
};

class IDevRLModel {
public:
    virtual ~IDevRLModel() = default;
    [[nodiscard]] virtual DevRLCandidate candidate() const noexcept = 0;
    [[nodiscard]] virtual std::string_view modelName() const noexcept = 0;
    [[nodiscard]] virtual DevRLDecision evaluate(const DevRLRequest& request) const = 0;
};

// Shared evaluation contract: validates input and legal mask, never selects an illegal action.
[[nodiscard]] DevRLDecision selectLegalAction(
    const DevRLRequest& request,
    const std::array<double, kOutputCount>& values);

// Adapter uses an already-loaded shared network. It does NOT silently load V1 as V3.
class DevSequentialTDAdapter final : public IDevRLModel {
public:
    explicit DevSequentialTDAdapter(
        std::shared_ptr<const devai::strategy::DevSharedNeuralNetwork> model);
    [[nodiscard]] DevRLCandidate candidate() const noexcept override;
    [[nodiscard]] std::string_view modelName() const noexcept override;
    [[nodiscard]] DevRLDecision evaluate(const DevRLRequest& request) const override;
private:
    std::shared_ptr<const devai::strategy::DevSharedNeuralNetwork> model_;
};

// No implementation of the advanced algorithms is implied by these identifiers.
[[nodiscard]] std::string_view candidateName(DevRLCandidate candidate) noexcept;
[[nodiscard]] bool isImplementedInH2(DevRLCandidate candidate) noexcept;

} // namespace devai::benchmark
