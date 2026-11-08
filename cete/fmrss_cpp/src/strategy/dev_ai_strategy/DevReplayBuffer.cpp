#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>


namespace devai::strategy
{

// ============================================================================
// Constructor
// ============================================================================

DevReplayBuffer::DevReplayBuffer(
    const std::size_t capacity,
    const std::uint32_t seed)
    :
    capacity_(capacity),
    random_engine_(seed)
{
    if (capacity_ == 0)
    {
        throw std::invalid_argument(
            "DevReplayBuffer capacity must be greater than zero.");
    }

    // Reserve once so normal insertion does not repeatedly reallocate.
    //
    // We deliberately use reserve() rather than resize() because only
    // genuinely inserted transitions should count as stored experiences.
    storage_.reserve(capacity_);
}


// ============================================================================
// physicalIndex
// ============================================================================

std::size_t DevReplayBuffer::physicalIndex(
    const std::size_t logical_index) const noexcept
{
    return (head_ + logical_index) % capacity_;
}


// ============================================================================
// push
// ============================================================================

void DevReplayBuffer::push(
    const DevTransition& transition)
{
    // Phase 3.1 remains the authority for transition validity.
    validator_.validate(transition);


    // ------------------------------------------------------------------------
    // Buffer not yet full.
    // ------------------------------------------------------------------------

    if (size_ < capacity_)
    {
        // Until the first wrap, storage_.size() may still be smaller than
        // capacity_. Normally the physical insertion index equals size_.
        //
        // The branch below also keeps the implementation correct after clear()
        // if vector storage remains allocated.
        const std::size_t insertion_index =
            physicalIndex(size_);

        if (insertion_index < storage_.size())
        {
            storage_[insertion_index] =
                transition;
        }
        else
        {
            storage_.push_back(
                transition);
        }

        ++size_;

        return;
    }


    // ------------------------------------------------------------------------
    // Buffer full.
    //
    // head_ points at the oldest transition.
    // Replace it with the newest transition, then advance head_.
    // ------------------------------------------------------------------------

    storage_[head_] =
        transition;

    head_ =
        (head_ + 1) % capacity_;
}


// ============================================================================
// sample
// ============================================================================

std::vector<DevTransition> DevReplayBuffer::sample(
    const std::size_t batch_size)
{
    if (batch_size == 0)
    {
        throw std::invalid_argument(
            "DevReplayBuffer sample size must be greater than zero.");
    }


    if (batch_size > size_)
    {
        throw std::invalid_argument(
            "DevReplayBuffer sample size cannot exceed stored transition count.");
    }


    // Build logical indices:
    //
    //     0 ... size_ - 1
    //
    // These represent oldest -> newest regardless of physical ring layout.

    std::vector<std::size_t> indices(
        size_);

    std::iota(
        indices.begin(),
        indices.end(),
        std::size_t{0});


    // Shuffle using the buffer's seeded PRNG.
    std::shuffle(
        indices.begin(),
        indices.end(),
        random_engine_);


    std::vector<DevTransition> batch;

    batch.reserve(
        batch_size);


    for (std::size_t i = 0;
         i < batch_size;
         ++i)
    {
        batch.push_back(
            at(indices[i]));
    }


    return batch;
}


// ============================================================================
// at
// ============================================================================

const DevTransition& DevReplayBuffer::at(
    const std::size_t logical_index) const
{
    if (logical_index >= size_)
    {
        throw std::out_of_range(
            "DevReplayBuffer logical index is out of range.");
    }


    return storage_[
        physicalIndex(logical_index)
    ];
}


// ============================================================================
// size
// ============================================================================

std::size_t DevReplayBuffer::size() const noexcept
{
    return size_;
}


// ============================================================================
// capacity
// ============================================================================

std::size_t DevReplayBuffer::capacity() const noexcept
{
    return capacity_;
}


// ============================================================================
// empty
// ============================================================================

bool DevReplayBuffer::empty() const noexcept
{
    return size_ == 0;
}


// ============================================================================
// full
// ============================================================================

bool DevReplayBuffer::full() const noexcept
{
    return size_ == capacity_;
}


// ============================================================================
// clear
// ============================================================================

void DevReplayBuffer::clear() noexcept
{
    storage_.clear();

    head_ = 0;
    size_ = 0;
}

} // namespace devai::strategy