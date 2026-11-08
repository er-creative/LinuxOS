#pragma once

#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"

#include <array>


namespace devai::strategy
{

class ObservationNormalizer
{
public:

    explicit ObservationNormalizer(
        double maximum_holding_bars = 390.0);


    [[nodiscard]]
    std::array<double, DEV_OBSERVATION_SIZE>
    normalize(
        const DevAIObservation& observation) const;


    [[nodiscard]]
    double maximumHoldingBars() const noexcept;


private:

    double maximum_holding_bars_;
};

} // namespace devai::strategy