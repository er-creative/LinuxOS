# cython: language_level=3
# cython: boundscheck=False
# cython: wraparound=False
# cython: initializedcheck=False
# cython: cdivision=True

"""Cython rolling lag-one autocorrelation kernel for FMRSS.

For output row i, the correlation window contains paired observations ending
at i - 1.  The current candle is therefore excluded to prevent look-ahead.
"""

import numpy as np
cimport numpy as cnp

from libc.math cimport isfinite, sqrt, NAN


cpdef tuple rolling_lag1_autocorrelation(
    const double[::1] values,
    int window,
    int minimum_periods,
    double epsilon,
):
    cdef Py_ssize_t size = values.shape[0]
    cdef Py_ssize_t i
    cdef Py_ssize_t add_index
    cdef Py_ssize_t remove_index
    cdef int count = 0
    cdef double x
    cdef double y
    cdef double sum_x = 0.0
    cdef double sum_y = 0.0
    cdef double sum_x2 = 0.0
    cdef double sum_y2 = 0.0
    cdef double sum_xy = 0.0
    cdef double covariance_numerator
    cdef double variance_x_numerator
    cdef double variance_y_numerator
    cdef double denominator
    cdef double correlation

    if window < 2:
        raise ValueError("window must be at least 2")
    if minimum_periods < 3 or minimum_periods > window:
        raise ValueError("minimum_periods must be between 3 and window")
    if epsilon <= 0.0:
        raise ValueError("epsilon must be positive")

    cdef cnp.ndarray[cnp.float64_t, ndim=1] correlations = np.full(
        size, np.nan, dtype=np.float64
    )
    cdef cnp.ndarray[cnp.int32_t, ndim=1] observations = np.zeros(
        size, dtype=np.int32
    )
    cdef double[::1] correlation_view = correlations
    cdef cnp.int32_t[::1] observation_view = observations

    with nogil:
        for i in range(size):
            # Add pair j=i-1: x=return[j], y=return[j-1].
            add_index = i - 1
            if add_index >= 1:
                x = values[add_index]
                y = values[add_index - 1]
                if isfinite(x) and isfinite(y):
                    sum_x += x
                    sum_y += y
                    sum_x2 += x * x
                    sum_y2 += y * y
                    sum_xy += x * y
                    count += 1

            # Remove the pair that is now left of the previous-window range.
            remove_index = i - window - 1
            if remove_index >= 1:
                x = values[remove_index]
                y = values[remove_index - 1]
                if isfinite(x) and isfinite(y):
                    sum_x -= x
                    sum_y -= y
                    sum_x2 -= x * x
                    sum_y2 -= y * y
                    sum_xy -= x * y
                    count -= 1

            observation_view[i] = count
            if count < minimum_periods:
                correlation_view[i] = NAN
                continue

            covariance_numerator = sum_xy - (sum_x * sum_y / count)
            variance_x_numerator = sum_x2 - (sum_x * sum_x / count)
            variance_y_numerator = sum_y2 - (sum_y * sum_y / count)

            if (
                variance_x_numerator <= epsilon
                or variance_y_numerator <= epsilon
            ):
                correlation_view[i] = NAN
                continue

            denominator = sqrt(
                variance_x_numerator * variance_y_numerator
            )
            if denominator <= epsilon:
                correlation_view[i] = NAN
            else:
                correlation = covariance_numerator / denominator
                # Guard against tiny floating-point excursions beyond the
                # mathematically valid correlation interval.
                if correlation > 1.0:
                    correlation = 1.0
                elif correlation < -1.0:
                    correlation = -1.0
                correlation_view[i] = correlation

    return correlations, observations
