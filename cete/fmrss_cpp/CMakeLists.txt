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

# ============================================================
# CETE SHARED — BROKERAGE / STATUTORY CHARGES
# ============================================================


add_library(
    cete_brokerage

    src/strategy/brokerage/BrokerageChargesEngine.cpp
)

target_include_directories(
    cete_brokerage
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    cete_brokerage
    PUBLIC
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        cete_brokerage
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ============================================================
# CETE SHARED P&L ACCOUNTING
# ============================================================
#
# Strategy-independent accounting base.
#
# Used by:
#   - DevAI
#   - PCA Strategy
#   - future CETE strategies
#
# ============================================================

add_library(
    cete_accounting

    src/strategy/accounting/PnLAccountingEngine.cpp
    src/strategy/accounting/PnLAccountingIntegration.cpp
)


target_include_directories(
    cete_accounting
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    cete_accounting
    PUBLIC
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        cete_accounting
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# CETE SHARED P&L ACCOUNTING VALIDATION
# ============================================================

add_executable(
    test_pnl_accounting_engine

    tests/strategy/accounting/test_pnl_accounting_engine.cpp
)


target_link_libraries(
    test_pnl_accounting_engine
    PRIVATE
        cete_accounting
)


target_include_directories(
    test_pnl_accounting_engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_pnl_accounting_engine
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_pnl_accounting_engine
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI STRATEGY
# ============================================================
#
# Phase 1 — RL Environment Foundation
# Phase 2 — Trading Simulation / Episode Engine
# Phase 3 — Dev Agent Foundation & Learning Infrastructure
#
# ============================================================

add_library(
    devai_strategy

    # ========================================================
    # DEVAI PHASE 1 — RL ENVIRONMENT FOUNDATION
    # ========================================================

    src/strategy/dev_ai_strategy/ObservationBuilder.cpp
    src/strategy/dev_ai_strategy/ObservationNormalizer.cpp
    src/strategy/dev_ai_strategy/RewardEngine.cpp
    src/strategy/dev_ai_strategy/TradingEnvironment.cpp


    # ========================================================
    # DEVAI PHASE 2.1 — HISTORICAL CETE STATE REPLAY
    # ========================================================

    src/strategy/dev_ai_strategy/HistoricalCETEReplay.cpp


    # ========================================================
    # DEVAI PHASE 2.2 — DEV EPISODE DEFINITION
    # ========================================================

    src/strategy/dev_ai_strategy/DevEpisode.cpp


    # ========================================================
    # DEVAI PHASE 2.3 — POSITION / EXPOSURE STATE
    # ========================================================

    src/strategy/dev_ai_strategy/PositionExposureState.cpp


    # ========================================================
    # DEVAI PHASE 2.4 — TRANSACTION-COST MODEL
    # ========================================================

    src/strategy/dev_ai_strategy/TransactionCostModel.cpp


    # ========================================================
    # DEVAI PHASE 2.5 — SLIPPAGE MODEL
    # ========================================================

    src/strategy/dev_ai_strategy/SlippageModel.cpp


    # ========================================================
    # DEVAI PHASE 2.8 — TERMINAL CONDITIONS
    # ========================================================

    src/strategy/dev_ai_strategy/TerminalConditionEngine.cpp


    # ========================================================
    # DEVAI PHASE 3.1 — DEV TRANSITION CONTRACT
    # ========================================================

    src/strategy/dev_ai_strategy/DevTransition.cpp


    # ========================================================
    # DEVAI PHASE 3.2 — DEV REPLAY BUFFER
    # ========================================================

    src/strategy/dev_ai_strategy/DevReplayBuffer.cpp


    # ========================================================
    # DEVAI PHASE 3.3 — DEV POLICY INTERFACE
    # ========================================================

    src/strategy/dev_ai_strategy/DevPolicy.cpp


    # ========================================================
    # DEVAI PHASE 3.4 — BASELINE DEV AGENT
    # ========================================================

    src/strategy/dev_ai_strategy/BaselineDevAgent.cpp


    # ========================================================
    # DEVAI PHASE 3.5 — DEV AGENT EPISODE RUNNER
    # ========================================================

    src/strategy/dev_ai_strategy/DevAgentEpisodeRunner.cpp


    # ========================================================
    # DEVAI PHASE 3.6 — HISTORICAL EXPERIENCE GENERATION
    # ========================================================

    src/strategy/dev_ai_strategy/HistoricalExperienceGenerator.cpp


    # ========================================================
    # DEVAI PHASE 3.7 — DEV NEURAL NETWORK / Q MODEL
    # DEVAI PHASE 3.8B — Q-NETWORK BACKPROPAGATION
    # DEVAI PHASE 3.8C — Q-NETWORK MINI-BATCH UPDATE
    # ========================================================

    src/strategy/dev_ai_strategy/DevQNetwork.cpp


    # ========================================================
    # DEVAI PHASE 3.8A — BELLMAN TARGET ENGINE
    # ========================================================

    src/strategy/dev_ai_strategy/DevBellmanTargetEngine.cpp


    # ========================================================
    # DEVAI PHASE 3.8C — MINI-BATCH TRAINER
    # ========================================================

    src/strategy/dev_ai_strategy/DevMiniBatchTrainer.cpp

    # ========================================================
    # DEVAI PHASE 3.8D — TARGET NETWORK SYNCHRONIZATION
    # ========================================================

    src/strategy/dev_ai_strategy/DevTargetNetworkSynchronizer.cpp

    # ========================================================
    # DEVAI PHASE 3.8E — EPSILON-GREEDY LEARNING POLICY
    # ========================================================

    src/strategy/dev_ai_strategy/DevEpsilonGreedyPolicy.cpp

    # ========================================================
    # DEVAI PHASE 3.8F — DEV TRAINING ENGINE
    # ========================================================

    src/strategy/dev_ai_strategy/DevTrainingEngine.cpp

    # ========================================================
    # DEVAI PHASE 3.9
    # ========================================================
    
    src/strategy/dev_ai_strategy/DevModelPersistence.cpp

    # ========================================================
    # DEVAI PHASE 3.10
    # ========================================================
    src/strategy/dev_ai_strategy/DevOutOfSampleValidator.cpp

    # ========================================================
    # DEVAI PHASE 3.11
    # ========================================================
    src/strategy/dev_ai_strategy/DevTopCandidateSelector.cpp

    # ========================================================
    # DEVAI PHASE 3.12
    # ========================================================
    src/strategy/dev_ai_strategy/DevAgent.cpp

    # ========================================================
    # DEVAI PHASE 3.13 — DEVMANAGER ARCHITECTURE
    # ========================================================
    src/strategy/dev_ai_strategy/DevManager.cpp

    # =============================================================================
    # DEVAI PHASE 3.14 — DEVAGENT -> DEVMANAGER DECISION CONTRACT
    # =============================================================================
    src/strategy/dev_ai_strategy/DevAgentManagerContract.cpp

    # =============================================================================
    # DEVAI PHASE 3.15 — HISTORICAL TRAINING DATASET / CACHE
    # =============================================================================
    src/strategy/dev_ai_strategy/DevHistoricalTrainingCache.cpp

    # =============================================================================
    # DEVAI PHASE 3.15B — REAL HISTORICAL CACHE GENERATOR
    # =============================================================================
    src/strategy/dev_ai_strategy/DevRealHistoricalCacheGenerator.cpp

    # ============================================================
    # DEVAI PHASE 3.16 — SHARED DEVAGENT TRAINING
    # ============================================================
    src/strategy/dev_ai_strategy/DevSharedAgentTrainer.cpp

    # ============================================================
    # DEVAI PHASE 3.17
    # ============================================================
    src/strategy/dev_ai_strategy/DevManagerTraining.cpp
    # ============================================================================
    # Phase 3.18 — Portfolio Rotation / 75% Allocation
    # ============================================================================
    src/strategy/dev_ai_strategy/DevPortfolioRotation.cpp

    # ============================================================================
    # Phase 3.19 
    # ============================================================================
    src/strategy/dev_ai_strategy/DevMultiStockOOSValidator.cpp

    # ============================================================================
    # Phase 3.19B 
    # ============================================================================
    
    src/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.cpp

    # ============================================================================
    # Phase 3.19C
    # ============================================================================

    src/strategy/dev_ai_strategy/DevHistoricalOOSRunner.cpp



    # ============================================================================
    # Phase 3.19D — Frozen Model OOS Integration
    # ============================================================================
    src/strategy/dev_ai_strategy/DevFrozenOOSPolicy.cpp

    # ============================================================================
    # Phase 3.19D-3 — Historical Model Training Integration
    # ============================================================================
    src/strategy/dev_ai_strategy/DevHistoricalTrainingIntegration.cpp

    # ============================================================================
    # Phase 3.19 Shared DevAI Neural Network Alignment
    # ============================================================================
    src/strategy/dev_ai_strategy/DevSharedNeuralNetwork.cpp

    # ============================================================================
    # Phase 3.19D-3B — Real Historical Training Driver
    # ============================================================================
    src/strategy/dev_ai_strategy/DevRealHistoricalTrainingDriver.cpp

    # ============================================================
    # DEVAI — SUPERVISOR AND MANAGER INTEGRATION
    # ============================================================

    src/cete/supervision/DevinderjeetSupervisor.cpp
    src/strategy/dev_ai_strategy/DevSupervisedManager.cpp
    src/strategy/dev_ai_strategy/DevManagerSupervisorBridge.cpp
    # ============================================================
    # CETE / DevAI — D9F.4B
    # Production Cost-Aware Reward Integration
    # ============================================================
    src/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.cpp
    src/strategy/dev_ai_strategy/reward/DevCostAwareRewardIntegration_D9F4B.cpp

)


# ============================================================
# H4.7 Stage 5E.2K.4 — Zerodha Equity Intraday Cost Model
# Compiled into the existing devai_strategy library.
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevZerodhaIntradayCosts.cpp
)

# ============================================================
# DEVAI STRATEGY DEPENDENCIES
# ============================================================

target_link_libraries(
    devai_strategy
    PUBLIC
        cete_brokerage
)


# ============================================================
# INCLUDE DIRECTORIES
# ============================================================

target_include_directories(
    devai_strategy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# ============================================================
# C++ STANDARD
# ============================================================

target_compile_features(
    devai_strategy
    PUBLIC
        cxx_std_20
)


# ============================================================
# COMPILER WARNINGS
# ============================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        devai_strategy
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 3.1 — DEV TRANSITION TEST
# ============================================================

add_executable(
    test_dev_transition
    tests/strategy/dev_ai_strategy/test_dev_transition.cpp
)

target_link_libraries(
    test_dev_transition
    PRIVATE
        devai_strategy
)

target_compile_features(
    test_dev_transition
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_dev_transition
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()


# ============================================================
# DEVAI PHASE 3.2 — DEV REPLAY BUFFER TEST
# ============================================================

add_executable(
    test_dev_replay_buffer
    tests/strategy/dev_ai_strategy/test_dev_replay_buffer.cpp
)

target_link_libraries(
    test_dev_replay_buffer
    PRIVATE
        devai_strategy
)

target_compile_features(
    test_dev_replay_buffer
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_dev_replay_buffer
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 3.3 — DEV POLICY INTERFACE TEST
# ============================================================

add_executable(
    test_dev_policy
    tests/strategy/dev_ai_strategy/test_dev_policy.cpp
)

target_link_libraries(
    test_dev_policy
    PRIVATE
        devai_strategy
)

target_compile_features(
    test_dev_policy
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_dev_policy
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 3.4 — BASELINE DEV AGENT TEST
# ============================================================

add_executable(
    test_baseline_dev_agent
    tests/strategy/dev_ai_strategy/test_baseline_dev_agent.cpp
)

target_link_libraries(
    test_baseline_dev_agent
    PRIVATE
        devai_strategy
)

target_compile_features(
    test_baseline_dev_agent
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_baseline_dev_agent
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()


# ============================================================
# DEVAI PHASE 1.3 — REAL CETE OBSERVATION INTEGRATION
# ============================================================

add_executable(
    test_real_devai_observation

    tests/strategy/dev_ai_strategy/test_real_devai_observation.cpp

    # --------------------------------------------------------
    # CETE — MARKET DATA / MULTI-TIMEFRAME RUNTIME
    # --------------------------------------------------------

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # --------------------------------------------------------
    # CETE — PHASE 3 FEATURE ENGINES
    # --------------------------------------------------------

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # --------------------------------------------------------
    # CETE — PHASE 4 MATHEMATICAL / STATISTICAL ENGINES
    # --------------------------------------------------------

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # --------------------------------------------------------
    # CETE — PHASE 5 CROSS-SECTIONAL RANKING
    # --------------------------------------------------------

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)


# ============================================================
# LINK DEVAI PHASE 1 LIBRARY
# ============================================================

target_link_libraries(
    test_real_devai_observation
    PRIVATE
    devai_strategy
)


# ============================================================
# INCLUDE DIRECTORY
# ============================================================

target_include_directories(
    test_real_devai_observation
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# ============================================================
# C++ STANDARD
# ============================================================

target_compile_features(
    test_real_devai_observation
    PRIVATE
    cxx_std_20
)


# ============================================================
# COMPILER WARNINGS
# ============================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_real_devai_observation
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ============================================================
# CETE SHARED RISK
# ============================================================

add_library(
    cete_risk

    src/strategy/risk/RiskStateEngine.cpp
)


target_include_directories(
    cete_risk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    cete_risk
    PUBLIC
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        cete_risk
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 1.4 — OBSERVATION NORMALIZATION TEST
# ============================================================

add_executable(
    test_observation_normalizer

    tests/strategy/dev_ai_strategy/test_observation_normalizer.cpp
)

target_link_libraries(
    test_observation_normalizer
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_observation_normalizer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_observation_normalizer
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_observation_normalizer
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 1.5 — ACTION-SPACE VALIDATION
# ============================================================

add_executable(
    test_dev_action_space

    tests/strategy/dev_ai_strategy/test_dev_action_space.cpp
)

target_link_libraries(
    test_dev_action_space
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_dev_action_space
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_action_space
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_dev_action_space
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 1.6 — REWARD ENGINE VALIDATION
# ============================================================

add_executable(
    test_reward_engine

    tests/strategy/dev_ai_strategy/test_reward_engine.cpp
)

target_link_libraries(
    test_reward_engine
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_reward_engine
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_reward_engine
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_reward_engine
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()



# ============================================================
# CETE SHARED — BROKERAGE VALIDATION
# ============================================================

add_executable(
    test_brokerage_charges_engine

    tests/strategy/brokerage/test_brokerage_charges_engine.cpp
)

target_link_libraries(
    test_brokerage_charges_engine
    PRIVATE
    cete_brokerage
)

target_include_directories(
    test_brokerage_charges_engine
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_brokerage_charges_engine
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_brokerage_charges_engine
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 1.7 — TRADING ENVIRONMENT VALIDATION
# ============================================================

add_executable(
    test_trading_environment

    tests/strategy/dev_ai_strategy/test_trading_environment.cpp
)

target_link_libraries(
    test_trading_environment
    PRIVATE
    devai_strategy
    cete_brokerage
)

target_include_directories(
    test_trading_environment
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_trading_environment
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_trading_environment
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 1.8 — LEAKAGE + ENVIRONMENT VALIDATION
# ============================================================

add_executable(
    test_phase1_leakage_validation
    tests/strategy/dev_ai_strategy/test_phase1_leakage_validation.cpp
)

target_link_libraries(
    test_phase1_leakage_validation
    PRIVATE
    devai_strategy
    cete_brokerage
)

target_include_directories(
    test_phase1_leakage_validation
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_phase1_leakage_validation
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_phase1_leakage_validation
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
# ============================================================
# CETE SHARED OUTCOME
# ============================================================

add_library(
    cete_outcome

    src/strategy/outcome/ForwardReturnOutcomeEngine.cpp
)


target_include_directories(
    cete_outcome
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    cete_outcome
    PUBLIC
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        cete_outcome
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 2.1
# HISTORICAL CETE STATE REPLAY VALIDATION
# ============================================================

add_executable(
    test_historical_cete_replay

    tests/strategy/dev_ai_strategy/test_historical_cete_replay.cpp

    # --------------------------------------------------------
    # CETE MARKET / MULTI-TIMEFRAME RUNTIME
    # --------------------------------------------------------

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # --------------------------------------------------------
    # CETE PHASE 3 — FEATURE ENGINES
    # --------------------------------------------------------

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # --------------------------------------------------------
    # CETE PHASE 4 — STATISTICAL / MATHEMATICAL ENGINES
    # --------------------------------------------------------

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # --------------------------------------------------------
    # CETE PHASE 5 — CROSS-SECTIONAL ENGINE
    # --------------------------------------------------------

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_link_libraries(
    test_historical_cete_replay
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_historical_cete_replay
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_historical_cete_replay
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_historical_cete_replay
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ============================================================================
# DevAI Phase 2.2 — Dev Episode Definition Validation
# ============================================================================

add_executable(
    test_dev_episode
    tests/strategy/dev_ai_strategy/test_dev_episode.cpp
)

target_link_libraries(
    test_dev_episode
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_dev_episode
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_episode
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_episode
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 2.3 — POSITION / EXPOSURE STATE VALIDATION
# ============================================================

add_executable(
    test_position_exposure_state

    tests/strategy/dev_ai_strategy/test_position_exposure_state.cpp
)

target_link_libraries(
    test_position_exposure_state
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_position_exposure_state
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_position_exposure_state
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_position_exposure_state
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 2.4 — TRANSACTION-COST MODEL VALIDATION
# ============================================================

add_executable(
    test_transaction_cost_model

    tests/strategy/dev_ai_strategy/test_transaction_cost_model.cpp
)


target_link_libraries(
    test_transaction_cost_model
    PRIVATE
        devai_strategy
)


target_include_directories(
    test_transaction_cost_model
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_transaction_cost_model
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_transaction_cost_model
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 2.5 — SLIPPAGE MODEL VALIDATION
# ============================================================

add_executable(
    test_slippage_model

    tests/strategy/dev_ai_strategy/test_slippage_model.cpp
)


target_link_libraries(
    test_slippage_model
    PRIVATE
        devai_strategy
)


target_include_directories(
    test_slippage_model
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_slippage_model
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_slippage_model
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# CETE PHASE 2.6A— SHARED P&L INTEGRATION VALIDATION
# ============================================================

add_executable(
    test_pnl_accounting_integration

    tests/strategy/accounting/test_pnl_accounting_integration.cpp
)


target_link_libraries(
    test_pnl_accounting_integration
    PRIVATE
        cete_accounting
)


target_include_directories(
    test_pnl_accounting_integration
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_pnl_accounting_integration
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_pnl_accounting_integration
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 2.6B
# TRADING ENVIRONMENT / CETE P&L RECONCILIATION
# ============================================================

add_executable(
    test_trading_environment_pnl_reconciliation

    tests/strategy/accounting/test_trading_environment_pnl_reconciliation.cpp
)


target_link_libraries(
    test_trading_environment_pnl_reconciliation
    PRIVATE
        devai_strategy
        cete_accounting
)


target_include_directories(
    test_trading_environment_pnl_reconciliation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_trading_environment_pnl_reconciliation
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_trading_environment_pnl_reconciliation
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# CETE PHASE 2.7A — SHARED RISK-STATE VALIDATION
# ============================================================

add_executable(
    test_risk_state_engine

    tests/strategy/risk/test_risk_state_engine.cpp
)


target_link_libraries(
    test_risk_state_engine
    PRIVATE
        cete_risk
)


target_include_directories(
    test_risk_state_engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_risk_state_engine
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_risk_state_engine
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 2.7B
# TRADING ENVIRONMENT / CETE RISK RECONCILIATION
# ============================================================

add_executable(
    test_trading_environment_risk_reconciliation

    tests/strategy/risk/test_trading_environment_risk_reconciliation.cpp
)


target_link_libraries(
    test_trading_environment_risk_reconciliation
    PRIVATE
        devai_strategy
        cete_risk
)


target_include_directories(
    test_trading_environment_risk_reconciliation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_trading_environment_risk_reconciliation
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_trading_environment_risk_reconciliation
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 2.8 — TERMINAL CONDITIONS
# ============================================================

add_executable(
    test_terminal_condition_engine

    tests/strategy/dev_ai_strategy/test_terminal_condition_engine.cpp
)


target_link_libraries(
    test_terminal_condition_engine
    PRIVATE
        devai_strategy
)


target_include_directories(
    test_terminal_condition_engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_terminal_condition_engine
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_terminal_condition_engine
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# CETE PHASE 2.9 — FORWARD-RETURN OUTCOME ENGINE
# ============================================================

add_executable(
    test_forward_return_outcome_engine

    tests/strategy/outcome/test_forward_return_outcome_engine.cpp
)


target_link_libraries(
    test_forward_return_outcome_engine
    PRIVATE
        cete_outcome
)


target_include_directories(
    test_forward_return_outcome_engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    test_forward_return_outcome_engine
    PRIVATE
        cxx_std_20
)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_forward_return_outcome_engine
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 2.10 — REAL HISTORICAL EPISODE VALIDATION
# ============================================================

add_executable(
    test_real_historical_episode

    tests/strategy/dev_ai_strategy/test_real_historical_episode.cpp

    # --------------------------------------------------------
    # CETE — MARKET DATA / MULTI-TIMEFRAME RUNTIME
    # --------------------------------------------------------

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # --------------------------------------------------------
    # CETE — PHASE 3 FEATURE ENGINES
    # --------------------------------------------------------

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # --------------------------------------------------------
    # CETE — PHASE 4 MATHEMATICAL / STATISTICAL ENGINES
    # --------------------------------------------------------

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # --------------------------------------------------------
    # CETE — PHASE 5 CROSS-SECTIONAL RANKING
    # --------------------------------------------------------

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_link_libraries(
    test_real_historical_episode
    PRIVATE
        devai_strategy
        cete_accounting
        cete_risk
        cete_outcome
)

target_include_directories(
    test_real_historical_episode
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_real_historical_episode
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_historical_episode
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.5 — DEV AGENT EPISODE RUNNER TEST
# ============================================================

add_executable(
    test_dev_agent_episode_runner
    tests/strategy/dev_ai_strategy/test_dev_agent_episode_runner.cpp
)

target_link_libraries(
    test_dev_agent_episode_runner
    PRIVATE
    devai_strategy
)

target_compile_features(
    test_dev_agent_episode_runner
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_agent_episode_runner
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.6 — HISTORICAL EXPERIENCE GENERATION TEST
# ============================================================

add_executable(
    test_historical_experience_generator
    tests/strategy/dev_ai_strategy/test_historical_experience_generator.cpp
)

target_link_libraries(
    test_historical_experience_generator
    PRIVATE
    devai_strategy
)

target_compile_features(
    test_historical_experience_generator
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_historical_experience_generator
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.6B — REAL HISTORICAL EXPERIENCE GENERATION
# ============================================================

add_executable(
    test_real_historical_experience_generation

    tests/strategy/dev_ai_strategy/test_real_historical_experience_generation.cpp

    # --------------------------------------------------------
    # CETE MARKET DATA
    # --------------------------------------------------------

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # --------------------------------------------------------
    # CETE FEATURE ENGINE
    # --------------------------------------------------------

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # --------------------------------------------------------
    # CETE MATHEMATICAL / STATISTICAL ENGINE
    # --------------------------------------------------------

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # --------------------------------------------------------
    # CETE CROSS-SECTIONAL RANKING
    # --------------------------------------------------------

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_link_libraries(
    test_real_historical_experience_generation
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_real_historical_experience_generation
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_real_historical_experience_generation
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_historical_experience_generation
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.7 — DEV NEURAL NETWORK / Q MODEL TEST
# ============================================================

add_executable(
    test_dev_q_network
    tests/strategy/dev_ai_strategy/test_dev_q_network.cpp
)

target_link_libraries(
    test_dev_q_network
    PRIVATE
    devai_strategy
)

target_compile_features(
    test_dev_q_network
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_q_network
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.7B — REAL HISTORICAL Q-NETWORK INTEGRATION
# ============================================================

add_executable(
    test_real_historical_q_network

    tests/strategy/dev_ai_strategy/test_real_historical_q_network.cpp

    # --------------------------------------------------------
    # CETE MARKET DATA
    # --------------------------------------------------------

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # --------------------------------------------------------
    # CETE FEATURE ENGINE
    # --------------------------------------------------------

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # --------------------------------------------------------
    # CETE MATHEMATICAL / STATISTICAL ENGINE
    # --------------------------------------------------------

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # --------------------------------------------------------
    # CETE CROSS-SECTIONAL RANKING
    # --------------------------------------------------------

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_link_libraries(
    test_real_historical_q_network
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_real_historical_q_network
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_real_historical_q_network
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_historical_q_network
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.8A — BELLMAN TARGET ENGINE
# ============================================================

add_executable(
    test_dev_bellman_target_engine
    tests/strategy/dev_ai_strategy/test_dev_bellman_target_engine.cpp
)

target_link_libraries(
    test_dev_bellman_target_engine
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_dev_bellman_target_engine
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_bellman_target_engine
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_bellman_target_engine
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.8B — Q-NETWORK GRADIENT / BACKPROPAGATION
# ============================================================

add_executable(
    test_dev_q_network_backpropagation
    tests/strategy/dev_ai_strategy/test_dev_q_network_backpropagation.cpp
)

target_link_libraries(
    test_dev_q_network_backpropagation
    PRIVATE
    devai_strategy
)

target_include_directories(
    test_dev_q_network_backpropagation
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_q_network_backpropagation
    PRIVATE
    cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_q_network_backpropagation
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
# ============================================================
# DEVAI Test Section
# ============================================================

# ============================================================
# DEVAI PHASE 3.8C — MINI-BATCH TRAINING TEST
# ============================================================

add_executable(
    test_dev_mini_batch_training
    tests/strategy/dev_ai_strategy/test_dev_mini_batch_training.cpp
)

target_link_libraries(
    test_dev_mini_batch_training
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_dev_mini_batch_training
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_mini_batch_training
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_dev_mini_batch_training
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 3.8D — TARGET NETWORK SYNCHRONIZATION TEST
# ============================================================

add_executable(
    test_dev_target_network_synchronizer
    tests/strategy/dev_ai_strategy/test_dev_target_network_synchronizer.cpp
)

target_link_libraries(
    test_dev_target_network_synchronizer
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_dev_target_network_synchronizer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_target_network_synchronizer
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_dev_target_network_synchronizer
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 3.8E — EPSILON-GREEDY LEARNING POLICY TEST
# ============================================================

add_executable(
    test_dev_epsilon_greedy_policy
    tests/strategy/dev_ai_strategy/test_dev_epsilon_greedy_policy.cpp
)

target_link_libraries(
    test_dev_epsilon_greedy_policy
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_dev_epsilon_greedy_policy
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_epsilon_greedy_policy
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        test_dev_epsilon_greedy_policy
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()

# ============================================================
# DEVAI PHASE 3.8F — DEV TRAINING ENGINE TEST
# ============================================================

add_executable(
    test_dev_training_engine
    tests/strategy/dev_ai_strategy/test_dev_training_engine.cpp
)

target_link_libraries(
    test_dev_training_engine
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_dev_training_engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_training_engine
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_training_engine
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.8G — REAL HISTORICAL TRAINING VALIDATION
# ============================================================


add_executable(
    test_real_historical_training_validation

    tests/strategy/dev_ai_strategy/test_real_historical_training_validation.cpp

    # Market
    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # Features
    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # Statistics
    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # Ranking
    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_link_libraries(
    test_real_historical_training_validation
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_real_historical_training_validation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_real_historical_training_validation
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_real_historical_training_validation
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()


# ============================================================
# DEVAI PHASE 3.9
# ============================================================

add_executable(
    test_dev_model_persistence
    tests/strategy/dev_ai_strategy/test_dev_model_persistence.cpp
)

target_link_libraries(
    test_dev_model_persistence
    PRIVATE
        devai_strategy
)

target_compile_options(
    test_dev_model_persistence
    PRIVATE
        -Wall
        -Wextra
)

# =============================================================================
# DEVAI PHASE 3.10 — INITIAL CONTROLLED OUT-OF-SAMPLE VALIDATION
# =============================================================================

add_executable(
    test_dev_out_of_sample_validation
    tests/strategy/dev_ai_strategy/test_dev_out_of_sample_validation.cpp
)

target_link_libraries(
    test_dev_out_of_sample_validation
    PRIVATE
        devai_strategy
)

target_compile_options(
    test_dev_out_of_sample_validation
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# =============================================================================
# DEVAI PHASE 3.11 — TOP-5 SELECTION FOUNDATION
# =============================================================================

add_executable(
    test_dev_top_candidate_selector
    tests/strategy/dev_ai_strategy/test_dev_top_candidate_selector.cpp
)

target_link_libraries(
    test_dev_top_candidate_selector
    PRIVATE
        devai_strategy
)

target_compile_options(
    test_dev_top_candidate_selector
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# =============================================================================
# DEVAI PHASE 3.12 — DEVAGENTS ARCHITECTURE
# =============================================================================
add_executable(
    test_dev_agent_architecture
    tests/strategy/dev_ai_strategy/test_dev_agent_architecture.cpp
)

target_link_libraries(
    test_dev_agent_architecture
    PRIVATE
        devai_strategy
)

target_compile_options(
    test_dev_agent_architecture
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# =============================================================================
# DEVAI PHASE 3.13 — DEVMANAGER ARCHITECTURE
# =============================================================================

add_executable(
    test_dev_manager_architecture
    tests/strategy/dev_ai_strategy/test_dev_manager_architecture.cpp
)

target_link_libraries(
    test_dev_manager_architecture
    PRIVATE
        devai_strategy
)

target_compile_options(
    test_dev_manager_architecture
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# =============================================================================
# DEVAI PHASE 3.14 — DEVAGENT -> DEVMANAGER DECISION CONTRACT
# =============================================================================

add_executable(
    test_dev_agent_manager_contract
    tests/strategy/dev_ai_strategy/test_dev_agent_manager_contract.cpp
)

target_link_libraries(
    test_dev_agent_manager_contract
    PRIVATE
        devai_strategy
)

target_compile_options(
    test_dev_agent_manager_contract
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
# =============================================================================
# DEVAI PHASE 3.15A — HISTORICAL TRAINING DATASET / CACHE
# =============================================================================
add_executable(
    test_dev_historical_training_cache
    tests/strategy/dev_ai_strategy/test_dev_historical_training_cache.cpp
)

target_link_libraries(
    test_dev_historical_training_cache
    PRIVATE
        devai_strategy
)

target_compile_options(
    test_dev_historical_training_cache
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# ============================================================
# DEVAI PHASE 3.15B — REAL HISTORICAL CACHE GENERATOR
# ============================================================

add_executable(
    test_dev_real_historical_cache_generator

    tests/strategy/dev_ai_strategy/test_dev_real_historical_cache_generator.cpp

    # --------------------------------------------------------
    # CETE — MARKET DATA / MULTI-TIMEFRAME RUNTIME
    # --------------------------------------------------------

    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # --------------------------------------------------------
    # CETE — PHASE 3 FEATURE ENGINES
    # --------------------------------------------------------

    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp

    # --------------------------------------------------------
    # CETE — PHASE 4 MATHEMATICAL / STATISTICAL ENGINES
    # --------------------------------------------------------

    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # --------------------------------------------------------
    # CETE — PHASE 5 CROSS-SECTIONAL RANKING
    # --------------------------------------------------------

    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp
)

target_link_libraries(
    test_dev_real_historical_cache_generator
    PRIVATE
        devai_strategy
        cete_accounting
        cete_risk
        cete_outcome
)

target_include_directories(
    test_dev_real_historical_cache_generator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_real_historical_cache_generator
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_real_historical_cache_generator
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()


# ============================================================
# DEVAI PHASE 3.16 — SHARED DEVAGENT TRAINING
# ============================================================

add_executable(
    test_dev_shared_agent_training
    tests/strategy/dev_ai_strategy/test_dev_shared_agent_training.cpp
)

target_link_libraries(
    test_dev_shared_agent_training
    PRIVATE
        devai_strategy
)

target_include_directories(
    test_dev_shared_agent_training
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_shared_agent_training
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        test_dev_shared_agent_training
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ============================================================
# DEVAI PHASE 3.17
# ============================================================

add_executable(test_dev_manager_training
    tests/strategy/dev_ai_strategy/test_dev_manager_training.cpp
)

target_link_libraries(test_dev_manager_training PRIVATE devai_strategy)


# ============================================================================
# Phase 3.18 — Portfolio Rotation / 75% Allocation
# ============================================================================

add_executable(test_dev_portfolio_rotation
    tests/strategy/dev_ai_strategy/test_dev_portfolio_rotation.cpp
)

target_link_libraries(test_dev_portfolio_rotation
    PRIVATE
        devai_strategy
)

target_compile_options(test_dev_portfolio_rotation
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# ============================================================================
# Phase 3.19 
# ============================================================================

add_executable(test_dev_multi_stock_oos_validation
    tests/strategy/dev_ai_strategy/test_dev_multi_stock_oos_validation.cpp
)

target_link_libraries(test_dev_multi_stock_oos_validation PRIVATE devai_strategy)


# ============================================================================
# Phase 3.19B 
# ============================================================================
add_executable(test_dev_real_historical_multi_stock_oos
    tests/strategy/dev_ai_strategy/test_dev_real_historical_multi_stock_oos.cpp
)
target_link_libraries(test_dev_real_historical_multi_stock_oos PRIVATE devai_strategy)
target_compile_features(test_dev_real_historical_multi_stock_oos PRIVATE cxx_std_20)

# ============================================================================
# Phase 3.19C
# ============================================================================
add_executable(test_dev_historical_oos_runner
    tests/strategy/dev_ai_strategy/test_dev_historical_oos_runner.cpp
)
target_link_libraries(test_dev_historical_oos_runner PRIVATE devai_strategy)
target_compile_features(test_dev_historical_oos_runner PRIVATE cxx_std_20)


# ============================================================================
# Phase 3.19D
# ============================================================================

add_executable(test_dev_frozen_oos_policy
    tests/strategy/dev_ai_strategy/test_dev_frozen_oos_policy.cpp)
target_link_libraries(test_dev_frozen_oos_policy PRIVATE devai_strategy)

add_executable(run_dev_frozen_historical_oos
    tests/strategy/dev_ai_strategy/run_dev_frozen_historical_oos.cpp)
target_link_libraries(run_dev_frozen_historical_oos PRIVATE devai_strategy)

# ============================================================================
# Phase 3.19D-LS — Dedicated LONG + SHORT regression tests
# Append at the END of your existing ~/fmrss_cpp/CMakeLists.txt
# ============================================================================
add_executable(test_dev_long_short_regression
    tests/strategy/dev_ai_strategy/test_dev_long_short_regression.cpp
)
target_link_libraries(test_dev_long_short_regression PRIVATE devai_strategy)
target_compile_features(test_dev_long_short_regression PRIVATE cxx_std_20)


# ============================================================================
# Phase 3.19 Shared DevAI Neural Network Alignment
# ============================================================================

add_executable(test_dev_shared_neural_network
    tests/strategy/dev_ai_strategy/test_dev_shared_neural_network.cpp
)

target_link_libraries(
    test_dev_shared_neural_network
    PRIVATE devai_strategy
)

# ============================================================================
# Phase 3.19D-3B — Real Historical Training Driver
# ============================================================================

add_executable(run_dev_real_historical_training
    apps/run_dev_real_historical_training.cpp)
target_link_libraries(run_dev_real_historical_training PRIVATE devai_strategy)

# ============================================================
# CETE Phase 3.19D-3B-1
# Real Historical Cache Generator
# ============================================================

add_executable(
    run_dev_real_historical_cache_generator

    apps/run_dev_real_historical_cache_generator.cpp

    # Phase 1 - Market Data Foundation
    src/market/MarketDataLoader.cpp
    src/market/CandleAggregator.cpp
    src/market/MultiTimeframeSynchronizer.cpp
    src/market/MultiTimeframeCursor.cpp
    src/market/SessionState.cpp
    src/market/MarketSnapshotBuilder.cpp

    # Phase 3 — Feature Engines
    src/features/PriceReturnFeatureEngine.cpp
    src/features/TrendMomentumFeatureEngine.cpp
    src/features/VolumeFeatureEngine.cpp
    src/features/VolatilityFeatureEngine.cpp
    src/features/StockBenchmarkFeatureEngine.cpp

    # Phase 4 — Statistical Engines
    src/statistics/RollingStatisticalEngine.cpp
    src/statistics/ReturnStatisticalEngine.cpp
    src/statistics/VolumeStatisticalEngine.cpp
    src/statistics/StockBenchmarkStatisticalEngine.cpp
    src/statistics/MarketRegimeEngine.cpp
    src/statistics/MathematicalStateComposer.cpp

    # Phase 5 — Cross-Sectional Ranking
    src/ranking/CrossSectionalRanker.cpp
    src/ranking/CrossSectionalMathematicalRanker.cpp
    src/ranking/CrossSectionalMathematicalStateComposer.cpp
    src/ranking/CrossSectionalUniverseSnapshotBuilder.cpp

)

target_include_directories(
    run_dev_real_historical_cache_generator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    run_dev_real_historical_cache_generator
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    run_dev_real_historical_cache_generator
    PRIVATE
        devai_strategy
)


# ============================================================
# CETE Phase 3.19D-3B-2
# Single-Pass Historical Replay.
# ============================================================

get_target_property(CETE_319D3B1_SOURCES run_dev_real_historical_cache_generator SOURCES)
list(REMOVE_ITEM CETE_319D3B1_SOURCES
    apps/run_dev_real_historical_cache_generator.cpp)
# H4.7 Stage 5B: link persistent mathematical feature caching once here.
# H4.7 ranking and replay probe inherit this target's source list below.
add_executable(run_dev_single_pass_historical_cache
    apps/run_dev_single_pass_historical_cache.cpp
    src/strategy/dev_ai_strategy/HistoricalCETESinglePassReplay.cpp
    src/cache/HistoricalMathematicalFeatureCache.cpp
    ${CETE_319D3B1_SOURCES}
)
target_include_directories(run_dev_single_pass_historical_cache PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_single_pass_historical_cache PRIVATE cxx_std_20)
target_link_libraries(run_dev_single_pass_historical_cache PRIVATE devai_strategy)

# ============================================================
# CETE Phase 3.19D-3B-3 - Historical Outcomes and Reward Validation
# ============================================================
add_executable(run_dev_historical_outcomes_rewards
    apps/run_dev_historical_outcomes_rewards.cpp
    src/strategy/dev_ai_strategy/DevHistoricalOutcomeRewardValidator.cpp
)
target_include_directories(run_dev_historical_outcomes_rewards PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_historical_outcomes_rewards PRIVATE cxx_std_20)
target_link_libraries(run_dev_historical_outcomes_rewards PRIVATE devai_strategy)

# ============================================================
# CETE Phase 3.19D-3B-4A — offline shared-network warm start
# ============================================================

add_executable(run_dev_shared_model_warmstart
    apps/run_dev_shared_model_warmstart.cpp)
target_include_directories(run_dev_shared_model_warmstart PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_shared_model_warmstart PRIVATE cxx_std_20)
target_link_libraries(run_dev_shared_model_warmstart PRIVATE devai_strategy)



# ============================================================
# CETE Phase 3.19D-3B-4B 
# ============================================================
add_library(dev_portfolio_environment STATIC
    src/strategy/dev_ai_strategy/DevPortfolioEnvironment.cpp)
target_include_directories(dev_portfolio_environment PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_portfolio_environment PUBLIC cxx_std_20)
add_executable(test_dev_portfolio_environment
    apps/test_dev_portfolio_environment.cpp)
target_link_libraries(test_dev_portfolio_environment PRIVATE dev_portfolio_environment)

# ============================================================
# CETE Phase 3.19D-3B-4C Historical Portfolio Replay Integration
# ============================================================
# Append ONCE to root CMakeLists.txt. Requires prior 3B4B dev_portfolio_environment target.
add_executable(run_dev_historical_portfolio_replay
    apps/run_dev_historical_portfolio_replay.cpp
    src/strategy/dev_ai_strategy/DevHistoricalPortfolioReplay.cpp)
target_include_directories(run_dev_historical_portfolio_replay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_historical_portfolio_replay PRIVATE cxx_std_20)
target_link_libraries(run_dev_historical_portfolio_replay PRIVATE
    dev_portfolio_environment devai_strategy)

# ============================================================
# DEVAI — Devinderjeet SUPERVISOR 
# ============================================================
# Append near the bottom of your root CMakeLists.txt.
# This standalone test target avoids guessing existing library target names.
add_executable(test_devinderjeet_supervisor
    apps/test_devinderjeet_supervisor.cpp
)
target_link_libraries(test_devinderjeet_supervisor PRIVATE devai_strategy)
target_include_directories(test_devinderjeet_supervisor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_devinderjeet_supervisor PRIVATE cxx_std_20)

# Add source to your existing devai_strategy target (once):
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevManagerSupervisorBridge.cpp
)

# ============================================================
# DEVAI — SUPERVISOR AND MANAGER INTEGRATION
# ============================================================

add_executable(test_dev_manager_supervisor_integration
    tests/strategy/dev_ai_strategy/test_dev_manager_supervisor_integration.cpp)
target_link_libraries(test_dev_manager_supervisor_integration PRIVATE devai_strategy)
target_compile_features(test_dev_manager_supervisor_integration PRIVATE cxx_std_20)


# ============================================================
# CETE/DevAI — Shared-Model Multi-Manager Integration
# ============================================================

# Append ONCE to the root CMakeLists.txt (after devai_strategy is defined).
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSharedModelMultiManagerRuntime.cpp
)
add_executable(test_dev_shared_model_multi_manager_runtime
    tests/strategy/dev_ai_strategy/test_dev_shared_model_multi_manager_runtime.cpp)
target_link_libraries(test_dev_shared_model_multi_manager_runtime PRIVATE devai_strategy)
target_compile_features(test_dev_shared_model_multi_manager_runtime PRIVATE cxx_std_20)

# ============================================================
# CETE/DevAI — Shared-Model Multi-Manager Historical Replay
# ============================================================

# Add once AFTER the existing devai_strategy library definition:
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSharedMultiManagerHistoricalReplay.cpp
)
add_executable(test_dev_shared_multi_manager_historical_replay
    tests/strategy/dev_ai_strategy/test_dev_shared_multi_manager_historical_replay.cpp
)
target_link_libraries(test_dev_shared_multi_manager_historical_replay PRIVATE devai_strategy)
target_compile_features(test_dev_shared_multi_manager_historical_replay PRIVATE cxx_std_20)


# ============================================================
# CETE/DevAI — Real Historical Shared-Model Multi-Manager Replay Integration
# ============================================================



# Append ONCE to your root CMakeLists.txt, after devai_strategy exists.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevRealMarketReplayAdapter.cpp
)
add_executable(test_dev_real_market_replay_adapter
    tests/strategy/dev_ai_strategy/test_dev_real_market_replay_adapter.cpp
)
target_link_libraries(test_dev_real_market_replay_adapter PRIVATE devai_strategy)
target_compile_features(test_dev_real_market_replay_adapter PRIVATE cxx_std_20)

# ============================================================
# CETE/DevAI Phase 3.19D-3B-4C-1 — Supervisor Settlement Integration
# ============================================================

# Append once after devai_strategy is declared.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSupervisorTerminalSettlement.cpp
)
add_executable(test_dev_supervisor_terminal_settlement
    tests/strategy/dev_ai_strategy/test_dev_supervisor_terminal_settlement.cpp
)
target_link_libraries(test_dev_supervisor_terminal_settlement PRIVATE devai_strategy)
target_compile_features(test_dev_supervisor_terminal_settlement PRIVATE cxx_std_20)

# ============================================================
# CETE/DevAI — 3.19D-3B-4C-2: Single-Manager Top-5 Real Historical Replay.
# ============================================================
# Append ONCE to root CMakeLists.txt after devai_strategy is defined.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSingleManagerTop5Replay.cpp)
add_executable(test_dev_single_manager_top5_replay
    tests/strategy/dev_ai_strategy/test_dev_single_manager_top5_replay.cpp)
target_link_libraries(test_dev_single_manager_top5_replay PRIVATE devai_strategy)
target_compile_features(test_dev_single_manager_top5_replay PRIVATE cxx_std_20)

# ============================================================
# CETE/DevAI — 3.19D-3B-4C-2B — Shared Replay Integrity
# ============================================================
# Append ONCE to root CMakeLists.txt; devai_strategy must already exist.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSharedReplayIntegrity.cpp
)
add_executable(test_dev_shared_replay_integrity
    tests/strategy/dev_ai_strategy/test_dev_shared_replay_integrity.cpp
)
target_link_libraries(test_dev_shared_replay_integrity PRIVATE devai_strategy)
target_compile_features(test_dev_shared_replay_integrity PRIVATE cxx_std_20)


# ============================================================
# CETE/DevAI — 3.19D-3B-4C-3 — Event-Driven Execution and Position Rotation
# ============================================================
# Add once after the devai_strategy library target exists.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevEventDrivenRotationReplay.cpp
)
add_executable(test_dev_event_driven_rotation_replay
    tests/strategy/dev_ai_strategy/test_dev_event_driven_rotation_replay.cpp
)
target_link_libraries(test_dev_event_driven_rotation_replay PRIVATE devai_strategy)
target_compile_features(test_dev_event_driven_rotation_replay PRIVATE cxx_std_20)

# ============================================================
# CETE/DevAI — 3.19D-3B-4C-4: End-to-End Historical Experience Integration
# ============================================================

# Append once after devai_strategy target exists and earlier 4C2A/4C3 sources are included.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.cpp
)
add_executable(test_dev_historical_experience_integration
    tests/strategy/dev_ai_strategy/test_dev_historical_experience_integration.cpp
)
target_link_libraries(test_dev_historical_experience_integration PRIVATE devai_strategy)
target_compile_features(test_dev_historical_experience_integration PRIVATE cxx_std_20)

# ============================================================
# CETE/DevAI — 3.19D-3: DevAgent and DevManager RL Training
# ============================================================

# ============================================================
# CETE/DevAI — Phase 3.19D-3
# DevAgent and DevManager Reinforcement Learning Training
#
# Architecture:
#   One shared DevAgent network (DevAgent_001–005)
#   One DevManager network (DevManager_001)
#   DevinderjeetSupervisor (rule-based, not trained)
# ============================================================

# Add training integration to the existing DevAI strategy library
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSupervisorRewardedTraining.cpp
)

# Training integration regression test
add_executable(test_dev_supervisor_rewarded_training
    tests/strategy/dev_ai_strategy/test_dev_supervisor_rewarded_training.cpp
)

# Link against the existing DevAI strategy library
target_link_libraries(test_dev_supervisor_rewarded_training
    PRIVATE devai_strategy
)

# Require C++20
target_compile_features(test_dev_supervisor_rewarded_training
    PRIVATE cxx_std_20
)

# ============================================================
# CCETE/DevAI — Phase 3.19D-3: Sequential Reinforcement Learning Training
# ============================================================

# Append ONCE after devai_strategy exists.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSequentialRLTrainer.cpp)
add_executable(train_devai_sequential apps/train_devai_sequential.cpp)
target_link_libraries(train_devai_sequential PRIVATE devai_strategy)
target_compile_features(train_devai_sequential PRIVATE cxx_std_20)

# ============================================================
# H4.7 Stage 5E.2K — Multi-Session Historical Cache Assembly
# ============================================================
add_executable(assemble_dev_historical_caches
    apps/assemble_dev_historical_caches.cpp
)
target_link_libraries(assemble_dev_historical_caches PRIVATE devai_strategy)
target_compile_features(assemble_dev_historical_caches PRIVATE cxx_std_20)


# ============================================================
# CETE/DevAI — Revised Multi-Manager Architecture
# ============================================================

target_sources(devai_strategy PRIVATE src/strategy/dev_ai_strategy/DevManagerRegistry.cpp)
add_executable(test_dev_manager_registry tests/strategy/dev_ai_strategy/test_dev_manager_registry.cpp)
target_link_libraries(test_dev_manager_registry PRIVATE devai_strategy)
target_compile_features(test_dev_manager_registry PRIVATE cxx_std_20)


# ============================================================
# CETE 5C — DevAISupervisor integration package
# ============================================================
# Append ONCE to root CMakeLists.txt, after devai_strategy is defined.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevAISupervisor.cpp
)
add_executable(test_dev_ai_supervisor
    tests/strategy/dev_ai_strategy/test_dev_ai_supervisor.cpp
)
target_link_libraries(test_dev_ai_supervisor PRIVATE devai_strategy)
target_compile_features(test_dev_ai_supervisor PRIVATE cxx_std_20)

# ============================================================
# CETE 5C-2 — DevinderjeetSupervisor Core Migration
# ============================================================
# The canonical implementation lives in CETE Core:
#   include/cete/supervision/DevinderjeetSupervisor.hpp
#   src/cete/supervision/DevinderjeetSupervisor.cpp
# The previous DevAI header is a compatibility forwarding header.
# Existing targets above already compile the relocated source. Do NOT
# append it to devai_strategy a second time or compile the retired .cpp.
add_executable(test_cete_supervisor_core_migration
    tests/cete/supervision/test_cete_supervisor_core_migration.cpp
    src/cete/supervision/DevinderjeetSupervisor.cpp
)
target_include_directories(test_cete_supervisor_core_migration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(test_cete_supervisor_core_migration PRIVATE cxx_std_20)

# ============================================================
# CETE 5C-3 — DevAISupervisor Runtime Integration
# ============================================================
# Append ONCE after devai_strategy and the existing 5C/5C-2 targets.
# Existing DevSharedModelMultiManagerRuntime.cpp must already be included.
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevAISupervisedRuntime.cpp
)
add_executable(test_dev_ai_supervised_runtime
    tests/strategy/dev_ai_strategy/test_dev_ai_supervised_runtime.cpp
)
target_link_libraries(test_dev_ai_supervised_runtime PRIVATE devai_strategy)
target_compile_features(test_dev_ai_supervised_runtime PRIVATE cxx_std_20)


# ============================================================
# CETE 5C-3B — Real Multi-Manager Runtime Integration Validation
# ============================================================
# Add ONCE at the end of the existing root CMakeLists.txt.
# Requires CETE 5C-2 and 5C-3 source targets already registered.
add_executable(test_dev_ai_supervised_multi_manager_integration
    tests/strategy/dev_ai_strategy/test_dev_ai_supervised_multi_manager_integration.cpp
)
target_link_libraries(test_dev_ai_supervised_multi_manager_integration PRIVATE devai_strategy)
target_compile_features(test_dev_ai_supervised_multi_manager_integration PRIVATE cxx_std_20)

# ============================================================
# CETE 5C-3C — DevAI Configuration Bootstrap (append ONCE)
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevAIConfigurationBootstrap.cpp
)
add_executable(test_dev_ai_configuration_bootstrap
    tests/strategy/dev_ai_strategy/test_dev_ai_configuration_bootstrap.cpp
)
target_link_libraries(test_dev_ai_configuration_bootstrap PRIVATE devai_strategy)
target_compile_features(test_dev_ai_configuration_bootstrap PRIVATE cxx_std_20)

# ============================================================
# CETE 5C-3C — Manager status report
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevAIManagerStatusReport.cpp
)
add_executable(test_dev_ai_manager_status_report
    tests/strategy/dev_ai_strategy/test_dev_ai_manager_status_report.cpp
)
target_link_libraries(test_dev_ai_manager_status_report PRIVATE devai_strategy)
target_compile_features(test_dev_ai_manager_status_report PRIVATE cxx_std_20)


# ============================================================
# CETE 5C-3C — Main executable: manager status inspection
# ============================================================
# Reuse the existing DevAI library, which already includes
# DevManagerRegistry and DevAIManagerStatusReport.
# This does not start training or enable live trading.
target_link_libraries(fmrss_cpp PRIVATE devai_strategy)


# ============================================================
# CETE 5C-3D — Persistent manager registration utility
# ============================================================
add_executable(cete_register_dev_manager_001
    tools/register_dev_manager_001.cpp
)
target_link_libraries(cete_register_dev_manager_001 PRIVATE devai_strategy)

add_executable(test_dev_manager_registration_5c3d
    tests/strategy/dev_ai_strategy/test_dev_manager_registration_5c3d.cpp
)
target_link_libraries(test_dev_manager_registration_5c3d PRIVATE devai_strategy)

# ============================================================
# CETE 5C-3E — Controlled manager activation (append once)
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevManagerActivationController.cpp
)
add_executable(test_dev_manager_activation_5c3e
    tests/strategy/dev_ai_strategy/test_dev_manager_activation_5c3e.cpp
)
target_link_libraries(test_dev_manager_activation_5c3e PRIVATE devai_strategy)

# ============================================================
# CETE 5C-3F — Persistent Model Approval Verification
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevModelApprovalVerifier.cpp
)

add_executable(test_dev_model_approval_5c3f
    tests/strategy/dev_ai_strategy/test_dev_model_approval_5c3f.cpp
)

target_link_libraries(test_dev_model_approval_5c3f
    PRIVATE devai_strategy
)


# ============================================================
# CETE 5C-3G — Approval and Runtime Eligibility
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevApprovedRuntimeGateway.cpp
)

add_executable(test_dev_approved_runtime_5c3g
    tests/strategy/dev_ai_strategy/test_dev_approved_runtime_5c3g.cpp
)

target_link_libraries(test_dev_approved_runtime_5c3g
    PRIVATE devai_strategy
)

# ============================================================
# CETE 5C-3H — Secure Execution Entry Point
# Append once. Requires existing devai_strategy and 5C-3G.
# ============================================================
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/DevSecureExecutionPath.cpp
)
add_executable(test_dev_secure_execution_path_5c3h
    tests/strategy/dev_ai_strategy/test_dev_secure_execution_path_5c3h.cpp
)
target_link_libraries(test_dev_secure_execution_path_5c3h PRIVATE devai_strategy)

# ============================================================
# CETE Phase 5C-3H-1 — HOLD Decision Regression Test
# ============================================================

add_executable(test_dev_hold_decision_5c3h1
    tests/strategy/dev_ai_strategy/test_dev_hold_decision_5c3h1.cpp
)

target_include_directories(test_dev_hold_decision_5c3h1
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(test_dev_hold_decision_5c3h1
    PRIVATE cxx_std_17
)

target_link_libraries(test_dev_hold_decision_5c3h1
    PRIVATE devai_strategy
)

# ============================================================
# CETE Phase 5C-3H-3 — append once after devai_strategy is defined
# ============================================================
add_executable(test_dev_rl_reward_integrity_5c3h3
    tests/strategy/dev_ai_strategy/test_dev_rl_reward_integrity_5c3h3.cpp
)
target_link_libraries(test_dev_rl_reward_integrity_5c3h3 PRIVATE devai_strategy)
target_compile_features(test_dev_rl_reward_integrity_5c3h3 PRIVATE cxx_std_20)


# ============================================================
# CETE Phase 5C-3H — Final Freeze Validation
# ============================================================
# Append ONCE to CMakeLists.txt
add_executable(test_dev_freeze_cost_reward_5c3h
    tests/strategy/dev_ai_strategy/test_dev_freeze_cost_reward_5c3h.cpp
)
target_link_libraries(test_dev_freeze_cost_reward_5c3h PRIVATE devai_strategy)
target_compile_features(test_dev_freeze_cost_reward_5c3h PRIVATE cxx_std_20)


# ============================================================
# CETE Phase 5C-3I-A — RL Training Readiness
# ============================================================

# Append to the root CMakeLists.txt after the existing targets.
add_executable(test_dev_rl_training_readiness_5c3ia
    src/strategy/dev_ai_strategy/DevRLTrainingReadiness.cpp
    tests/strategy/dev_ai_strategy/test_dev_rl_training_readiness_5c3ia.cpp)
target_compile_features(test_dev_rl_training_readiness_5c3ia PRIVATE cxx_std_20)
target_include_directories(test_dev_rl_training_readiness_5c3ia PRIVATE include)



# ============================================================
# Phase 5C-3I-B —Configurable Multi-Manager Contract
# ============================================================
# Add ONCE to root CMakeLists.txt. Standalone contract target.
add_executable(test_dev_configurable_phase4_integration_5c3ib
    tests/strategy/dev_ai_strategy/test_dev_configurable_phase4_integration_5c3ib.cpp
    src/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.cpp)
target_include_directories(test_dev_configurable_phase4_integration_5c3ib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_configurable_phase4_integration_5c3ib PRIVATE cxx_std_17)

# ============================================================
# CETE Phase 5C-3I-C — Real Historical Multi-Manager Strategy Integration.
# ============================================================
# Existing Phase 5C-3I-B target must remain.
add_executable(test_dev_historical_manager_strategy_adapter_5c3ic
    tests/strategy/dev_ai_strategy/test_dev_historical_manager_strategy_adapter_5c3ic.cpp
    src/strategy/dev_ai_strategy/DevHistoricalManagerStrategyAdapter.cpp
    src/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.cpp
)
target_include_directories(test_dev_historical_manager_strategy_adapter_5c3ic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_historical_manager_strategy_adapter_5c3ic PRIVATE cxx_std_20)
# If your existing project links the configurable integration through a library,
# link that library instead of compiling its .cpp twice in the same target.


# ============================================================
# CETE Phase Phase 5C-3I-D — Real Statistical Engine Integration
# ============================================================
# Add to root CMakeLists.txt ONCE after identifying your actual ranker library target.
# Do NOT add this source to both a linked library and an executable.
# Replace CETE_RANKER_LIBRARY_TARGET with the existing target containing
# src/ranking/CrossSectionalMathematicalRanker.cpp and its dependencies.
# target_sources(YOUR_EXISTING_MANAGER_LIBRARY PRIVATE
#     src/strategy/dev_ai_strategy/DevRealStatisticalStrategyBridge.cpp)
# target_link_libraries(YOUR_EXISTING_MANAGER_LIBRARY PRIVATE CETE_RANKER_LIBRARY_TARGET)


# ============================================================
# CETE Phase 5C-3I-D2 — Statistical Observations and Configurable Candidate Integration
# ============================================================
# Append ONCE to the root CMakeLists.txt.
add_executable(test_dev_statistical_observation_integration_5c3id2
    tests/strategy/dev_ai_strategy/test_dev_statistical_observation_integration_5c3id2.cpp
    src/strategy/dev_ai_strategy/DevStatisticalObservationIntegration.cpp
    src/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.cpp
    src/strategy/dev_ai_strategy/ObservationBuilder.cpp)
target_include_directories(test_dev_statistical_observation_integration_5c3id2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_statistical_observation_integration_5c3id2 PRIVATE cxx_std_20)
# Do not duplicate the .cpp sources in this target if instead linking a library containing them.

# ============================================================
# CETE Phase 5C-3I-D3 — DevManager-Controlled Historical Multi-Stock Statistical Integration.
# ============================================================
# Add this only ONCE to your root CMakeLists.txt.
# This target compiles its direct implementation dependencies separately to avoid
# assumptions about your existing project library target names.
add_executable(test_dev_manager_historical_statistical_5c3id3
    tests/strategy/dev_ai_strategy/test_dev_manager_historical_statistical_5c3id3.cpp
    src/strategy/dev_ai_strategy/DevManagerHistoricalStatisticalIntegration.cpp
    src/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.cpp
    src/strategy/dev_ai_strategy/ObservationBuilder.cpp
)
target_include_directories(test_dev_manager_historical_statistical_5c3id3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_manager_historical_statistical_5c3id3 PRIVATE cxx_std_17)


# ============================================================
# Phase 5C-3I-D4: append to root CMakeLists.txt exactly once.
# ============================================================

add_library(dev_historical_minute_validator_5c3id4 STATIC
    src/strategy/dev_ai_strategy/DevHistoricalMinuteValidator.cpp)
target_include_directories(dev_historical_minute_validator_5c3id4 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_historical_minute_validator_5c3id4 PUBLIC cxx_std_17)

add_executable(test_dev_historical_minute_validator_5c3id4
    tests/strategy/dev_ai_strategy/test_dev_historical_minute_validator_5c3id4.cpp)
target_link_libraries(test_dev_historical_minute_validator_5c3id4 PRIVATE
    dev_historical_minute_validator_5c3id4)

add_executable(run_dev_historical_minute_validation_5c3id4
    tests/strategy/dev_ai_strategy/run_dev_historical_minute_validation_5c3id4.cpp)
target_link_libraries(run_dev_historical_minute_validation_5c3id4 PRIVATE
    dev_historical_minute_validator_5c3id4)

# ============================================================
# CETE/DevAI Phase 5C-3I-D5 — Real Historical Multi-Stock Statistical Integration.
# ============================================================

# Append ONCE to ~/fmrss_cpp/CMakeLists.txt; requires existing D4 R2 sources.
add_library(dev_real_historical_multi_stock_5c3id5 STATIC
    src/strategy/dev_ai_strategy/DevRealHistoricalMultiStockFeed.cpp
    src/strategy/dev_ai_strategy/DevHistoricalMinuteValidator.cpp)
target_include_directories(dev_real_historical_multi_stock_5c3id5 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_real_historical_multi_stock_5c3id5 PUBLIC cxx_std_17)
add_executable(test_dev_real_historical_multi_stock_5c3id5 tests/strategy/dev_ai_strategy/test_dev_real_historical_multi_stock_5c3id5.cpp)
target_link_libraries(test_dev_real_historical_multi_stock_5c3id5 PRIVATE dev_real_historical_multi_stock_5c3id5)
add_executable(run_dev_real_historical_multi_stock_5c3id5 tests/strategy/dev_ai_strategy/run_dev_real_historical_multi_stock_5c3id5.cpp)
target_link_libraries(run_dev_real_historical_multi_stock_5c3id5 PRIVATE dev_real_historical_multi_stock_5c3id5)


# ============================================================
# CETE/DevAI  Phase 5C-3I-D6 — DevManager_001 Real Statistical Strategy and Ranking Integration.
# ============================================================
# Append ONCE to existing ~/fmrss_cpp/CMakeLists.txt after D5 targets.
add_library(dev_manager001_historical_ranking_5c3id6 STATIC
    src/strategy/dev_ai_strategy/DevManager001HistoricalRanking.cpp)
target_include_directories(dev_manager001_historical_ranking_5c3id6 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_manager001_historical_ranking_5c3id6 PUBLIC cxx_std_17)
target_link_libraries(dev_manager001_historical_ranking_5c3id6 PUBLIC dev_real_historical_multi_stock_5c3id5)
add_executable(test_dev_manager001_historical_ranking_5c3id6
    tests/strategy/dev_ai_strategy/test_dev_manager001_historical_ranking_5c3id6.cpp)
target_link_libraries(test_dev_manager001_historical_ranking_5c3id6 PRIVATE dev_manager001_historical_ranking_5c3id6)
add_executable(run_dev_manager001_historical_ranking_5c3id6
    tests/strategy/dev_ai_strategy/run_dev_manager001_historical_ranking_5c3id6.cpp)
target_link_libraries(run_dev_manager001_historical_ranking_5c3id6 PRIVATE dev_manager001_historical_ranking_5c3id6)


# ============================================================
# CETE/DevAI Phase 5C-3I-D7 — DevManager_001 Actual Mathematical/Statistical Strategy Integration.
# ============================================================
# Append once; D5 and D6 must already be registered.
add_library(dev_manager001_statistical_strategy_5c3id7 STATIC
    src/strategy/dev_ai_strategy/DevManager001StatisticalStrategyD7.cpp)
target_include_directories(dev_manager001_statistical_strategy_5c3id7 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_manager001_statistical_strategy_5c3id7 PUBLIC cxx_std_17)
target_link_libraries(dev_manager001_statistical_strategy_5c3id7 PUBLIC dev_manager001_historical_ranking_5c3id6)
add_executable(test_dev_manager001_statistical_strategy_5c3id7
    tests/strategy/dev_ai_strategy/test_dev_manager001_statistical_strategy_5c3id7.cpp)
target_link_libraries(test_dev_manager001_statistical_strategy_5c3id7 PRIVATE dev_manager001_statistical_strategy_5c3id7)



# ============================================================
# CETE/DevAI Phase 5C-3I-D7B — Real Historical Statistical Integration
# ============================================================
# Append ONCE after D7 targets in ~/fmrss_cpp/CMakeLists.txt
add_library(dev_manager001_historical_strategy_5c3id7b STATIC
    src/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.cpp)
target_include_directories(dev_manager001_historical_strategy_5c3id7b PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_manager001_historical_strategy_5c3id7b PUBLIC cxx_std_17)
target_link_libraries(dev_manager001_historical_strategy_5c3id7b PUBLIC
    dev_manager001_statistical_strategy_5c3id7
    dev_manager001_historical_ranking_5c3id6)
add_executable(test_dev_manager001_historical_strategy_5c3id7b
    tests/strategy/dev_ai_strategy/test_dev_manager001_historical_strategy_5c3id7b.cpp)
target_link_libraries(test_dev_manager001_historical_strategy_5c3id7b PRIVATE dev_manager001_historical_strategy_5c3id7b)
add_executable(run_dev_manager001_historical_strategy_5c3id7b
    tests/strategy/dev_ai_strategy/run_dev_manager001_historical_strategy_5c3id7b.cpp)
target_link_libraries(run_dev_manager001_historical_strategy_5c3id7b PRIVATE dev_manager001_historical_strategy_5c3id7b)



# ============================================================
# CETE D7C one-minute decision synchronization PATCH
# ============================================================

add_executable(test_dev_manager001_one_minute_sync_5c3id7c
  tests/strategy/dev_ai_strategy/test_dev_manager001_one_minute_sync_5c3id7c.cpp)
target_link_libraries(test_dev_manager001_one_minute_sync_5c3id7c PRIVATE
  dev_manager001_historical_strategy_5c3id7b)


# ============================================================
# CETE D7C — Real Historical Validation
# ============================================================
add_executable(run_dev_manager001_real_one_minute_5c3id7c tests/strategy/dev_ai_strategy/run_dev_manager001_real_one_minute_5c3id7c.cpp)
target_link_libraries(run_dev_manager001_real_one_minute_5c3id7c PRIVATE dev_manager001_historical_strategy_5c3id7b)

# ============================================================
# CETE D7D — Ten-Stock One-Minute OOS Validation
# ============================================================
add_executable(run_dev_manager001_ten_stock_oos_5c3id7d
    tests/strategy/dev_ai_strategy/run_dev_manager001_ten_stock_oos_5c3id7d.cpp)
target_link_libraries(run_dev_manager001_ten_stock_oos_5c3id7d
    PRIVATE dev_manager001_historical_strategy_5c3id7b)

# ============================================================
# Phase 5C-3I-D8 — Causal Forward-Return Prediction and Top-5 Price Forecasting Engine
# ============================================================

add_library(dev_causal_forward_return_5c3id8 STATIC
    src/strategy/dev_ai_strategy/DevCausalForwardReturnD8.cpp
    src/strategy/dev_ai_strategy/DevManager001Top5ForecastD8.cpp)
target_include_directories(dev_causal_forward_return_5c3id8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_causal_forward_return_5c3id8 PUBLIC cxx_std_17)
target_link_libraries(dev_causal_forward_return_5c3id8 PUBLIC dev_manager001_historical_ranking_5c3id6)

add_executable(test_dev_causal_forward_return_5c3id8
    tests/strategy/dev_ai_strategy/test_dev_causal_forward_return_5c3id8.cpp)
target_link_libraries(test_dev_causal_forward_return_5c3id8 PRIVATE dev_causal_forward_return_5c3id8)

add_executable(test_dev_manager001_top5_forecast_5c3id8
    tests/strategy/dev_ai_strategy/test_dev_manager001_top5_forecast_5c3id8.cpp)
target_link_libraries(test_dev_manager001_top5_forecast_5c3id8 PRIVATE dev_causal_forward_return_5c3id8)

add_executable(run_dev_manager001_causal_forecast_5c3id8
    tests/strategy/dev_ai_strategy/run_dev_manager001_causal_forecast_5c3id8.cpp)
target_link_libraries(run_dev_manager001_causal_forecast_5c3id8 PRIVATE
    dev_causal_forward_return_5c3id8 dev_manager001_historical_strategy_5c3id7b)

add_executable(run_dev_manager001_top5_forecast_5c3id8
    tests/strategy/dev_ai_strategy/run_dev_manager001_top5_forecast_5c3id8.cpp)
target_link_libraries(run_dev_manager001_top5_forecast_5c3id8 PRIVATE
    dev_causal_forward_return_5c3id8 dev_real_historical_multi_stock_5c3id5)


# ============================================================
# Phase 5C-3I-D8E — Forecast Quality and Economic Validation and 
# followed by D9 — Shared Reinforcement Learning Model Training
# ============================================================
# Add once. D8E source is reused from the original D8E/D9 research package.
add_library(dev_d9a_intraday_policy STATIC
    src/strategy/dev_ai_strategy/DevIntradayPolicyD9A.cpp)
target_include_directories(dev_d9a_intraday_policy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_d9a_intraday_policy PUBLIC cxx_std_17)
add_executable(test_dev_d9a_intraday_policy
    tests/strategy/dev_ai_strategy/test_dev_d9a_intraday_policy.cpp)
target_link_libraries(test_dev_d9a_intraday_policy PRIVATE dev_d9a_intraday_policy)
if(TARGET dev_d8e_d9_research)
    set(DEV_D8E_VALIDATION_LIB dev_d8e_d9_research)
else()
    add_library(dev_d8e_validation STATIC src/strategy/dev_ai_strategy/DevForecastValidationD8E.cpp)
    target_include_directories(dev_d8e_validation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(dev_d8e_validation PUBLIC cxx_std_17)
    set(DEV_D8E_VALIDATION_LIB dev_d8e_validation)
endif()
add_executable(test_dev_d8e_validation tests/strategy/dev_ai_strategy/test_dev_d8e_validation.cpp)
target_link_libraries(test_dev_d8e_validation PRIVATE ${DEV_D8E_VALIDATION_LIB})

# ============================================================
# Phase 5C-3I-D9B: independent shared Q-learning environment + D9A policy types
# ============================================================
# Append once, after D9A has been registered.
add_library(dev_d9b_position_aware STATIC
    src/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.cpp)
target_include_directories(dev_d9b_position_aware PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_d9b_position_aware PUBLIC cxx_std_17)
add_executable(test_dev_d9b_position_aware
    tests/strategy/dev_ai_strategy/test_dev_d9b_position_aware.cpp)
target_link_libraries(test_dev_d9b_position_aware PRIVATE dev_d9b_position_aware)

# ============================================================
# Phase 5C-3I-D9C — Ten-Stock Historical Shared RL Training and Hyperparameter Optimization
# ============================================================
# D9C uses the existing CETE D8 and D9B targets; no source overwrite.
add_executable(export_dev_d9c_causal_dataset
    tests/strategy/dev_ai_strategy/export_dev_d9c_causal_dataset.cpp)
target_link_libraries(export_dev_d9c_causal_dataset PRIVATE
    dev_causal_forward_return_5c3id8 dev_real_historical_multi_stock_5c3id5)
add_executable(run_dev_d9c_shared_rl_training
    tests/strategy/dev_ai_strategy/run_dev_d9c_shared_rl_training.cpp)
target_link_libraries(run_dev_d9c_shared_rl_training PRIVATE
    dev_d9b_position_aware)

# ============================================================
# Phase CETE D9C — Three Operating Modes
# ============================================================
# Append to root CMakeLists.txt exactly once.
add_library(dev_d9c_execution_gateway STATIC
  src/strategy/dev_ai_strategy/DevExecutionGatewayD9C.cpp)
target_include_directories(dev_d9c_execution_gateway PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dev_d9c_execution_gateway PUBLIC cxx_std_17)
add_executable(test_dev_d9c_modes
  tests/strategy/dev_ai_strategy/test_dev_d9c_modes.cpp)
target_link_libraries(test_dev_d9c_modes PRIVATE dev_d9c_execution_gateway cete_brokerage)

# ============================================================
# CETE / DevAI — D9F Portfolio and RL Learning Correction
# ============================================================

add_library(devai_d9f_engine STATIC
    src/strategy/dev_ai_strategy/DevManagerRL_D9F.cpp
    src/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.cpp
    src/strategy/brokerage/BrokerageChargesEngine.cpp
)

target_include_directories(devai_d9f_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/strategy/dev_ai_strategy
)

target_compile_features(devai_d9f_engine PUBLIC cxx_std_17)

# Historical portfolio training and validation runner
add_executable(run_dev_d9f_portfolio
    tests/strategy/dev_ai_strategy/run_dev_d9f_portfolio.cpp
)

target_link_libraries(run_dev_d9f_portfolio
    PRIVATE devai_d9f_engine
)

# Manager Q-learning unit tests
add_executable(test_dev_d9f_manager_q
    tests/strategy/dev_ai_strategy/test_dev_d9f_manager_q.cpp
)

target_link_libraries(test_dev_d9f_manager_q
    PRIVATE devai_d9f_engine
)

# Register D9F unit tests with CTest
enable_testing()

add_test(
    NAME dev_d9f_manager_q
    COMMAND test_dev_d9f_manager_q
)


# ============================================================
# CETE / DevAI — D9F.2 — Reward Attribution and Statistical Entry Eligibility Correction
# ============================================================

# Append to your root CMakeLists.txt ONLY if D9F already has its own target.
# This adds the eligibility implementation to the existing D9F runner.
target_sources(run_dev_d9f_portfolio PRIVATE
    src/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.cpp)
add_executable(test_dev_d9f2_eligibility
    tests/strategy/dev_ai_strategy/test_dev_d9f2_eligibility.cpp
    src/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.cpp)
target_include_directories(test_dev_d9f2_eligibility PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_d9f2_eligibility PRIVATE cxx_std_17)
add_test(NAME dev_d9f2_eligibility COMMAND test_dev_d9f2_eligibility)


# ============================================================
# CETE / DevAI — D9F.3 — Reward Attribution, Brokerage-Aware Learning, and Trade-Level Validation
# ============================================================
# Add this block ONCE at the END of the existing CMakeLists.txt.
# D9F.2 target and its existing libraries must already be defined.
add_executable(run_dev_d9f3_portfolio
    tests/strategy/dev_ai_strategy/run_dev_d9f3_portfolio.cpp
    src/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.cpp)
target_include_directories(run_dev_d9f3_portfolio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/strategy/dev_ai_strategy)
target_link_libraries(run_dev_d9f3_portfolio PRIVATE devai_d9f_engine)
target_compile_features(run_dev_d9f3_portfolio PRIVATE cxx_std_20)
add_executable(test_dev_d9f3_reward
    tests/strategy/dev_ai_strategy/test_dev_d9f3_reward.cpp)
target_include_directories(test_dev_d9f3_reward PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_d9f3_reward PRIVATE cxx_std_17)
add_test(NAME dev_d9f3_reward COMMAND test_dev_d9f3_reward)


# ============================================================
# CETE / DevAI — D9F.3A — Forward Return Forecast Audit
# ============================================================
# Add this ONCE at the bottom of CMakeLists.txt (after D9F.3 entries).
add_executable(run_dev_d9f3a_portfolio
    tests/strategy/dev_ai_strategy/run_dev_d9f3a_portfolio.cpp
    src/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.cpp)
target_include_directories(run_dev_d9f3a_portfolio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/strategy/dev_ai_strategy)
target_link_libraries(run_dev_d9f3a_portfolio PRIVATE devai_d9f_engine)
target_compile_features(run_dev_d9f3a_portfolio PRIVATE cxx_std_20)
add_executable(test_dev_d9f3a_forward_audit
    tests/strategy/dev_ai_strategy/test_dev_d9f3a_forward_audit.cpp)
target_include_directories(test_dev_d9f3a_forward_audit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_d9f3a_forward_audit PRIVATE cxx_std_17)
add_test(NAME dev_d9f3a_forward_audit COMMAND test_dev_d9f3a_forward_audit)


# ============================================================
# CETE / DevAI — D9F.3B
# Forecast Calibration and DevManager Decision Attribution
# ============================================================

# 1. Forecast Calibration Audit
add_executable(
    run_dev_d9f3b_audit
    tests/strategy/dev_ai_strategy/run_dev_d9f3b_audit.cpp
)

target_compile_features(
    run_dev_d9f3b_audit
    PRIVATE cxx_std_17
)

# 2. DevManager Decision Attribution Audit
add_executable(
    run_dev_d9f3b_decision_audit
    tests/strategy/dev_ai_strategy/run_dev_d9f3b_decision_audit.cpp
    src/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.cpp
)

target_include_directories(
    run_dev_d9f3b_decision_audit
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/strategy/dev_ai_strategy
)

target_link_libraries(
    run_dev_d9f3b_decision_audit
    PRIVATE
        devai_d9f_engine
)

target_compile_features(
    run_dev_d9f3b_decision_audit
    PRIVATE cxx_std_20
)


# ============================================================
# CETE / DevAI — D9F.3C
# Forecast Horizon and RL State Verification
# ============================================================

# Add ONCE to root CMakeLists.txt. Standalone test: no modifications to existing targets.
add_executable(test_dev_d9f3c_contract
    tests/strategy/dev_ai_strategy/test_dev_d9f3c_contract.cpp
    src/strategy/dev_ai_strategy/DevManagerRL_D9F.cpp
    src/strategy/dev_ai_strategy/DevCausalForwardReturnD8.cpp)
target_include_directories(test_dev_d9f3c_contract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_d9f3c_contract PRIVATE cxx_std_20)
add_test(NAME dev_d9f3c_contract COMMAND test_dev_d9f3c_contract)

# ============================================================
# CETE / DevAI — D9F.4A
# Multi-Candidate RL State Engine
# ============================================================
 
# Add once to the root CMakeLists.txt. Standalone test; does not link/modify the D9F.3B runner.
add_executable(test_dev_multi_candidate_state_d9f4a
    tests/strategy/dev_ai_strategy/test_dev_multi_candidate_state_d9f4a.cpp
    src/strategy/dev_ai_strategy/DevMultiCandidateState_D9F4A.cpp)
target_include_directories(test_dev_multi_candidate_state_d9f4a PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_multi_candidate_state_d9f4a PRIVATE cxx_std_20)

# ============================================================
# CETE / DevAI — D9F.4B
# Cost-Aware Reward Engine
# ============================================================


add_executable(test_dev_cost_aware_reward_d9f4b
    tests/strategy/dev_ai_strategy/reward/test_dev_cost_aware_reward_d9f4b.cpp
    src/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.cpp
)

target_include_directories(
    test_dev_cost_aware_reward_d9f4b
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_cost_aware_reward_d9f4b
    PRIVATE
    cxx_std_20
)


# ============================================================
# CETE / DevAI — D9F.4B
# Production Cost-Aware Reward Integration
# ============================================================

# Standalone integration test
add_executable(test_dev_cost_aware_reward_integration_d9f4b
    tests/strategy/dev_ai_strategy/reward/test_dev_cost_aware_reward_integration_d9f4b.cpp
    src/strategy/dev_ai_strategy/reward/DevCostAwareRewardIntegration_D9F4B.cpp
    src/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.cpp
    src/strategy/dev_ai_strategy/DevPortfolioEnvironment.cpp
)

target_include_directories(
    test_dev_cost_aware_reward_integration_d9f4b
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_cost_aware_reward_integration_d9f4b
    PRIVATE
    cxx_std_20
)

# Production library integration
target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.cpp
    src/strategy/dev_ai_strategy/reward/DevCostAwareRewardIntegration_D9F4B.cpp
)

# ============================================================
# CETE / DevAI
# D9F.4C — Dual-Mode Reward, Production Integration
#           and Historical Replay Integration
# ============================================================

# IMPORTANT:
# This section must appear AFTER the devai_strategy
# library target has been created.
#
# Each D9F.4C implementation is compiled exactly once
# into the existing production strategy library.

# ============================================================
# 1. D9F.4C — Production Source Files
# ============================================================

target_sources(devai_strategy PRIVATE

    # Dual-mode RL reward and transition integration
    src/strategy/dev_ai_strategy/reward/DevDualModeTransition_D9F4C.cpp

    # Verified execution-ledger reward bridge
    src/strategy/dev_ai_strategy/reward/DevLedgerRewardBridge_D9F4C.cpp

    # Production manager transition adapter
    src/strategy/dev_ai_strategy/reward/DevProductionTransitionIntegration_D9F4C.cpp

    # Historical replay integration
    src/strategy/dev_ai_strategy/reward/DevHistoricalReplayIntegration_D9F4C.cpp
)

# ============================================================
# 2. D9F.4C — Dual-Mode Transition Test
# ============================================================

add_executable(test_dev_dual_mode_transition_d9f4c
    tests/strategy/dev_ai_strategy/reward/test_dev_dual_mode_transition_d9f4c.cpp
)

target_link_libraries(test_dev_dual_mode_transition_d9f4c
    PRIVATE devai_strategy
)

# ============================================================
# 3. D9F.4C — Verified Ledger Reward Bridge Test
# ============================================================

add_executable(test_dev_ledger_reward_bridge_d9f4c
    tests/strategy/dev_ai_strategy/reward/test_dev_ledger_reward_bridge_d9f4c.cpp
)

target_link_libraries(test_dev_ledger_reward_bridge_d9f4c
    PRIVATE devai_strategy
)

# ============================================================
# 4. D9F.4C — Production Transition Integration Test
# ============================================================

add_executable(test_dev_production_transition_integration_d9f4c
    tests/strategy/dev_ai_strategy/reward/test_dev_production_transition_integration_d9f4c.cpp
)

target_link_libraries(test_dev_production_transition_integration_d9f4c
    PRIVATE devai_strategy
)

# ============================================================
# 5. D9F.4C — Historical Replay Integration Test
# ============================================================

add_executable(test_dev_historical_replay_integration_d9f4c
    tests/strategy/dev_ai_strategy/reward/test_dev_historical_replay_integration_d9f4c.cpp
)

target_link_libraries(test_dev_historical_replay_integration_d9f4c
    PRIVATE devai_strategy
)

# ============================================================
# 6. D9F.4C — Include Paths and C++ Standard
# ============================================================

set(D9F4C_TEST_TARGETS
    test_dev_dual_mode_transition_d9f4c
    test_dev_ledger_reward_bridge_d9f4c
    test_dev_production_transition_integration_d9f4c
    test_dev_historical_replay_integration_d9f4c
)

foreach(target_name IN LISTS D9F4C_TEST_TARGETS)

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_features(${target_name} PRIVATE
        cxx_std_20
    )

endforeach()

# ============================================================
# END — CETE / DevAI D9F.4C
# ============================================================




# ============================================================
# CETE / DevAI — D9F.4C Real Ledger Replay
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/reward/DevRealLedgerReplay_D9F4C.cpp
)

add_executable(test_dev_real_ledger_replay_d9f4c
    tests/strategy/dev_ai_strategy/reward/test_dev_real_ledger_replay_d9f4c.cpp
)

target_link_libraries(test_dev_real_ledger_replay_d9f4c
    PRIVATE devai_strategy
)

target_compile_features(test_dev_real_ledger_replay_d9f4c
    PRIVATE cxx_std_20
)

# ============================================================
# CETE / DevAI
# D9F.4C — Historical Candle Replay and DevManager_001
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/reward/DevCandleManagerReplay_D9F4C.cpp
)

add_executable(test_dev_candle_manager_replay_d9f4c
    tests/strategy/dev_ai_strategy/reward/test_dev_candle_manager_replay_d9f4c.cpp
)

target_link_libraries(test_dev_candle_manager_replay_d9f4c
    PRIVATE devai_strategy
)

target_compile_features(test_dev_candle_manager_replay_d9f4c
    PRIVATE cxx_std_20
)

add_executable(run_dev_candle_manager_replay_d9f4c
    tools/run_dev_candle_manager_replay_d9f4c.cpp
)

target_link_libraries(run_dev_candle_manager_replay_d9f4c
    PRIVATE devai_strategy
)

target_compile_features(run_dev_candle_manager_replay_d9f4c
    PRIVATE cxx_std_20
)

# ============================================================
# CETE / DevAI — D9F.4C Session Rules and DevAISupervisor
# Standalone policy module; replay integration follows separately.
# ============================================================

target_sources(devai_strategy PRIVATE
    src/strategy/dev_ai_strategy/risk/DevSessionSupervisor.cpp
)

add_executable(test_dev_session_supervisor
    tests/strategy/dev_ai_strategy/risk/test_dev_session_supervisor.cpp
)

target_link_libraries(test_dev_session_supervisor PRIVATE
    devai_strategy
)

target_compile_features(test_dev_session_supervisor PRIVATE
    cxx_std_20
)

add_test(
    NAME dev_session_supervisor
    COMMAND test_dev_session_supervisor
)

# ============================================================
# CETE / DevAI — D9F.4C Decision Audit C++ Unit Test
# ============================================================
add_executable(test_dev_candle_decision_audit_d9f4c
    tests/strategy/dev_ai_strategy/reward/test_dev_candle_decision_audit_d9f4c.cpp
)
target_link_libraries(test_dev_candle_decision_audit_d9f4c PRIVATE devai_strategy)
target_compile_features(test_dev_candle_decision_audit_d9f4c PRIVATE cxx_std_20)
add_test(NAME dev_candle_decision_audit_d9f4c COMMAND test_dev_candle_decision_audit_d9f4c)

# ============================================================
# D9F.4C-E — Mathematical Ranking and RL Decision Validation
# ============================================================

add_library(devai_d9f4ce_diagnostics STATIC
    src/strategy/dev_ai_strategy/diagnostics/DevRankingDecisionValidator_D9F4CE.cpp)
target_include_directories(devai_d9f4ce_diagnostics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(devai_d9f4ce_diagnostics PUBLIC cxx_std_20)
add_executable(run_dev_ranking_decision_validation_d9f4ce
    src/strategy/dev_ai_strategy/diagnostics/run_dev_ranking_decision_validation_d9f4ce.cpp)
target_link_libraries(run_dev_ranking_decision_validation_d9f4ce PRIVATE devai_d9f4ce_diagnostics)
add_executable(test_dev_ranking_decision_validation_d9f4ce
    tests/strategy/dev_ai_strategy/diagnostics/test_dev_ranking_decision_validation_d9f4ce.cpp)
target_link_libraries(test_dev_ranking_decision_validation_d9f4ce PRIVATE devai_d9f4ce_diagnostics)
add_test(NAME dev_ranking_decision_validation_d9f4ce COMMAND test_dev_ranking_decision_validation_d9f4ce)


# ============================================================
# D9F.4C-F — Mathematical Ranking Source and Feature Integrity Audit
# ============================================================
add_library(dev_ranking_source_audit_d9f4cf STATIC
    src/strategy/dev_ai_strategy/diagnostics/DevRankingSourceAudit_D9F4CF.cpp)
target_compile_features(dev_ranking_source_audit_d9f4cf PUBLIC cxx_std_17)
target_include_directories(dev_ranking_source_audit_d9f4cf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_executable(run_dev_ranking_source_audit_d9f4cf
    src/strategy/dev_ai_strategy/diagnostics/run_dev_ranking_source_audit_d9f4cf.cpp)
target_link_libraries(run_dev_ranking_source_audit_d9f4cf PRIVATE dev_ranking_source_audit_d9f4cf)
add_executable(test_dev_ranking_source_audit_d9f4cf
    tests/strategy/dev_ai_strategy/diagnostics/test_dev_ranking_source_audit_d9f4cf.cpp)
target_link_libraries(test_dev_ranking_source_audit_d9f4cf PRIVATE dev_ranking_source_audit_d9f4cf)
# ============================================================
# D9F.4C-F1 — Original Generator Performance Comparison
# ============================================================

get_target_property(
    D9F4CF1_ORIGINAL_SOURCES
    run_dev_real_historical_cache_generator
    SOURCES
)

list(
    REMOVE_ITEM D9F4CF1_ORIGINAL_SOURCES
    apps/run_dev_real_historical_cache_generator.cpp
)

add_executable(
    run_dev_real_historical_cache_generator_pre_f1

    apps/run_dev_real_historical_cache_generator_pre_f1.cpp

    ${D9F4CF1_ORIGINAL_SOURCES}
)

target_include_directories(
    run_dev_real_historical_cache_generator_pre_f1
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    run_dev_real_historical_cache_generator_pre_f1
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    run_dev_real_historical_cache_generator_pre_f1
    PRIVATE
        devai_strategy
)

# ============================================================
# D9F.4C-F2: historical Top-5 shared-model inference audit
# ============================================================

add_executable(run_dev_f2_historical_inference
    apps/run_dev_f2_historical_inference.cpp
)
target_include_directories(run_dev_f2_historical_inference PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_f2_historical_inference PRIVATE cxx_std_20)
target_link_libraries(run_dev_f2_historical_inference PRIVATE devai_strategy)

# ============================================================
# D9F.4C- F3A — Unified Shared-Model Historical Integration Audit
# ============================================================

add_executable(run_dev_f3a_unified_historical_audit
    apps/run_dev_f3a_unified_historical_audit.cpp
)

target_link_libraries(run_dev_f3a_unified_historical_audit
    PRIVATE
    devai_strategy
)

target_compile_features(run_dev_f3a_unified_historical_audit
    PRIVATE cxx_std_20
)

# ============================================================
# D9F.4C-F3B Manager Policy and Risk Audit
# ============================================================

add_executable(run_dev_f3b_manager_policy_risk_audit
    apps/run_dev_f3b_manager_policy_risk_audit.cpp
)
target_compile_features(run_dev_f3b_manager_policy_risk_audit PRIVATE cxx_std_20)
target_include_directories(run_dev_f3b_manager_policy_risk_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(run_dev_f3b_manager_policy_risk_audit PRIVATE devai_strategy)

# ============================================================
# D9F.4C-F3C — full-session historical replay audit
# ============================================================

# Requires the existing devai_strategy target to already contain
# DevCandleManagerReplay_D9F4C.cpp and its dependencies.
add_executable(run_dev_f3c_full_session_replay_audit
    apps/run_dev_f3c_full_session_replay_audit.cpp
)
target_link_libraries(run_dev_f3c_full_session_replay_audit PRIVATE devai_strategy)
target_compile_features(run_dev_f3c_full_session_replay_audit PRIVATE cxx_std_20)


# ============================================================
# D9F.4C-F3E-D1 — Multi-Holding Counterfactual Replay
# Experiments: baseline (0), 1, 3, 5, and 10-minute minimum hold
# ============================================================
# The replay engine is already compiled into devai_strategy.
# Keep F3C unchanged; this executable is for historical research only.

add_executable(run_dev_f3d1_multi_holding_counterfactual
    apps/run_dev_f3d1_multi_holding_counterfactual.cpp
)

target_link_libraries(run_dev_f3d1_multi_holding_counterfactual
    PRIVATE
        devai_strategy
)

target_compile_features(run_dev_f3d1_multi_holding_counterfactual
    PRIVATE
        cxx_std_20
)


# ============================================================
# D9F.4C-F3E-F1 — Multi-Horizon Forward Return Labels
# ============================================================

add_executable(test_dev_forward_return_labels_f3ef1
    tests/strategy/dev_ai_strategy/forecast/test_dev_forward_return_labels_f3ef1.cpp
)

target_include_directories(test_dev_forward_return_labels_f3ef1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(test_dev_forward_return_labels_f3ef1
    PRIVATE cxx_std_20
)


# ============================================================
# CETE / DevAI — F3E-F1B
# Real Market Data Forward Return Integration
# ============================================================

add_executable(run_dev_f3ef1b_real_market_forward_returns
    apps/run_dev_f3ef1b_real_market_forward_returns.cpp

    # CETE Phase 1 — Market Data Loader
    src/market/MarketDataLoader.cpp
)

target_include_directories(
    run_dev_f3ef1b_real_market_forward_returns
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    run_dev_f3ef1b_real_market_forward_returns
    PRIVATE
        devai_strategy
)

target_compile_features(
    run_dev_f3ef1b_real_market_forward_returns
    PRIVATE
        cxx_std_20
)

# ============================================================
# F3E-F1B — Real Market Integration Unit Test
# ============================================================

add_executable(test_dev_real_market_forward_returns_f3ef1b
    tests/strategy/dev_ai_strategy/forecast/test_dev_real_market_forward_returns_f3ef1b.cpp
)

target_include_directories(
    test_dev_real_market_forward_returns_f3ef1b
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_real_market_forward_returns_f3ef1b
    PRIVATE
        cxx_std_20
)

# ============================================================
# CETE / DevAI F3E-F1C — Historical Forecast Training Dataset
# ============================================================
add_executable(run_dev_f3ef1c_training_dataset
    apps/run_dev_f3ef1c_training_dataset.cpp
    src/market/MarketDataLoader.cpp
)
target_include_directories(run_dev_f3ef1c_training_dataset PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(run_dev_f3ef1c_training_dataset PRIVATE cxx_std_20)

add_executable(test_dev_training_dataset_f3ef1c
    tests/strategy/dev_ai_strategy/forecast/test_dev_training_dataset_f3ef1c.cpp
)
target_include_directories(test_dev_training_dataset_f3ef1c PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(test_dev_training_dataset_f3ef1c PRIVATE cxx_std_20)

add_executable(test_dev_real_training_dataset_f3ef1c
    tests/strategy/dev_ai_strategy/forecast/test_dev_real_training_dataset_f3ef1c.cpp
    src/market/MarketDataLoader.cpp
)
target_include_directories(test_dev_real_training_dataset_f3ef1c PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(test_dev_real_training_dataset_f3ef1c PRIVATE cxx_std_20)

# ============================================================
# F3E-F2 — standalone C++20 multi-horizon ridge forecasting
# ============================================================

add_executable(test_dev_forward_forecast_f3ef2
    tests/strategy/dev_ai_strategy/forecast/test_dev_forward_forecast_f3ef2.cpp)
target_include_directories(test_dev_forward_forecast_f3ef2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_forward_forecast_f3ef2 PRIVATE cxx_std_20)

add_executable(run_dev_f3ef2_train
    apps/run_dev_f3ef2_train.cpp)
target_include_directories(run_dev_f3ef2_train PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_f3ef2_train PRIVATE cxx_std_20)

# ============================================================
# CETE / DevAI F3E-F2B - forecast quality and hypothetical opportunity analysis
# ============================================================

add_executable(run_dev_f3ef2b_evaluate apps/run_dev_f3ef2b_evaluate.cpp)
target_include_directories(run_dev_f3ef2b_evaluate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_f3ef2b_evaluate PRIVATE cxx_std_20)
add_executable(test_dev_forecast_evaluation_f3ef2b tests/strategy/dev_ai_strategy/forecast/test_dev_forecast_evaluation_f3ef2b.cpp)
target_include_directories(test_dev_forecast_evaluation_f3ef2b PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_forecast_evaluation_f3ef2b PRIVATE cxx_std_20)

# ============================================================
# F3E-F2C - independent C++20 forecast improvement baseline
# ============================================================

add_executable(run_dev_f3ef2c_train apps/run_dev_f3ef2c_train.cpp)
target_include_directories(run_dev_f3ef2c_train PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_f3ef2c_train PRIVATE cxx_std_20)
add_executable(test_dev_forecast_improved_f3ef2c tests/strategy/dev_ai_strategy/forecast/test_dev_forecast_improved_f3ef2c.cpp)
target_include_directories(test_dev_forecast_improved_f3ef2c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_forecast_improved_f3ef2c PRIVATE cxx_std_20)

# ============================================================
# F3E-F2C.1 — Forecast diagnostics; requires F3E-F2C header
# ============================================================

add_executable(run_dev_f3ef2c1_diagnostics
    apps/run_dev_f3ef2c1_diagnostics.cpp)
target_include_directories(run_dev_f3ef2c1_diagnostics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_f3ef2c1_diagnostics PRIVATE cxx_std_20)

add_executable(test_dev_forecast_diagnostics_f3ef2c1
    tests/strategy/dev_ai_strategy/forecast/test_dev_forecast_diagnostics_f3ef2c1.cpp)
target_include_directories(test_dev_forecast_diagnostics_f3ef2c1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_forecast_diagnostics_f3ef2c1 PRIVATE cxx_std_20)

# ============================================================
# CETE / DevAI F3E-F2C.2: completed 5m/15m statistical feature dataset
# ============================================================

add_executable(test_dev_multitimeframe_features_f3ef2c2
    tests/strategy/dev_ai_strategy/forecast/test_dev_multitimeframe_features_f3ef2c2.cpp)
target_include_directories(test_dev_multitimeframe_features_f3ef2c2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_multitimeframe_features_f3ef2c2 PRIVATE cxx_std_20)

add_executable(run_dev_f3ef2c2_multitimeframe_dataset
    apps/run_dev_f3ef2c2_multitimeframe_dataset.cpp
    src/market/MarketDataLoader.cpp)
target_include_directories(run_dev_f3ef2c2_multitimeframe_dataset PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_f3ef2c2_multitimeframe_dataset PRIVATE cxx_std_20)

# ============================================================
# F3E-F2C.3 -- independent multi-timeframe forecasting retrain
# ============================================================

add_executable(run_dev_f3ef2c3_train apps/run_dev_f3ef2c3_train.cpp)
target_include_directories(run_dev_f3ef2c3_train PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_dev_f3ef2c3_train PRIVATE cxx_std_20)
add_executable(test_dev_forecast_multitimeframe_f3ef2c3 tests/strategy/dev_ai_strategy/forecast/test_dev_forecast_multitimeframe_f3ef2c3.cpp)
target_include_directories(test_dev_forecast_multitimeframe_f3ef2c3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_forecast_multitimeframe_f3ef2c3 PRIVATE cxx_std_20)

# ============================================================
# F3E-F2C.4 standalone proxy portfolio evaluation
# ============================================================

add_executable(run_dev_f3ef2c4_portfolio apps/run_dev_f3ef2c4_portfolio.cpp)
target_compile_features(run_dev_f3ef2c4_portfolio PRIVATE cxx_std_20)
add_executable(test_dev_portfolio_f3ef2c4 tests/strategy/dev_ai_strategy/forecast/test_dev_portfolio_f3ef2c4.cpp)
target_compile_features(test_dev_portfolio_f3ef2c4 PRIVATE cxx_std_20)

# ============================================================
# F3E-F2C.4A/B independent verification target
# ============================================================

# Append ONCE to the project root CMakeLists.txt.
add_executable(test_f3ef2c4ab
  tests/strategy/dev_ai_strategy/test_f3ef2c4ab.cpp
  src/strategy/dev_ai_strategy/DevExecutionGatewayD9C.cpp
  src/strategy/brokerage/BrokerageChargesEngine.cpp)
target_include_directories(test_f3ef2c4ab PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(test_f3ef2c4ab PRIVATE cxx_std_20)



# ============================================================
# F3E-F2C.5 — DevManager_001 LONG/SHORT Integration
# ============================================================
# Append ONCE to the root CMakeLists.txt; assumes F3E-F2C.4A/B installed.
add_executable(test_f3ef2c5
    tests/strategy/dev_ai_strategy/test_f3ef2c5.cpp
    src/strategy/dev_ai_strategy/DevManagerDirectionBridge_F3EF2C5.cpp)
target_include_directories(test_f3ef2c5 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_f3ef2c5 PRIVATE cxx_std_20)
# The bridge only uses gateway data types; no gateway implementation is linked.


# ============================================================
# F3E-F2C.5A standalone direction-policy regression test.
# ============================================================

add_executable(test_f3ef2c5a_direction_policy
  tests/devai/strategy/dev_ai_strategy/test_f3ef2c5a_direction_policy.cpp)
target_include_directories(test_f3ef2c5a_direction_policy PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(test_f3ef2c5a_direction_policy PRIVATE cxx_std_20)
# Production adapter source should be added to the existing DevAI target
# that already links DevManager.cpp and DevManagerDirectionBridge_F3EF2C5.cpp.
# Do NOT compile it as a separate manager or duplicate those sources.

# ============================================================
# F3E-F2C.5B — Shared RL LONG/SHORT Training
# ============================================================

# Append once to your root CMakeLists.txt.
add_executable(test_f3ef2c5b
    tests/strategy/dev_ai_strategy/test_f3ef2c5b.cpp)
target_include_directories(test_f3ef2c5b PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(test_f3ef2c5b PRIVATE cxx_std_20)


# ============================================================
# F3E-F2C.5C isolated offline replay regression
# ============================================================

add_executable(test_f3ef2c5c
  tests/strategy/dev_ai_strategy/test_f3ef2c5c.cpp)
target_include_directories(test_f3ef2c5c PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(test_f3ef2c5c PRIVATE cxx_std_20)

# ============================================================
# F3E-F2C.5D: existing devai_strategy already owns the shared network
# ============================================================

# historical cache, ledger, OOS runner, manager and agent contract sources.
add_executable(test_f3ef2c5d
    tests/strategy/dev_ai_strategy/test_f3ef2c5d.cpp)
target_include_directories(test_f3ef2c5d PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(test_f3ef2c5d PRIVATE cxx_std_20)

add_executable(run_dev_f3ef2c5d_frozen_oos
    apps/run_dev_f3ef2c5d_frozen_oos.cpp)
target_link_libraries(run_dev_f3ef2c5d_frozen_oos PRIVATE devai_strategy)
target_compile_features(run_dev_f3ef2c5d_frozen_oos PRIVATE cxx_std_20)

# ============================================================
# F3E-F2C.5D — LONG + SHORT C++ update
# ============================================================
add_executable(split_devai_historical_cache
    apps/split_devai_historical_cache.cpp)
target_link_libraries(split_devai_historical_cache PRIVATE devai_strategy)
target_compile_features(split_devai_historical_cache PRIVATE cxx_std_20)

#====================================================================
# F3E-F2C.5E — Six-Agent Architecture and DevManager Decision Engine
#====================================================================
# Legacy six-slot test: retained for reference, excluded from normal builds.
add_executable(test_f3ef2c5e EXCLUDE_FROM_ALL tests/strategy/dev_ai_strategy/test_f3ef2c5e.cpp)
target_include_directories(test_f3ef2c5e PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(test_f3ef2c5e PRIVATE cxx_std_20)





# ============================================================
# Five-Agent Regression: registered with CTest
# ============================================================
include(CTest)
if(BUILD_TESTING)
    add_test(NAME dev_five_agent_architecture COMMAND test_dev_agent_architecture)
    add_test(NAME dev_five_agent_multi_stock_oos COMMAND test_dev_multi_stock_oos_validation)
endif()

# Phase 3.19 real historical ledger regression (fixture-based)
add_test(NAME dev_real_historical_multi_stock_oos_ledger COMMAND test_dev_real_historical_multi_stock_oos)

# ============================================================
# CETE Phase 3.19.3C — Historical Replay Eligibility Gate Integration
# ============================================================
# Add once to the END of your existing root CMakeLists.txt
add_library(cete_replay_eligibility STATIC
    src/replay/ReplayEligibilityGate.cpp
)
target_include_directories(cete_replay_eligibility PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(cete_replay_eligibility PUBLIC cxx_std_17)
add_executable(test_replay_eligibility_gate
    tests/replay/test_replay_eligibility_gate.cpp
)
target_link_libraries(test_replay_eligibility_gate PRIVATE cete_replay_eligibility)

# ============================================================
# CETE H4.7 Stage 4
# Production Statistical Ranking + Historical Replay Probe
# ============================================================

# ------------------------------------------------------------
# 1. Reuse existing production historical replay sources
# ------------------------------------------------------------

get_target_property(
    CETE_H47_STAGE4_SOURCES
    run_dev_single_pass_historical_cache
    SOURCES
)

if(NOT CETE_H47_STAGE4_SOURCES)
    message(FATAL_ERROR
        "H4.7 Stage 4: Historical replay sources not found"
    )
endif()

# Remove the existing application's main() source
list(
    REMOVE_ITEM
    CETE_H47_STAGE4_SOURCES
    apps/run_dev_single_pass_historical_cache.cpp
)

# ------------------------------------------------------------
# 2. Stage 4 — Production Statistical Ranking
# ------------------------------------------------------------

add_executable(
    run_cete_h4_7_stage4_ranking

    tests/benchmark/run_cete_h4_7_stage4_ranking.cpp

    ${CETE_H47_STAGE4_SOURCES}
)

target_include_directories(
    run_cete_h4_7_stage4_ranking
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    run_cete_h4_7_stage4_ranking
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    run_cete_h4_7_stage4_ranking
    PRIVATE
        devai_strategy
)

set_target_properties(
    run_cete_h4_7_stage4_ranking
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/h4_7"
)

# ------------------------------------------------------------
# 3. Stage 4 — Historical Replay Diagnostic Probe
# ------------------------------------------------------------

add_executable(
    test_cete_h4_7_replay_probe

    tests/benchmark/test_cete_h4_7_replay_probe.cpp

    ${CETE_H47_STAGE4_SOURCES}
)

target_include_directories(
    test_cete_h4_7_replay_probe
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_cete_h4_7_replay_probe
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    test_cete_h4_7_replay_probe
    PRIVATE
        devai_strategy
)

set_target_properties(
    test_cete_h4_7_replay_probe
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/h4_7"
)

# ============================================================
# H4.7 Stage 5B — Persistent Cache Hardening Unit Tests
# ============================================================
add_executable(test_cete_h47_stage5b_cache_hardening
    tests/cache/test_cete_h47_stage5b_cache_hardening.cpp
    src/cache/HistoricalMathematicalFeatureCache.cpp
)
target_include_directories(test_cete_h47_stage5b_cache_hardening PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(test_cete_h47_stage5b_cache_hardening PRIVATE cxx_std_20)

# ============================================================
# H4.7 Stage 5E.1 — 
# ============================================================

add_executable(test_dev_ranking_refresh_h47_5e1
    tests/strategy/dev_ai_strategy/test_dev_ranking_refresh_h47_5e1.cpp)
target_include_directories(test_dev_ranking_refresh_h47_5e1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_ranking_refresh_h47_5e1 PRIVATE cxx_std_20)
target_link_libraries(test_dev_ranking_refresh_h47_5e1 PRIVATE devai_strategy)
add_test(NAME dev_ranking_refresh_h47_5e1 COMMAND test_dev_ranking_refresh_h47_5e1)

add_executable(run_cete_h4_7_stage5e1_ranking
    tests/benchmark/run_cete_h4_7_stage5e1_ranking.cpp
    ${CETE_H47_STAGE4_SOURCES})
target_include_directories(run_cete_h4_7_stage5e1_ranking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(run_cete_h4_7_stage5e1_ranking PRIVATE cxx_std_20)
target_link_libraries(run_cete_h4_7_stage5e1_ranking PRIVATE devai_strategy)
set_target_properties(run_cete_h4_7_stage5e1_ranking PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/h4_7")

# ============================================================
# CETE/DevAI — Stage 5E.2: Shared RL Inference Integration
# ============================================================

add_executable(test_dev_shared_rl_bridge_h47_5e2
    tests/strategy/dev_ai_strategy/test_dev_shared_rl_bridge_h47_5e2.cpp)
target_include_directories(test_dev_shared_rl_bridge_h47_5e2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_shared_rl_bridge_h47_5e2 PRIVATE cxx_std_20)
target_link_libraries(test_dev_shared_rl_bridge_h47_5e2 PRIVATE devai_strategy)
add_test(NAME dev_shared_rl_bridge_h47_5e2 COMMAND test_dev_shared_rl_bridge_h47_5e2)

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2B — Selected Advanced RL Model Inference
#
# C++20
# One selected shared RL model
# Five DevAgents
# Maskable PPO inference adapter
# Historical testing only — no live orders
# ============================================================

add_executable(
    test_dev_selected_rl_inference_h47_5e2b

    tests/strategy/dev_ai_strategy/test_dev_selected_rl_inference_h47_5e2b.cpp
    src/benchmark/DevRLModelInterface.cpp
    src/benchmark/DevMaskablePPO.cpp
)

target_include_directories(
    test_dev_selected_rl_inference_h47_5e2b
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_selected_rl_inference_h47_5e2b
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    test_dev_selected_rl_inference_h47_5e2b
    PRIVATE
        devai_strategy
)

add_test(
    NAME dev_selected_rl_inference_h47_5e2b
    COMMAND test_dev_selected_rl_inference_h47_5e2b
)


# ============================================================
# CETE/DevAI — Stage 5E.2C: Historical Shared RL Inference Integration
# ============================================================

add_executable(test_dev_historical_shared_rl_replay_h47_5e2c
    tests/strategy/dev_ai_strategy/test_dev_historical_shared_rl_replay_h47_5e2c.cpp
    src/benchmark/DevRLModelInterface.cpp)
target_include_directories(test_dev_historical_shared_rl_replay_h47_5e2c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_dev_historical_shared_rl_replay_h47_5e2c PRIVATE cxx_std_20)
target_link_libraries(test_dev_historical_shared_rl_replay_h47_5e2c PRIVATE devai_strategy)
add_test(NAME dev_historical_shared_rl_replay_h47_5e2c
    COMMAND test_dev_historical_shared_rl_replay_h47_5e2c)

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2D — Historical Cache RL Request Integration
# ============================================================

add_executable(
    test_dev_cached_historical_rl_request_h47_5e2d

    tests/strategy/dev_ai_strategy/test_dev_cached_historical_rl_request_h47_5e2d.cpp
)

target_include_directories(
    test_dev_cached_historical_rl_request_h47_5e2d
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_cached_historical_rl_request_h47_5e2d
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    test_dev_cached_historical_rl_request_h47_5e2d
    PRIVATE
        devai_strategy
)

add_test(
    NAME dev_cached_historical_rl_request_h47_5e2d
    COMMAND test_dev_cached_historical_rl_request_h47_5e2d
)

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2E — Historical RL Inference Audit
# ============================================================

add_executable(
    test_dev_historical_inference_audit_h47_5e2e

    tests/strategy/dev_ai_strategy/test_dev_historical_inference_audit_h47_5e2e.cpp
)

target_include_directories(
    test_dev_historical_inference_audit_h47_5e2e
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_historical_inference_audit_h47_5e2e
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    test_dev_historical_inference_audit_h47_5e2e
    PRIVATE
        devai_strategy
)

add_test(
    NAME dev_historical_inference_audit_h47_5e2e
    COMMAND test_dev_historical_inference_audit_h47_5e2e
)


# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2F — Historical Shared RL Integration
# ============================================================

add_executable(
    test_dev_historical_shared_rl_executable_h47_5e2f

    tests/strategy/dev_ai_strategy/test_dev_historical_shared_rl_executable_h47_5e2f.cpp

    src/benchmark/DevRLModelInterface.cpp
)

target_include_directories(
    test_dev_historical_shared_rl_executable_h47_5e2f
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    test_dev_historical_shared_rl_executable_h47_5e2f
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    test_dev_historical_shared_rl_executable_h47_5e2f
    PRIVATE
        devai_strategy
)

add_test(
    NAME dev_historical_shared_rl_executable_h47_5e2f
    COMMAND test_dev_historical_shared_rl_executable_h47_5e2f
)
# H4.7 Stage 5E.2G: price causality and execution integration regression
add_executable(test_dev_sequential_rl_execution_h47_5e2g
    tests/strategy/dev_ai_strategy/test_dev_sequential_rl_execution_h47_5e2g.cpp)
target_link_libraries(test_dev_sequential_rl_execution_h47_5e2g PRIVATE devai_strategy)
target_compile_features(test_dev_sequential_rl_execution_h47_5e2g PRIVATE cxx_std_20)
add_test(NAME dev_sequential_rl_execution_h47_5e2g
    COMMAND test_dev_sequential_rl_execution_h47_5e2g)


# ============================================================
# H4.7 Stage 5E.2K.6B
# Historical Mathematical / RL Pipeline Audit
# ============================================================

if(NOT TARGET run_cete_h47_stage6b_pipeline_audit)

    add_executable(run_cete_h47_stage6b_pipeline_audit
        apps/run_cete_h47_stage6b_pipeline_audit.cpp
    )

    target_link_libraries(run_cete_h47_stage6b_pipeline_audit
        PRIVATE
            devai_strategy
    )

    target_compile_features(run_cete_h47_stage6b_pipeline_audit
        PRIVATE
            cxx_std_20
    )

endif()

# ============================================================
# H4.7 Stage 5E.2K.6B — Mathematical-Only Baseline
# ============================================================

if(NOT TARGET run_cete_h47_stage6b_math_only_baseline)
    add_executable(run_cete_h47_stage6b_math_only_baseline
        apps/run_cete_h47_stage6b_math_only_baseline.cpp
    )

    target_link_libraries(run_cete_h47_stage6b_math_only_baseline
        PRIVATE devai_strategy
    )

    target_compile_features(run_cete_h47_stage6b_math_only_baseline
        PRIVATE cxx_std_20
    )
endif()

# ============================================================
# H4.7 Stage 5E.2K.6B.1 — Mathematical Baseline Diagnostic
# ============================================================

if(NOT TARGET run_cete_h47_stage6b1_math_diagnostic)
    add_executable(run_cete_h47_stage6b1_math_diagnostic
        apps/run_cete_h47_stage6b1_math_diagnostic.cpp
    )

    target_compile_features(
        run_cete_h47_stage6b1_math_diagnostic
        PRIVATE cxx_std_20
    )
endif()

# ============================================================
# H4.7 Stage 5E.2K.6B.2
# Mathematical Signal and Forward-Return Attribution
# ============================================================

if(NOT TARGET run_cete_h47_stage6b2_attribution)

    add_executable(run_cete_h47_stage6b2_attribution
        apps/run_cete_h47_stage6b2_attribution.cpp
    )

    target_compile_features(
        run_cete_h47_stage6b2_attribution
        PRIVATE cxx_std_20
    )

endif()

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2K.6C.1
# Causal Forward-Return Forecast Foundation
# ============================================================

if(NOT TARGET run_cete_h47_stage6c1_forecast)

    add_executable(
        run_cete_h47_stage6c1_forecast

        apps/run_cete_h47_stage6c1_forecast.cpp
    )

    target_compile_features(
        run_cete_h47_stage6c1_forecast
        PRIVATE cxx_std_20
    )

    target_include_directories(
        run_cete_h47_stage6c1_forecast
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(
        run_cete_h47_stage6c1_forecast
        PRIVATE
            devai_strategy
    )

    set_target_properties(
        run_cete_h47_stage6c1_forecast
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
    )

endif()

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2K.6C.1A
# Forecast Predictive-Value Audit
# ============================================================

if(NOT TARGET run_cete_h47_stage6c1a_audit)

    add_executable(
        run_cete_h47_stage6c1a_audit
        apps/run_cete_h47_stage6c1a_audit.cpp
    )

    target_compile_features(
        run_cete_h47_stage6c1a_audit
        PRIVATE cxx_std_20
    )

endif()

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2K.6C.1B
# Enhanced Mathematical Forecast Model
# ============================================================

if(NOT TARGET run_cete_h47_stage6c1b_enhanced)

    add_executable(
        run_cete_h47_stage6c1b_enhanced
        apps/run_cete_h47_stage6c1b_enhanced.cpp
    )

    target_link_libraries(
        run_cete_h47_stage6c1b_enhanced
        PRIVATE
            devai_strategy
    )

    target_include_directories(
        run_cete_h47_stage6c1b_enhanced
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_features(
        run_cete_h47_stage6c1b_enhanced
        PRIVATE cxx_std_20
    )

endif()

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2K.6C.1C
# Raw Mathematical Feature Foundation
# and Forecast Benchmarking
# ============================================================

if(NOT TARGET run_cete_h47_stage6c1c_raw)

    add_executable(
        run_cete_h47_stage6c1c_raw

        apps/run_cete_h47_stage6c1c_raw.cpp

        src/market/MarketDataLoader.cpp
    )

    target_include_directories(
        run_cete_h47_stage6c1c_raw
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(
        run_cete_h47_stage6c1c_raw
        PRIVATE
            devai_strategy
    )

    target_compile_features(
        run_cete_h47_stage6c1c_raw
        PRIVATE cxx_std_20
    )

    set_target_properties(
        run_cete_h47_stage6c1c_raw
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
    )

endif()

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2K.6C.1D
# Causal Multi-Timeframe Synchronization
# ============================================================

if(NOT TARGET run_cete_h47_stage6c1d_sync)

    add_executable(
        run_cete_h47_stage6c1d_sync

        apps/run_cete_h47_stage6c1d_sync.cpp

        src/market/MarketDataLoader.cpp
    )

    target_include_directories(
        run_cete_h47_stage6c1d_sync
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(
        run_cete_h47_stage6c1d_sync
        PRIVATE
            devai_strategy
    )

    target_compile_features(
        run_cete_h47_stage6c1d_sync
        PRIVATE cxx_std_20
    )

endif()

# ============================================================
# CETE / DevAI
# H4.7 Stage 5E.2K.6C.1E
# Cost-Aware Forward-Return Ranking Audit
# ============================================================

if(NOT TARGET run_cete_h47_stage6c1e_rank)

    add_executable(
        run_cete_h47_stage6c1e_rank
        apps/run_cete_h47_stage6c1e_rank.cpp
    )

    target_compile_features(
        run_cete_h47_stage6c1e_rank
        PRIVATE cxx_std_20
    )

endif()

# ============================================================

# CETE H4.7 Stage 5E.2K.6C.1F
# ============================================================

if(NOT TARGET run_cete_h47_stage6c1f_walkforward)

    add_executable(
        run_cete_h47_stage6c1f_walkforward
        apps/run_cete_h47_stage6c1f_walkforward.cpp
        src/market/MarketDataLoader.cpp
    )

    target_include_directories(
        run_cete_h47_stage6c1f_walkforward
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(
        run_cete_h47_stage6c1f_walkforward
        PRIVATE devai_strategy
    )

    target_compile_features(
        run_cete_h47_stage6c1f_walkforward
        PRIVATE cxx_std_20
    )

endif()

# ============================================================
# CETE / DevAI — Stage 6C.1G.2B.1
# Portfolio Execution Safety and Risk Gate Regression Tests
# ============================================================

# Ledger integration test: uses the existing DevAI strategy library.
add_executable(test_dev_historical_ledger_safety_h47_6c1g2b1
    tests/strategy/dev_ai_strategy/test_dev_historical_ledger_safety_h47_6c1g2b1.cpp
)
target_link_libraries(test_dev_historical_ledger_safety_h47_6c1g2b1
    PRIVATE devai_strategy
)
target_compile_features(test_dev_historical_ledger_safety_h47_6c1g2b1
    PRIVATE cxx_std_20
)
add_test(NAME dev_historical_ledger_safety_h47_6c1g2b1
    COMMAND test_dev_historical_ledger_safety_h47_6c1g2b1
)

# Risk gate is header-only; include directory comes from the project root.
add_executable(test_dev_historical_execution_risk_gate_h47_6c1g2b1
    tests/strategy/dev_ai_strategy/test_dev_historical_execution_risk_gate_h47_6c1g2b1.cpp
)
target_include_directories(test_dev_historical_execution_risk_gate_h47_6c1g2b1
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(test_dev_historical_execution_risk_gate_h47_6c1g2b1
    PRIVATE cxx_std_20
)
add_test(NAME dev_historical_execution_risk_gate_h47_6c1g2b1
    COMMAND test_dev_historical_execution_risk_gate_h47_6c1g2b1
)
