#include "devai/strategy/dev_ai_strategy/DevOutOfSampleValidator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace devai::strategy
{

std::size_t
DevOutOfSampleValidator::actionIndex(
    const DevAction action)
{
    switch (action)
    {
        case DevAction::HOLD:  return 0U;
        case DevAction::LONG:  return 1U;
        case DevAction::SHORT: return 2U;
        case DevAction::EXIT:  return 3U;
    }

    throw std::invalid_argument(
        "DevOutOfSampleValidator: invalid DevAction.");
}

bool
DevOutOfSampleValidator::parametersEqual(
    const DevQNetworkParameters& left,
    const DevQNetworkParameters& right) noexcept
{
    return
        left.weights1 == right.weights1 &&
        left.biases1 == right.biases1 &&
        left.weights2 == right.weights2 &&
        left.biases2 == right.biases2 &&
        left.weights3 == right.weights3 &&
        left.biases3 == right.biases3;
}

DevOutOfSampleValidationResult
DevOutOfSampleValidator::evaluate(
    const DevQNetwork& frozen_network,
    const std::int64_t training_end_time,
    const std::vector<DevOutOfSamplePoint>& oos_points) const
{
    if (training_end_time <= 0)
    {
        throw std::invalid_argument(
            "DevOutOfSampleValidator: training_end_time must be positive.");
    }

    if (oos_points.empty())
    {
        throw std::invalid_argument(
            "DevOutOfSampleValidator: OOS sequence must not be empty.");
    }

    if (frozen_network.parameterCount() != 7652U)
    {
        throw std::runtime_error(
            "DevOutOfSampleValidator: frozen Dev Q-network architecture mismatch.");
    }

    for (std::size_t i = 0; i < oos_points.size(); ++i)
    {
        const auto& point = oos_points[i];

        if (point.decision_time <= 0)
        {
            throw std::invalid_argument(
                "DevOutOfSampleValidator: OOS decision time must be positive.");
        }

        if (point.decision_time <= training_end_time)
        {
            throw std::invalid_argument(
                "DevOutOfSampleValidator: OOS point overlaps training period.");
        }

        if (i > 0 &&
            point.decision_time <=
                oos_points[i - 1U].decision_time)
        {
            throw std::invalid_argument(
                "DevOutOfSampleValidator: OOS points must be strictly chronological.");
        }

        for (const double value :
             point.normalized_observation)
        {
            if (!std::isfinite(value))
            {
                throw std::invalid_argument(
                    "DevOutOfSampleValidator: OOS observation contains non-finite value.");
            }
        }
    }

    const DevQNetworkParameters before =
        frozen_network.parameters();

    DevOutOfSampleValidationResult result;

    result.training_end_time =
        training_end_time;

    result.oos_start_time =
        oos_points.front().decision_time;

    result.oos_end_time =
        oos_points.back().decision_time;

    result.strict_temporal_separation =
        result.oos_start_time >
        result.training_end_time;

    result.predictions.reserve(
        oos_points.size());

    double sum_maximum_q = 0.0;

    result.minimum_maximum_q =
        std::numeric_limits<double>::infinity();

    result.maximum_maximum_q =
        -std::numeric_limits<double>::infinity();

    for (const auto& point : oos_points)
    {
        const DevQValues q_values =
            frozen_network.predict(
                point.normalized_observation);

        if (!q_values.numerical_integrity)
        {
            throw std::runtime_error(
                "DevOutOfSampleValidator: Q inference failed numerical integrity.");
        }

        DevOutOfSamplePrediction prediction;

        prediction.decision_time =
            point.decision_time;

        prediction.greedy_action =
            q_values.greedyAction();

        prediction.q_values =
            q_values.values;

        prediction.maximum_q =
            q_values.maximumValue();

        prediction.numerical_integrity =
            std::isfinite(
                prediction.maximum_q);

        for (const double q :
             prediction.q_values)
        {
            prediction.numerical_integrity =
                prediction.numerical_integrity &&
                std::isfinite(q);
        }

        if (!prediction.numerical_integrity)
        {
            throw std::runtime_error(
                "DevOutOfSampleValidator: non-finite OOS prediction.");
        }

        ++result.greedy_action_counts[
            actionIndex(
                prediction.greedy_action)];

        sum_maximum_q +=
            prediction.maximum_q;

        result.minimum_maximum_q =
            std::min(
                result.minimum_maximum_q,
                prediction.maximum_q);

        result.maximum_maximum_q =
            std::max(
                result.maximum_maximum_q,
                prediction.maximum_q);

        result.predictions.push_back(
            prediction);
    }

    result.evaluated_points =
        result.predictions.size();

    result.mean_maximum_q =
        sum_maximum_q /
        static_cast<double>(
            result.evaluated_points);

    const DevQNetworkParameters after =
        frozen_network.parameters();

    result.network_unchanged =
        parametersEqual(
            before,
            after);

    result.numerical_integrity =
        result.strict_temporal_separation &&
        result.network_unchanged &&
        result.evaluated_points ==
            oos_points.size() &&
        std::isfinite(
            result.mean_maximum_q) &&
        std::isfinite(
            result.minimum_maximum_q) &&
        std::isfinite(
            result.maximum_maximum_q);

    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "DevOutOfSampleValidator: OOS validation failed integrity checks.");
    }

    return result;
}

} // namespace devai::strategy
