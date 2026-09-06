# cython: language_level=3
# cython: boundscheck=False
# cython: wraparound=False
# cython: initializedcheck=False
# cython: cdivision=True

"""Compiled rolling median and MAD kernel for FMRSS.

For output row i, the reference window ends at i - 1.  Consequently the
function cannot use the current residual impulse and does not introduce
look-ahead leakage.
"""

import numpy as np
cimport numpy as cnp

from libc.math cimport fabs, isfinite, NAN
from libc.stdlib cimport free, malloc, qsort


cdef int _compare_double(const void* left, const void* right) noexcept nogil:
    cdef double a = (<double*>left)[0]
    cdef double b = (<double*>right)[0]

    if a < b:
        return -1
    if a > b:
        return 1
    return 0


cdef inline double _median_sorted(double* values, Py_ssize_t count) noexcept nogil:
    if count & 1:
        return values[count // 2]

    return 0.5 * (
        values[(count // 2) - 1]
        + values[count // 2]
    )


cpdef tuple rolling_median_mad(
    const double[::1] values,
    int window,
    int minimum_periods
):
    """Return previous-window rolling median and median absolute deviation."""

    cdef Py_ssize_t size = values.shape[0]
    cdef Py_ssize_t i
    cdef Py_ssize_t j
    cdef Py_ssize_t start
    cdef Py_ssize_t count
    cdef double median
    cdef double value

    if window < 1:
        raise ValueError("window must be at least 1")

    if minimum_periods < 1 or minimum_periods > window:
        raise ValueError(
            "minimum_periods must be between 1 and window"
        )

    cdef cnp.ndarray[cnp.float64_t, ndim=1] medians = np.full(
        size,
        np.nan,
        dtype=np.float64
    )

    cdef cnp.ndarray[cnp.float64_t, ndim=1] mads = np.full(
        size,
        np.nan,
        dtype=np.float64
    )

    cdef double[::1] median_view = medians
    cdef double[::1] mad_view = mads

    cdef double* buffer = <double*>malloc(
        window * sizeof(double)
    )

    cdef double* deviation_buffer = <double*>malloc(
        window * sizeof(double)
    )

    if buffer == NULL or deviation_buffer == NULL:
        if buffer != NULL:
            free(buffer)
        if deviation_buffer != NULL:
            free(deviation_buffer)
        raise MemoryError(
            "Unable to allocate rolling robust-statistics buffers"
        )

    try:
        with nogil:
            for i in range(size):
                # Exclude row i. This directly implements shift(1).
                start = i - window
                if start < 0:
                    start = 0

                count = 0

                for j in range(start, i):
                    value = values[j]
                    if isfinite(value):
                        buffer[count] = value
                        count += 1

                if count < minimum_periods:
                    median_view[i] = NAN
                    mad_view[i] = NAN
                    continue

                qsort(
                    buffer,
                    count,
                    sizeof(double),
                    _compare_double
                )

                median = _median_sorted(
                    buffer,
                    count
                )

                median_view[i] = median

                for j in range(count):
                    deviation_buffer[j] = fabs(
                        buffer[j] - median
                    )

                qsort(
                    deviation_buffer,
                    count,
                    sizeof(double),
                    _compare_double
                )

                mad_view[i] = _median_sorted(
                    deviation_buffer,
                    count
                )

    finally:
        free(buffer)
        free(deviation_buffer)

    return medians, mads
