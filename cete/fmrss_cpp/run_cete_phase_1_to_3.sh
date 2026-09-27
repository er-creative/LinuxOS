#!/usr/bin/env bash

# ============================================================
# CETE — PHASE 1 TO PHASE 3 COMPLETE VALIDATION RUNNER
#
# Phase 1 — Market Data Foundation
# Phase 2 — Multi-Timeframe Runtime
# Phase 3 — Feature Engine
#
# Stops immediately if any test fails.
# ============================================================

set -u
set -o pipefail

# ============================================================
# CONFIGURATION
# ============================================================

PROJECT_ROOT="$HOME/fmrss_cpp"
BUILD_DIR="$PROJECT_ROOT/build"

DATA_ROOT="/home/hadoop/shareMarket_Data"
MINUTE_DATA_DIR="$DATA_ROOT/minute"

REFERENCE_SYMBOL="RELIANCE"
REFERENCE_1M_FILE="$MINUTE_DATA_DIR/${REFERENCE_SYMBOL}_1min.txt"

PASSED=0
FAILED=0
CURRENT_PHASE=""

# ============================================================
# TERMINAL COLOURS
# ============================================================

BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RESET='\033[0m'

# ============================================================
# HELPER FUNCTIONS
# ============================================================

print_main_header()
{
    echo
    echo "============================================================"
    echo -e "${BOLD}CETE — COMPLETE PHASE 1–3 VALIDATION${RESET}"
    echo "============================================================"
    echo "Project        : $PROJECT_ROOT"
    echo "Build          : $BUILD_DIR"
    echo "Market data    : $MINUTE_DATA_DIR"
    echo "Reference file : $REFERENCE_1M_FILE"
    echo "============================================================"
}

print_phase()
{
    CURRENT_PHASE="$1"

    echo
    echo "============================================================"
    echo -e "${CYAN}${BOLD}$1${RESET}"
    echo "============================================================"
}

run_test()
{
    local target="$1"
    local description="$2"

    shift 2

    local executable="$BUILD_DIR/$target"

    echo
    echo "------------------------------------------------------------"
    echo -e "${BOLD}$description${RESET}"
    echo "Target : $target"

    if [[ $# -gt 0 ]]; then
        echo "Args   : $*"
    fi

    echo "------------------------------------------------------------"

    # --------------------------------------------------------
    # Verify executable
    # --------------------------------------------------------

    if [[ ! -f "$executable" ]]; then

        echo
        echo -e "${RED}✗ TEST EXECUTABLE NOT FOUND${RESET}"
        echo "Expected:"
        echo "$executable"

        exit 1
    fi

    if [[ ! -x "$executable" ]]; then

        echo
        echo -e "${RED}✗ TEST FILE IS NOT EXECUTABLE${RESET}"
        echo "$executable"

        exit 1
    fi

    # --------------------------------------------------------
    # Run test
    # --------------------------------------------------------

    "$executable" "$@"
    local exit_code=$?

    if [[ $exit_code -eq 0 ]]; then

        PASSED=$((PASSED + 1))

        echo
        echo -e "${GREEN}✓ PASSED — $description${RESET}"

    else

        FAILED=$((FAILED + 1))

        echo
        echo "============================================================"
        echo -e "${RED}${BOLD}✗ TEST FAILED${RESET}"
        echo "============================================================"
        echo "Phase     : $CURRENT_PHASE"
        echo "Test      : $description"
        echo "Target    : $target"
        echo "Exit code : $exit_code"

        if [[ $# -gt 0 ]]; then
            echo "Arguments : $*"
        fi

        echo
        echo -e "${RED}CETE Phase 1–3 validation stopped.${RESET}"
        echo "============================================================"

        exit "$exit_code"
    fi
}

# ============================================================
# START
# ============================================================

print_main_header

# ============================================================
# VERIFY PROJECT
# ============================================================

if [[ ! -d "$PROJECT_ROOT" ]]; then

    echo
    echo -e "${RED}✗ CETE PROJECT DIRECTORY NOT FOUND${RESET}"
    echo "$PROJECT_ROOT"

    exit 1
fi

# ============================================================
# VERIFY MARKET DATA DIRECTORY
# ============================================================

if [[ ! -d "$MINUTE_DATA_DIR" ]]; then

    echo
    echo -e "${RED}✗ 1-MINUTE MARKET-DATA DIRECTORY NOT FOUND${RESET}"
    echo "$MINUTE_DATA_DIR"

    exit 1
fi

# ============================================================
# VERIFY REFERENCE FILE
# ============================================================

if [[ ! -f "$REFERENCE_1M_FILE" ]]; then

    echo
    echo -e "${RED}✗ REFERENCE MARKET-DATA FILE NOT FOUND${RESET}"
    echo "$REFERENCE_1M_FILE"

    exit 1
fi

echo
echo -e "${GREEN}✓ Market-data reference file found${RESET}"

cd "$PROJECT_ROOT" || exit 1

# ============================================================
# STEP 1 — CMAKE CONFIGURATION
# ============================================================

echo
echo "============================================================"
echo -e "${BOLD}STEP 1 — CMAKE CONFIGURATION${RESET}"
echo "============================================================"

if cmake -S . -B "$BUILD_DIR" -G Ninja; then

    echo
    echo -e "${GREEN}✓ CMake configuration passed${RESET}"

else

    echo
    echo -e "${RED}✗ CMake configuration failed${RESET}"

    exit 1
fi

# ============================================================
# STEP 2 — BUILD CETE
# ============================================================

echo
echo "============================================================"
echo -e "${BOLD}STEP 2 — BUILD CETE${RESET}"
echo "============================================================"

if cmake --build "$BUILD_DIR"; then

    echo
    echo -e "${GREEN}✓ CETE build passed${RESET}"

else

    echo
    echo -e "${RED}✗ CETE build failed${RESET}"

    exit 1
fi

# ============================================================
#
# PHASE 1
#
# MARKET DATA FOUNDATION
#
# ============================================================

print_phase "PHASE 1 — MARKET DATA FOUNDATION"

# ------------------------------------------------------------
# 1.1 Candle Aggregator
# ------------------------------------------------------------

run_test \
    "test_candle_aggregator" \
    "1.1 Candle Aggregator"

# ------------------------------------------------------------
# 1.2 Real Market Aggregation
#
# Required argument:
# <1-minute-market-data-file>
# ------------------------------------------------------------

run_test \
    "test_real_market_aggregation" \
    "1.2 Real Market Aggregation" \
    "$REFERENCE_1M_FILE"

# ------------------------------------------------------------
# 1.3 Multi-Timeframe Synchronizer
# ------------------------------------------------------------

run_test \
    "test_multi_timeframe_synchronizer" \
    "1.3 Multi-Timeframe Synchronizer"

# ------------------------------------------------------------
# 1.4 Real Multi-Timeframe Validation
#
# Required argument:
# <1-minute-market-data-file>
# ------------------------------------------------------------

run_test \
    "test_real_multitimeframe" \
    "1.4 Real Multi-Timeframe Validation" \
    "$REFERENCE_1M_FILE"

# ------------------------------------------------------------
# 1.5 All-Symbol Multi-Timeframe Validation
# ------------------------------------------------------------

run_test \
    "test_all_symbols_multitimeframe" \
    "1.5 All-Symbol Multi-Timeframe Validation"

# ------------------------------------------------------------
# 1.6 Market-Data Gap Detection
# ------------------------------------------------------------

run_test \
    "test_market_data_gaps" \
    "1.6 Market-Data Gap Detection"

# ------------------------------------------------------------
# 1.7 Market-Data Repair Engine
# ------------------------------------------------------------

run_test \
    "test_market_data_repair" \
    "1.7 Market-Data Repair Engine"

echo
echo "------------------------------------------------------------"
echo -e "${GREEN}${BOLD}✓ PHASE 1 — COMPLETE${RESET}"
echo "------------------------------------------------------------"

# ============================================================
#
# PHASE 2
#
# MULTI-TIMEFRAME RUNTIME
#
# ============================================================

print_phase "PHASE 2 — MULTI-TIMEFRAME RUNTIME"

# ------------------------------------------------------------
# 2.1 Multi-Timeframe Cursor
# ------------------------------------------------------------

run_test \
    "test_multi_timeframe_cursor" \
    "2.1 Multi-Timeframe Cursor"

# ------------------------------------------------------------
# 2.2 Session State
# ------------------------------------------------------------

run_test \
    "test_session_state" \
    "2.2 Session State"

# ------------------------------------------------------------
# 2.3 Market Snapshot
# ------------------------------------------------------------

run_test \
    "test_market_snapshot" \
    "2.3 Market Snapshot"

# ------------------------------------------------------------
# 2.4 Stock / NIFTY Synchronization
# ------------------------------------------------------------

run_test \
    "test_stock_benchmark_synchronizer" \
    "2.4 Stock / NIFTY Synchronization"

# ------------------------------------------------------------
# 2.5 Real Runtime Pipeline
# ------------------------------------------------------------

run_test \
    "test_real_runtime_pipeline" \
    "2.5 Real Runtime Pipeline"

echo
echo "------------------------------------------------------------"
echo -e "${GREEN}${BOLD}✓ PHASE 2 — COMPLETE${RESET}"
echo "------------------------------------------------------------"

# ============================================================
#
# PHASE 3
#
# FEATURE ENGINE
#
# ============================================================

print_phase "PHASE 3 — FEATURE ENGINE"

# ============================================================
# 3.1 PRICE / RETURN
# ============================================================

echo
echo -e "${CYAN}>>> 3.1 PRICE / RETURN FEATURES${RESET}"

run_test \
    "test_price_return_feature_engine" \
    "3.1A Price / Return Feature Engine"

run_test \
    "test_real_price_return_features" \
    "3.1B Real Price / Return Validation"

# ============================================================
# 3.2 TREND / MOMENTUM
# ============================================================

echo
echo -e "${CYAN}>>> 3.2 TREND / MOMENTUM FEATURES${RESET}"

run_test \
    "test_trend_momentum_feature_engine" \
    "3.2 Trend / Momentum Feature Engine"

# ============================================================
# 3.3 VOLUME
# ============================================================

echo
echo -e "${CYAN}>>> 3.3 VOLUME FEATURES${RESET}"

run_test \
    "test_volume_feature_engine" \
    "3.3A Volume Feature Engine"

run_test \
    "test_real_volume_features" \
    "3.3B Real Volume Validation"

# ============================================================
# 3.4 VOLATILITY
# ============================================================

echo
echo -e "${CYAN}>>> 3.4 VOLATILITY FEATURES${RESET}"

run_test \
    "test_volatility_feature_engine" \
    "3.4A Volatility Feature Engine"

run_test \
    "test_real_volatility_features" \
    "3.4B Real Volatility Validation"

# ============================================================
# 3.5 MULTI-TIMEFRAME
# ============================================================

echo
echo -e "${CYAN}>>> 3.5 MULTI-TIMEFRAME FEATURES${RESET}"

run_test \
    "test_multi_timeframe_feature_engine" \
    "3.5A Multi-Timeframe Feature Engine"

run_test \
    "test_real_multi_timeframe_features" \
    "3.5B Real Multi-Timeframe Feature Validation"

# ============================================================
# 3.6 STOCK / NIFTY RELATIVE FEATURES
# ============================================================

echo
echo -e "${CYAN}>>> 3.6 STOCK / NIFTY RELATIVE FEATURES${RESET}"

run_test \
    "test_stock_benchmark_feature_engine" \
    "3.6A Stock / NIFTY Relative Feature Engine"

run_test \
    "test_real_stock_benchmark_features" \
    "3.6B Real Stock / NIFTY Relative Validation"

echo
echo "------------------------------------------------------------"
echo -e "${GREEN}${BOLD}✓ PHASE 3 — COMPLETE${RESET}"
echo "------------------------------------------------------------"

# ============================================================
# FINAL REPORT
# ============================================================

echo
echo
echo "============================================================"
echo -e "${GREEN}${BOLD}CETE PHASE 1–3 COMPLETE VALIDATION PASSED${RESET}"
echo "============================================================"
echo
echo -e "PHASE 1 — Market Data Foundation       ${GREEN}✓ PASSED${RESET}"
echo -e "PHASE 2 — Multi-Timeframe Runtime      ${GREEN}✓ PASSED${RESET}"
echo -e "PHASE 3 — Feature Engine               ${GREEN}✓ PASSED${RESET}"
echo
echo "------------------------------------------------------------"
echo "Tests passed : $PASSED"
echo "Tests failed : $FAILED"
echo "------------------------------------------------------------"
echo
echo -e "${GREEN}${BOLD}CETE PHASES 1–3 ARE HEALTHY${RESET}"
echo -e "${GREEN}${BOLD}READY TO PROCEED TO PHASE 4${RESET}"
echo
echo "============================================================"

exit 0