#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"


namespace devai::strategy
{

// ============================================================================
// DevReplayBuffer
//
// Bounded replay-memory container for validated DevTransition objects.
//
// Responsibilities:
//
//   - store completed Dev transitions
//   - enforce a fixed maximum capacity
//   - replace the oldest transition when full
//   - sample transitions without replacement
//   - provide deterministic sampling when initialized with the same seed
//   - preserve logical chronological ordering through at()
//
// This class deliberately does NOT:
//
//   - calculate rewards
//   - calculate P&L
//   - calculate brokerage/slippage
//   - inspect market data
//   - calculate forward returns
//   - select Dev actions
//   - perform neural-network inference
//   - perform learning
//   - know anything about DQN/PPO/etc.
//
// Every transition is validated before insertion using the frozen
// Phase 3.1 DevTransitionValidator.
// ============================================================================
class DevReplayBuffer
{
public:

    // ========================================================================
    // Construction
    //
    // capacity must be > 0.
    //
    // seed controls the internal random-number generator so tests and
    // experiments can be reproduced.
    // ========================================================================

    explicit DevReplayBuffer(
        std::size_t capacity,
        std::uint32_t seed = 0U);


    // ========================================================================
    // Insert one validated transition.
    //
    // If the buffer is not full:
    //     append transition.
    //
    // If the buffer is full:
    //     replace the logically oldest transition.
    // ========================================================================

    void push(
        const DevTransition& transition);


    // ========================================================================
    // Randomly sample batch_size transitions without replacement.
    //
    // batch_size must be:
    //
    //     1 <= batch_size <= size()
    // ========================================================================

    [[nodiscard]]
    std::vector<DevTransition> sample(
        std::size_t batch_size);


    // ========================================================================
    // Logical chronological access.
    //
    // at(0)          -> oldest stored transition
    // at(size() - 1) -> newest stored transition
    // ========================================================================

    [[nodiscard]]
    const DevTransition& at(
        std::size_t logical_index) const;


    // ========================================================================
    // State
    // ========================================================================

    [[nodiscard]]
    std::size_t size() const noexcept;


    [[nodiscard]]
    std::size_t capacity() const noexcept;


    [[nodiscard]]
    bool empty() const noexcept;


    [[nodiscard]]
    bool full() const noexcept;


    // ========================================================================
    // Remove all transitions.
    //
    // Capacity is preserved.
    // Random-number-generator state is intentionally preserved.
    // ========================================================================

    void clear() noexcept;


private:

    // ========================================================================
    // Convert logical chronological index into physical ring-buffer index.
    // ========================================================================

    [[nodiscard]]
    std::size_t physicalIndex(
        std::size_t logical_index) const noexcept;


    std::size_t capacity_{0};

    std::vector<DevTransition> storage_{};

    // Physical index of the logically oldest transition.
    std::size_t head_{0};

    // Number of valid transitions currently stored.
    std::size_t size_{0};

    std::mt19937 random_engine_{};

    DevTransitionValidator validator_{};
};

} // namespace devai::strategy