cmake_minimum_required(VERSION 3.16)

project(FMRSS_CPP
    VERSION 1.0.0
    LANGUAGES CXX
)

# ----------------------------------------------------------
# C++ Standard
# ----------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# ----------------------------------------------------------
# Main Executable
# ----------------------------------------------------------

add_executable(fmrss_cpp
    src/main.cpp
    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
)

target_include_directories(fmrss_cpp PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fmrss_cpp PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()


# ----------------------------------------------------------
# CandleAggregator Test
# ----------------------------------------------------------

add_executable(test_candle_aggregator
    tests/test_candle_aggregator.cpp
    src/market/CandleAggregator.cpp
)

target_include_directories(test_candle_aggregator PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_candle_aggregator PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Real Market Aggregation Integration Test
# ----------------------------------------------------------

add_executable(test_real_market_aggregation
    tests/test_real_market_aggregation.cpp
    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
)

target_include_directories(test_real_market_aggregation PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_real_market_aggregation PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Multi-Timeframe Synchronizer Test
# ----------------------------------------------------------

add_executable(test_multi_timeframe_synchronizer
    tests/test_multi_timeframe_synchronizer.cpp
    src/market/MultiTimeframeSynchronizer.cpp
)

target_include_directories(test_multi_timeframe_synchronizer PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_multi_timeframe_synchronizer PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Real Multi-Timeframe Integration Test
# ----------------------------------------------------------

add_executable(test_real_multitimeframe
    tests/test_real_multitimeframe.cpp
    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
)

target_include_directories(test_real_multitimeframe PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_real_multitimeframe PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# All-Symbol Multi-Timeframe Integration Test
# ----------------------------------------------------------

add_executable(test_all_symbols_multitimeframe
    tests/test_all_symbols_multitimeframe.cpp
    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
)

target_include_directories(
    test_all_symbols_multitimeframe PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_all_symbols_multitimeframe PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Market Data Gap / Quality Validation
# ----------------------------------------------------------

add_executable(test_market_data_gaps
    tests/test_market_data_gaps.cpp
    src/market/MarketDataLoader.cpp
)

target_include_directories(
    test_market_data_gaps PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_market_data_gaps PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Market Data Repair Engine Test
# ----------------------------------------------------------

add_executable(test_market_data_repair
    tests/test_market_data_repair.cpp
    src/market/MarketDataRepairEngine.cpp
)

target_include_directories(
    test_market_data_repair PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_market_data_repair PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Historical Data Provider Test
# ----------------------------------------------------------

add_executable(test_historical_data_provider
    tests/test_historical_data_provider.cpp
    src/market/FileHistoricalDataProvider.cpp
    src/market/MarketDataLoader.cpp
)

target_include_directories(
    test_historical_data_provider PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_historical_data_provider PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Multi-Timeframe Cursor Test
# ----------------------------------------------------------

add_executable(test_multi_timeframe_cursor
    tests/test_multi_timeframe_cursor.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/MultiTimeframeSynchronizer.cpp
)

target_include_directories(
    test_multi_timeframe_cursor PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_multi_timeframe_cursor PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Session State Test
# ----------------------------------------------------------

add_executable(test_session_state
    tests/test_session_state.cpp
    src/market/SessionState.cpp
)

target_include_directories(
    test_session_state PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_session_state PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
# ----------------------------------------------------------
# Market Snapshot Test
# ----------------------------------------------------------

add_executable(test_market_snapshot
    tests/test_market_snapshot.cpp
    src/market/MarketSnapshotBuilder.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/SessionState.cpp
)

target_include_directories(
    test_market_snapshot PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_market_snapshot PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Stock + Benchmark Synchronization Test
# ----------------------------------------------------------

add_executable(test_stock_benchmark_synchronizer
    tests/test_stock_benchmark_synchronizer.cpp
    src/market/StockBenchmarkSynchronizer.cpp
)

target_include_directories(
    test_stock_benchmark_synchronizer PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_stock_benchmark_synchronizer PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# Phase 2.5 - Real Runtime Pipeline Validation
# ----------------------------------------------------------

add_executable(test_real_runtime_pipeline
    tests/test_real_runtime_pipeline.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp
    src/market/StockBenchmarkSynchronizer.cpp
)

target_include_directories(
    test_real_runtime_pipeline PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_runtime_pipeline PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ==========================================================
# CETE PHASE 3 — FEATURE ENGINE
# ==========================================================


# ----------------------------------------------------------
# CETE Phase 3.1 - Price / Return Feature Engine
# ----------------------------------------------------------

add_executable(test_price_return_feature_engine
    tests/test_price_return_feature_engine.cpp
    src/features/PriceReturnFeatureEngine.cpp
)

target_include_directories(
    test_price_return_feature_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_price_return_feature_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()


# ----------------------------------------------------------
# CETE Phase 3.1 - Real Price / Return Feature Validation
# ----------------------------------------------------------

add_executable(test_real_price_return_features
    tests/test_real_price_return_features.cpp

    src/features/PriceReturnFeatureEngine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp
)

target_include_directories(
    test_real_price_return_features PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_price_return_features PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()


# ----------------------------------------------------------
# CETE Phase 3.2 - Trend / Momentum Feature Engine
# ----------------------------------------------------------

add_executable(test_trend_momentum_feature_engine
    tests/test_trend_momentum_feature_engine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
)

target_include_directories(
    test_trend_momentum_feature_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_trend_momentum_feature_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.3 - Volume Feature Engine
# ----------------------------------------------------------

add_executable(test_volume_feature_engine
    tests/test_volume_feature_engine.cpp
    src/features/VolumeFeatureEngine.cpp
)

target_include_directories(
    test_volume_feature_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_volume_feature_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.3 - Real Volume Feature Validation
# ----------------------------------------------------------

add_executable(test_real_volume_features
    tests/test_real_volume_features.cpp

    src/features/VolumeFeatureEngine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp
)

target_include_directories(
    test_real_volume_features PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_volume_features PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.4 - Volatility Feature Engine
# ----------------------------------------------------------

add_executable(test_volatility_feature_engine
    tests/test_volatility_feature_engine.cpp
    src/features/VolatilityFeatureEngine.cpp
)

target_include_directories(
    test_volatility_feature_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_volatility_feature_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.4 - Real Volatility Feature Validation
# ----------------------------------------------------------

add_executable(test_real_volatility_features
    tests/test_real_volatility_features.cpp

    src/features/VolatilityFeatureEngine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp
)

target_include_directories(
    test_real_volatility_features PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_volatility_features PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.5 - Multi-Timeframe Feature Engine
# ----------------------------------------------------------

add_executable(test_multi_timeframe_feature_engine
    tests/test_multi_timeframe_feature_engine.cpp
    src/features/MultiTimeframeFeatureEngine.cpp
)

target_include_directories(
    test_multi_timeframe_feature_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_multi_timeframe_feature_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.5 - Real Multi-Timeframe Feature Validation
# ----------------------------------------------------------

add_executable(test_real_multi_timeframe_features
    tests/test_real_multi_timeframe_features.cpp

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp
    src/features/MultiTimeframeFeatureEngine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp
)

target_include_directories(
    test_real_multi_timeframe_features PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_multi_timeframe_features PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.6 - Stock / Benchmark Relative Features
# ----------------------------------------------------------

add_executable(test_stock_benchmark_feature_engine
    tests/test_stock_benchmark_feature_engine.cpp
    src/features/StockBenchmarkFeatureEngine.cpp
)

target_include_directories(
    test_stock_benchmark_feature_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_stock_benchmark_feature_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ----------------------------------------------------------
# CETE Phase 3.6 - Real Stock / NIFTY Relative Validation
# ----------------------------------------------------------

add_executable(test_real_stock_benchmark_features
    tests/test_real_stock_benchmark_features.cpp

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp
    src/features/StockBenchmarkFeatureEngine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp
    src/market/StockBenchmarkSynchronizer.cpp
)

target_include_directories(
    test_real_stock_benchmark_features PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_stock_benchmark_features PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.1 — ROLLING STATISTICAL ENGINE
# ============================================================

add_executable(test_rolling_statistical_engine
    tests/test_rolling_statistical_engine.cpp
    src/statistics/RollingStatisticalEngine.cpp
)

target_include_directories(
    test_rolling_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_rolling_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.1 — REAL ROLLING STATISTICAL VALIDATION
# ============================================================

add_executable(test_real_rolling_statistical_engine
    tests/test_real_rolling_statistical_engine.cpp

    src/statistics/RollingStatisticalEngine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp
)

target_include_directories(
    test_real_rolling_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_rolling_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.2 Return Statistical Engine
# ============================================================

add_executable(test_return_statistical_engine
    tests/test_return_statistical_engine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/RollingStatisticalEngine.cpp
)

target_include_directories(
    test_return_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_return_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()


# ============================================================
# PHASE 4.2 REAL Return Statistical VALIDATION
# ============================================================
add_executable(test_real_return_statistical_engine
    tests/test_real_return_statistical_engine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    src/features/PriceReturnFeatureEngine.cpp

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
)

target_include_directories(
    test_real_return_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_return_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.3 volume_statistical_engine VALIDATION
# ============================================================
add_executable(test_volume_statistical_engine
    tests/test_volume_statistical_engine.cpp
    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
)

target_include_directories(
    test_volume_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_volume_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.3 Real volume_statistical_engine VALIDATION
# ============================================================


add_executable(test_real_volume_statistical_engine
    tests/test_real_volume_statistical_engine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    src/features/VolumeFeatureEngine.cpp

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
)

target_include_directories(
    test_real_volume_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_volume_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.4 test_stock_benchmark_statistical_engine VALIDATION
# ============================================================

add_executable(test_stock_benchmark_statistical_engine
    tests/test_stock_benchmark_statistical_engine.cpp
    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
)

target_include_directories(
    test_stock_benchmark_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_stock_benchmark_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.4 Real test_stock_benchmark_statistical_engine VALIDATION
# ============================================================

add_executable(test_real_stock_benchmark_statistical_engine
    tests/test_real_stock_benchmark_statistical_engine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    src/features/PriceReturnFeatureEngine.cpp

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
)

target_include_directories(
    test_real_stock_benchmark_statistical_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_stock_benchmark_statistical_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# PHASE 4.5 market_regime VALIDATION
# ============================================================

add_executable(test_market_regime_engine
    tests/test_market_regime_engine.cpp
    src/statistics/MarketRegimeEngine.cpp
)

target_include_directories(
    test_market_regime_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_market_regime_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()


# ============================================================
# PHASE 4.5 Real market_regime VALIDATION
# ============================================================
add_executable(
    test_real_market_regime_engine

    tests/test_real_market_regime_engine.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
)

target_include_directories(
    test_real_market_regime_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_market_regime_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()


# ============================================================
# PHASE 4.6 Mathematical_state_composerVALIDATION
# ============================================================

add_executable(
    test_mathematical_state_composer

    tests/test_mathematical_state_composer.cpp

    src/statistics/MathematicalStateComposer.cpp
)

target_include_directories(
    test_mathematical_state_composer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================
# PHASE 4.6 Real Mathematical_state_composer VALIDATION
# ============================================================

add_executable(
    test_real_mathematical_state_composer

    tests/test_real_mathematical_state_composer.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp
)

target_include_directories(
    test_real_mathematical_state_composer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# ==================
# 4.6 Version 2
# =================
add_executable(
    test_real_mathematical_state_composer_ver2

    tests/test_real_mathematical_state_composer_ver2.cpp

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp
)

target_include_directories(
    test_real_mathematical_state_composer_ver2
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================
# PHASE 5.1 CROSS-SECTIONAL RANKER VALIDATION
# ============================================================

add_executable(
    test_cross_sectional_ranker

    tests/test_cross_sectional_ranker.cpp
    src/ranking/CrossSectionalRanker.cpp
)

target_include_directories(
    test_cross_sectional_ranker
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================
# PHASE 5.2 CROSS-SECTIONAL MATHEMATICAL RANKER VALIDATION
# ============================================================

add_executable(
    test_cross_sectional_mathematical_ranker

    tests/test_cross_sectional_mathematical_ranker.cpp

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
)

target_include_directories(
    test_cross_sectional_mathematical_ranker
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================
# PHASE 5.3 MULTI-TIMEFRAME CROSS-SECTIONAL STATE Validation
# ============================================================

add_executable(
    test_cross_sectional_mathematical_state_composer

    tests/test_cross_sectional_mathematical_state_composer.cpp

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
)

target_include_directories(
    test_cross_sectional_mathematical_state_composer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================
# PHASE 5.4 CROSS-SECTIONAL UNIVERSE SNAPSHOT Validation
# ============================================================

add_executable(
    test_cross_sectional_universe_snapshot

    tests/test_cross_sectional_universe_snapshot.cpp

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_include_directories(
    test_cross_sectional_universe_snapshot
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)



# ============================================================
# PHASE 5.5 REAL END-TO-END CROSS-SECTIONAL VALIDATION
# ============================================================

add_executable(
    test_real_cross_sectional_end_to_end

    tests/test_real_cross_sectional_end_to_end.cpp

    # --------------------------------------------------------
    # Phase 1 / Phase 2
    # --------------------------------------------------------

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # --------------------------------------------------------
    # Phase 3
    # --------------------------------------------------------

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # --------------------------------------------------------
    # Phase 4
    # --------------------------------------------------------

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # --------------------------------------------------------
    # Phase 5
    # --------------------------------------------------------

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_include_directories(
    test_real_cross_sectional_end_to_end
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_cross_sectional_end_to_end
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()