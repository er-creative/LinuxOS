# cython: language_level=3
# cython: boundscheck=False
# cython: wraparound=False
# cython: initializedcheck=False
# cython: cdivision=True

"""Causal rolling-volume baselines for FMRSS."""

import numpy as np
cimport numpy as cnp

from libc.math cimport isfinite, NAN
from libc.stdlib cimport free, malloc, qsort


cdef int _compare_double(
    const void* left,
    const void* right
) noexcept nogil:
    cdef double a = (<double*>left)[0]
    cdef double b = (<double*>right)[0]

    if a < b:
        return -1
    if a > b:
        return 1
    return 0


cdef inline double _median_sorted(
    double* values,
    Py_ssize_t count
) noexcept nogil:
    if count & 1:
        return values[count // 2]

    return 0.5 * (
        values[(count // 2) - 1]
        + values[count // 2]
    )


cdef inline Py_ssize_t _copy_finite(
    double* source,
    Py_ssize_t count,
    double* destination
) noexcept nogil:
    cdef Py_ssize_t i
    cdef Py_ssize_t finite_count = 0

    for i in range(count):
        if isfinite(source[i]):
            destination[finite_count] = source[i]
            finite_count += 1

    return finite_count


cpdef tuple rolling_volume_baselines(
    const double[::1] volume,
    const cnp.int16_t[::1] time_slots,
    int rolling_window,
    int rolling_minimum_periods,
    int slot_window,
    int slot_minimum_periods
):
    """Calculate prior-window overall and same-time volume medians.

    The output at row i excludes volume[i]. The same-time history therefore
    contains only earlier occurrences of that five-minute time slot.
    """

    cdef Py_ssize_t size = volume.shape[0]
    cdef Py_ssize_t i
    cdef Py_ssize_t j
    cdef Py_ssize_t start
    cdef Py_ssize_t occurrence_count
    cdef Py_ssize_t finite_count
    cdef int slot
    cdef int slot_count
    cdef int slot_position
    cdef double value

    if time_slots.shape[0] != size:
        raise ValueError(
            "volume and time_slots must have the same length"
        )

    if rolling_window < 1:
        raise ValueError("rolling_window must be positive")

    if not 1 <= rolling_minimum_periods <= rolling_window:
        raise ValueError(
            "rolling_minimum_periods is outside the rolling window"
        )

    if slot_window < 1:
        raise ValueError("slot_window must be positive")

    if not 1 <= slot_minimum_periods <= slot_window:
        raise ValueError(
            "slot_minimum_periods is outside the slot window"
        )

    if rolling_window > 32767 or slot_window > 32767:
        raise ValueError("rolling windows must not exceed 32767")

    cdef cnp.ndarray[cnp.float64_t, ndim=1] rolling_medians = np.full(
        size, np.nan, dtype=np.float64
    )
    cdef cnp.ndarray[cnp.int16_t, ndim=1] rolling_counts = np.zeros(
        size, dtype=np.int16
    )
    cdef cnp.ndarray[cnp.float64_t, ndim=1] slot_medians = np.full(
        size, np.nan, dtype=np.float64
    )
    cdef cnp.ndarray[cnp.int16_t, ndim=1] slot_counts_output = np.zeros(
        size, dtype=np.int16
    )

    cdef double[::1] rolling_median_view = rolling_medians
    cdef cnp.int16_t[::1] rolling_count_view = rolling_counts
    cdef double[::1] slot_median_view = slot_medians
    cdef cnp.int16_t[::1] slot_count_output_view = slot_counts_output

    cdef double* work_buffer = <double*>malloc(
        max(rolling_window, slot_window) * sizeof(double)
    )
    cdef double* slot_history = <double*>malloc(
        1440 * slot_window * sizeof(double)
    )
    cdef int* slot_counts = <int*>malloc(1440 * sizeof(int))
    cdef int* slot_positions = <int*>malloc(1440 * sizeof(int))

    if (
        work_buffer == NULL
        or slot_history == NULL
        or slot_counts == NULL
        or slot_positions == NULL
    ):
        if work_buffer != NULL:
            free(work_buffer)
        if slot_history != NULL:
            free(slot_history)
        if slot_counts != NULL:
            free(slot_counts)
        if slot_positions != NULL:
            free(slot_positions)
        raise MemoryError("Unable to allocate volume rolling buffers")

    try:
        with nogil:
            for i in range(1440):
                slot_counts[i] = 0
                slot_positions[i] = 0

            for i in range(size):
                # Overall rolling baseline: previous row window only.
                start = i - rolling_window
                if start < 0:
                    start = 0

                finite_count = 0
                for j in range(start, i):
                    value = volume[j]
                    if isfinite(value):
                        work_buffer[finite_count] = value
                        finite_count += 1

                rolling_count_view[i] = <cnp.int16_t>finite_count

                if finite_count >= rolling_minimum_periods:
                    qsort(
                        work_buffer,
                        finite_count,
                        sizeof(double),
                        _compare_double
                    )
                    rolling_median_view[i] = _median_sorted(
                        work_buffer,
                        finite_count
                    )
                else:
                    rolling_median_view[i] = NAN

                # Same-time baseline from earlier occurrences only.
                slot = <int>time_slots[i]

                if slot < 0 or slot >= 1440:
                    slot_median_view[i] = NAN
                    slot_count_output_view[i] = 0
                    continue

                occurrence_count = slot_counts[slot]
                finite_count = _copy_finite(
                    &slot_history[slot * slot_window],
                    occurrence_count,
                    work_buffer
                )

                slot_count_output_view[i] = <cnp.int16_t>finite_count

                if finite_count >= slot_minimum_periods:
                    qsort(
                        work_buffer,
                        finite_count,
                        sizeof(double),
                        _compare_double
                    )
                    slot_median_view[i] = _median_sorted(
                        work_buffer,
                        finite_count
                    )
                else:
                    slot_median_view[i] = NAN

                # Push the current occurrence after calculating row i.
                slot_position = slot_positions[slot]
                slot_history[
                    slot * slot_window + slot_position
                ] = volume[i]

                if occurrence_count < slot_window:
                    slot_counts[slot] = occurrence_count + 1

                slot_positions[slot] = (
                    slot_position + 1
                ) % slot_window

    finally:
        free(work_buffer)
        free(slot_history)
        free(slot_counts)
        free(slot_positions)

    return (
        rolling_medians,
        rolling_counts,
        slot_medians,
        slot_counts_output
    )
