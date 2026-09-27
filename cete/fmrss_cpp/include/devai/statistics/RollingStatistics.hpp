#pragma once

#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <stdexcept>

namespace devai::statistics
{

class RollingStatistics
{
public:

    // ============================================================
    // Constructor
    // ============================================================

    explicit RollingStatistics(
        std::size_t window_size)
        : window_size_(window_size)
    {
        if (window_size_ == 0)
        {
            throw std::invalid_argument(
                "RollingStatistics window size must be greater than zero.");
        }
    }


    // ============================================================
    // Add observation
    // ============================================================

    void add(double value)
    {
        if (!std::isfinite(value))
        {
            throw std::invalid_argument(
                "RollingStatistics cannot accept a non-finite value.");
        }

        values_.push_back(value);

        /*
         * Keep these running totals because they remain useful for
         * O(1) rolling mean calculation and possible future
         * diagnostics.
         *
         * IMPORTANT:
         * variance() deliberately does NOT use
         *
         *     E[x^2] - E[x]^2
         *
         * because that formulation can suffer severe floating-point
         * cancellation when values are relatively large while their
         * dispersion is small.
         */

        sum_ += static_cast<long double>(value);

        sum_squares_ +=
            static_cast<long double>(value) *
            static_cast<long double>(value);

        if (values_.size() > window_size_)
        {
            const double removed =
                values_.front();

            values_.pop_front();

            sum_ -=
                static_cast<long double>(removed);

            sum_squares_ -=
                static_cast<long double>(removed) *
                static_cast<long double>(removed);
        }
    }


    // ============================================================
    // Reset
    // ============================================================

    void reset() noexcept
    {
        values_.clear();

        sum_ = 0.0L;

        sum_squares_ = 0.0L;
    }


    // ============================================================
    // State
    // ============================================================

    [[nodiscard]]
    std::size_t windowSize() const noexcept
    {
        return window_size_;
    }


    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return values_.size();
    }


    [[nodiscard]]
    bool empty() const noexcept
    {
        return values_.empty();
    }


    [[nodiscard]]
    bool ready() const noexcept
    {
        return values_.size() ==
               window_size_;
    }


    [[nodiscard]]
    const std::deque<double>&
    values() const noexcept
    {
        return values_;
    }


    // ============================================================
    // Rolling Mean
    //
    // Arithmetic mean of all observations currently present in
    // the rolling window.
    //
    // Partial-window statistics are intentionally supported.
    // ============================================================

    [[nodiscard]]
    double mean() const noexcept
    {
        if (values_.empty())
        {
            return quietNaN();
        }

        const long double count =
            static_cast<long double>(
                values_.size());

        return static_cast<double>(
            sum_ / count);
    }


    // ============================================================
    // Population Variance
    //
    // CETE Phase 4.1 definition:
    //
    //                  N
    //              1  ----
    // variance =  --- \      (x_i - mean)^2
    //              N  /
    //                 ----
    //                 i = 1
    //
    // IMPORTANT:
    //
    // Do NOT replace this with:
    //
    //     E[x^2] - E[x]^2
    //
    // Although algebraically equivalent, that formula is less
    // numerically stable for market data because it subtracts
    // two potentially large and nearly equal floating-point
    // quantities.
    //
    // The centered calculation below uses long double internally
    // to further reduce floating-point error.
    //
    // This remains POPULATION variance, not sample variance.
    // ============================================================

    [[nodiscard]]
    double variance() const noexcept
    {
        if (values_.empty())
        {
            return quietNaN();
        }

        const long double count =
            static_cast<long double>(
                values_.size());

        const long double mean_value =
            sum_ / count;

        long double
            squared_deviation_sum = 0.0L;

        for (const double value : values_)
        {
            const long double difference =
                static_cast<long double>(value) -
                mean_value;

            squared_deviation_sum +=
                difference * difference;
        }

        long double population_variance =
            squared_deviation_sum / count;

        /*
         * Mathematically the centered calculation cannot produce a
         * negative variance. This guard is retained as defensive
         * numerical protection.
         */

        if (population_variance < 0.0L)
        {
            constexpr long double
                tiny_negative_tolerance =
                    1.0e-18L;

            if (population_variance >=
                -tiny_negative_tolerance)
            {
                population_variance = 0.0L;
            }
            else
            {
                return quietNaN();
            }
        }

        return static_cast<double>(
            population_variance);
    }


    // ============================================================
    // Population Standard Deviation
    // ============================================================

    [[nodiscard]]
    double standardDeviation() const noexcept
    {
        const double variance_value =
            variance();

        if (!std::isfinite(
                variance_value))
        {
            return quietNaN();
        }

        if (variance_value <= 0.0)
        {
            return 0.0;
        }

        return std::sqrt(
            variance_value);
    }


    // ============================================================
    // Standard Z-Score
    //
    //                 x - mean
    // Z =          ---------------
    //                  stddev
    //
    // The current observation may be part of the rolling window.
    // This preserves the existing CETE Phase 4.1 contract.
    //
    // Zero-dispersion window -> 0.0
    // ============================================================

    [[nodiscard]]
    double zScore(double value) const noexcept
    {
        if (!std::isfinite(value))
        {
            return quietNaN();
        }

        if (values_.empty())
        {
            return quietNaN();
        }

        const double mean_value =
            mean();

        const double stddev =
            standardDeviation();

        if (!std::isfinite(mean_value) ||
            !std::isfinite(stddev))
        {
            return quietNaN();
        }

        constexpr double
            dispersion_floor = 1.0e-12;

        if (stddev <= dispersion_floor)
        {
            return 0.0;
        }

        return
            (value - mean_value) /
            stddev;
    }


private:

    // ============================================================
    // NaN helper
    // ============================================================

    [[nodiscard]]
    static double quietNaN() noexcept
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }


    // ============================================================
    // State
    // ============================================================

    std::size_t window_size_{0};

    std::deque<double> values_;

    /*
     * long double is intentionally used for accumulation.
     *
     * sum_ is used by mean().
     *
     * sum_squares_ is retained as rolling state but deliberately
     * NOT used by variance(), because E[x²] - E[x]² caused the
     * numerical instability detected by the real-market Phase 4.1
     * validation.
     */

    long double sum_{0.0L};

    long double sum_squares_{0.0L};
};

} // namespace devai::statistics