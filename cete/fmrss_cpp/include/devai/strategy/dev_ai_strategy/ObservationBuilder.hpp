#pragma once

#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"

#include <string>


namespace devai::strategy
{

class ObservationBuilder
{
public:

    [[nodiscard]]
    DevAIObservation build(
        const devai::ranking::CrossSectionalUniverseSnapshot& snapshot,
        const std::string& symbol,
        const DevPortfolioState& portfolio) const;
};

} // namespace devai::strategy