import gc
import io
import json
import os
import tempfile
import threading
import time
from contextlib import redirect_stdout
from datetime import time as clock_time
from pathlib import Path
from urllib.parse import unquote

import numpy as np
import pandas as pd


class FiveMinuteForwardTestPhaseA:
    """Synthetic-data Phase A for the complete FMRSS feature pipeline.

    One real-time cycle appends one synthetic five-minute candle for NIFTY and
    every share, then runs alignment through cross-sectional ranking. Source
    market files are read only and are never modified.
    """

    COLUMNS = [
        "Symbol", "Date", "Time", "Open", "High", "Low", "Close", "Volume"
    ]

    BENCHMARK_NAMES = {"NIFTY", "NIFTY50"}

    def __init__(
        self,
        symbols_file="/home/devinderjeet/fmrss/config/shares.txt",
        source_data_folder="/home/hadoop/shareMarket_Data",
        nifty_filename="NIFTY%2050_5mins.txt",
        tests_folder="/home/devinderjeet/fmrss/tests",
        history_candles=1500,
        expected_share_count=80,
        interval_seconds=1.0,
        market_sigma=0.0012,
        idiosyncratic_sigma=0.0018,
        maximum_candle_move=0.025,
        anchor_reversion_strength=0.01,
        random_seed=42,
        reset_test_data=False,
        use_cython=True,
        require_cython=True,
        quiet=True,
        garbage_collect_each_cycle=True,
        compact_test_files_every_ticks=100,
    ):
        if history_candles < 500:
            raise ValueError("history_candles must be at least 500")
        if interval_seconds <= 0:
            raise ValueError("interval_seconds must be positive")
        if expected_share_count < 1:
            raise ValueError("expected_share_count must be positive")

        self.symbols_file = str(symbols_file)
        self.source_data_folder = Path(source_data_folder).expanduser().resolve()
        self.nifty_filename = str(nifty_filename)
        self.tests_folder = Path(tests_folder).expanduser().resolve()
        self.test_data_folder = self.tests_folder / "synthetic_market_data"
        self.history_candles = int(history_candles)
        self.expected_share_count = int(expected_share_count)
        self.interval_seconds = float(interval_seconds)
        self.market_sigma = float(market_sigma)
        self.idiosyncratic_sigma = float(idiosyncratic_sigma)
        self.maximum_candle_move = float(maximum_candle_move)
        self.anchor_reversion_strength = float(anchor_reversion_strength)
        self.random_seed = random_seed
        self.reset_test_data = bool(reset_test_data)
        self.use_cython = bool(use_cython)
        self.require_cython = bool(require_cython)
        self.quiet = bool(quiet)
        self.garbage_collect_each_cycle = bool(garbage_collect_each_cycle)
        if compact_test_files_every_ticks < 1:
            raise ValueError("compact_test_files_every_ticks must be positive")
        self.compact_test_files_every_ticks = int(
            compact_test_files_every_ticks
        )

        self.rng = np.random.default_rng(random_seed)
        self.symbols = self._load_symbols()
        if len(self.symbols) != self.expected_share_count:
            raise RuntimeError(
                f"Expected {self.expected_share_count} tradable shares, "
                f"but found {len(self.symbols)}"
            )

        self.tests_folder.mkdir(parents=True, exist_ok=True)
        self.test_data_folder.mkdir(parents=True, exist_ok=True)
        self.paths = {
            "alignment": self.tests_folder / "Test_Alignment.xlsx",
            "residual": self.tests_folder / "Test_ResidualEngine.xlsx",
            "regime": self.tests_folder / "Test_RegimeEngine.xlsx",
            "volume": self.tests_folder / "Test_VolumeEngine.xlsx",
            "volatility": self.tests_folder / "Test_VolatilityEngine.xlsx",
            "score": self.tests_folder / "Test_MathematicalScoreEngine.xlsx",
            "ranking": self.tests_folder / "Test_RankingShares.xlsx",
            "status": self.tests_folder / "Test_PhaseA_Status.xlsx",
            "state": self.tests_folder / "phase_a_state.json",
        }

        self.market_state = {}
        self.cycle_number = 0
        self.generated_tick_count = 0
        self.last_result = None
        self.last_error = None
        self._pipeline_thread = None
        self._generator_thread = None
        self._stop_event = threading.Event()
        self._cycle_lock = threading.Lock()
        self._market_lock = threading.RLock()

        self._prepare_test_market()

    @staticmethod
    def _normalized_benchmark(symbol):
        return (
            unquote(str(symbol)).replace(" ", "").replace("_", "").upper()
        )

    def _load_symbols(self):
        if not os.path.isfile(self.symbols_file):
            raise FileNotFoundError(f"Symbols file not found: {self.symbols_file}")
        with open(self.symbols_file, "r", encoding="utf-8") as handle:
            values = [line.strip().upper() for line in handle if line.strip()]
        values = list(dict.fromkeys(values))
        return [
            symbol for symbol in values
            if self._normalized_benchmark(symbol) not in self.BENCHMARK_NAMES
        ]

    def _source_path(self, symbol=None, nifty=False):
        if nifty:
            return self.source_data_folder / self.nifty_filename
        return self.source_data_folder / f"{symbol}_5mins.txt"

    def _test_path(self, symbol=None, nifty=False):
        if nifty:
            return self.test_data_folder / self.nifty_filename
        return self.test_data_folder / f"{symbol}_5mins.txt"

    def _read_market_file(self, path, symbol, limit=None, require_volume=True):
        if not path.is_file():
            raise FileNotFoundError(f"Five-minute file not found: {path}")
        frame = pd.read_csv(
            path, names=self.COLUMNS, header=None, skiprows=1, low_memory=False
        )
        frame["DateTime"] = pd.to_datetime(
            frame["Date"].astype(str).str.strip()
            + " " + frame["Time"].astype(str).str.strip(),
            errors="coerce", format="mixed",
        )
        for column in ["Open", "High", "Low", "Close", "Volume"]:
            frame[column] = pd.to_numeric(frame[column], errors="coerce")
        required = ["DateTime", "Open", "High", "Low", "Close"]
        if require_volume:
            required.append("Volume")
        frame.dropna(subset=required, inplace=True)
        if not require_volume:
            frame["Volume"] = frame["Volume"].fillna(0)
        valid = (
            frame[["Open", "High", "Low", "Close"]].gt(0).all(axis=1)
            & frame["High"].ge(frame[["Open", "Close"]].max(axis=1))
            & frame["Low"].le(frame[["Open", "Close"]].min(axis=1))
        )
        frame = frame.loc[valid].copy()
        frame["Symbol"] = symbol
        frame.sort_values("DateTime", inplace=True)
        frame.drop_duplicates("DateTime", keep="last", inplace=True)
        if limit is not None:
            frame = frame.tail(limit).copy()
        frame.reset_index(drop=True, inplace=True)
        return frame

    @staticmethod
    def _atomic_text_dataframe(frame, target):
        target = Path(target)
        target.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(
            prefix=f".{target.stem}_", suffix=".txt", dir=target.parent
        )
        os.close(descriptor)
        try:
            frame.to_csv(temporary, columns=FiveMinuteForwardTestPhaseA.COLUMNS,
                         index=False)
            os.replace(temporary, target)
        except Exception:
            if os.path.exists(temporary):
                os.unlink(temporary)
            raise

    def _seed_one(self, symbol, source, target, require_volume):
        frame = self._read_market_file(
            source, symbol, limit=self.history_candles,
            require_volume=require_volume,
        )
        if len(frame) < 450:
            raise RuntimeError(f"{symbol} has insufficient seed history: {len(frame)}")
        self._atomic_text_dataframe(frame, target)
        returns = np.log(frame["Close"]).diff().dropna()
        sigma = float(returns.tail(100).std())
        if not np.isfinite(sigma) or sigma <= 0:
            sigma = self.idiosyncratic_sigma
        positive_volume = frame.loc[frame["Volume"].gt(0), "Volume"].tail(100)
        average_volume = float(positive_volume.median()) if not positive_volume.empty else 1.0
        state = {
            "price": float(frame["Close"].iloc[-1]),
            "anchor": float(frame["Close"].iloc[-1]),
            "volume": max(1.0, average_volume),
            "sigma": min(max(sigma, 0.0003), self.maximum_candle_move / 2),
            "timestamp": pd.Timestamp(frame["DateTime"].iloc[-1]),
        }
        del frame, returns, positive_volume
        return state

    def _state_from_test_file(self, symbol, target, require_volume):
        frame = self._read_market_file(
            target, symbol, limit=100, require_volume=require_volume
        )
        returns = np.log(frame["Close"]).diff().dropna()
        positive_volume = frame.loc[frame["Volume"].gt(0), "Volume"]
        state = {
            "price": float(frame["Close"].iloc[-1]),
            "anchor": float(frame["Close"].iloc[0]),
            "volume": float(positive_volume.median()) if not positive_volume.empty else 1.0,
            "sigma": float(returns.std()) if len(returns) > 1 else self.idiosyncratic_sigma,
            "timestamp": pd.Timestamp(frame["DateTime"].iloc[-1]),
        }
        state["sigma"] = min(max(state["sigma"], 0.0003), self.maximum_candle_move / 2)
        del frame, returns, positive_volume
        return state

    def _prepare_test_market(self):
        instruments = [("NIFTY%2050", True)] + [(s, False) for s in self.symbols]
        latest_timestamp = pd.Timestamp.min
        for symbol, nifty in instruments:
            source = self._source_path(symbol, nifty=nifty)
            target = self._test_path(symbol, nifty=nifty)
            require_volume = not nifty
            if self.reset_test_data or not target.is_file():
                state = self._seed_one(
                    "NIFTY 50" if nifty else symbol,
                    source, target, require_volume,
                )
            else:
                state = self._state_from_test_file(
                    "NIFTY 50" if nifty else symbol, target, require_volume
                )
            key = "__NIFTY__" if nifty else symbol
            state["beta"] = 1.0 if nifty else float(self.rng.uniform(0.45, 1.55))
            self.market_state[key] = state
            latest_timestamp = max(latest_timestamp, state["timestamp"])

        # All new synthetic candles use one common clock for valid alignment.
        for state in self.market_state.values():
            state["timestamp"] = latest_timestamp
        if self.paths["state"].is_file() and not self.reset_test_data:
            try:
                saved = json.loads(self.paths["state"].read_text())
                self.cycle_number = int(saved.get("cycle_number", 0))
                self.generated_tick_count = int(
                    saved.get("generated_tick_count", 0)
                )
            except (ValueError, OSError):
                pass

    @staticmethod
    def _next_market_timestamp(timestamp):
        candidate = pd.Timestamp(timestamp) + pd.Timedelta(minutes=5)
        if candidate.time() > clock_time(15, 30):
            candidate = candidate.normalize() + pd.Timedelta(days=1, hours=9, minutes=15)
        while candidate.weekday() >= 5:
            candidate += pd.Timedelta(days=1)
        if candidate.time() < clock_time(9, 15):
            candidate = candidate.normalize() + pd.Timedelta(hours=9, minutes=15)
        return candidate

    def _append_row(self, target, values):
        with open(target, "a", encoding="utf-8") as handle:
            handle.write(",".join(str(value) for value in values) + "\n")

    def _generate_candles(self):
        with self._market_lock:
            market_state = self.market_state["__NIFTY__"]
            timestamp = self._next_market_timestamp(market_state["timestamp"])
            market_return = float(np.clip(
                self.rng.normal(0.0, self.market_sigma),
                -self.maximum_candle_move, self.maximum_candle_move,
            ))
            generated = []

            for key, state in self.market_state.items():
                is_nifty = key == "__NIFTY__"
                symbol = "NIFTY 50" if is_nifty else key
                previous = state["price"]
                if is_nifty:
                    value_return = market_return
                else:
                    anchor_pull = -self.anchor_reversion_strength * np.log(
                        max(previous, 1e-12) / max(state["anchor"], 1e-12)
                    )
                    value_return = (
                        state["beta"] * market_return
                        + anchor_pull
                        + self.rng.normal(
                            0.0, min(state["sigma"], self.idiosyncratic_sigma)
                        )
                    )
                value_return = float(np.clip(
                    value_return, -self.maximum_candle_move,
                    self.maximum_candle_move
                ))
                opening = previous
                closing = max(0.01, previous * np.exp(value_return))
                excursion = (
                    abs(self.rng.normal()) * state["sigma"] * previous * 0.55
                )
                high = max(opening, closing) + excursion
                low = max(0.01, min(opening, closing) - excursion)
                volume = 0 if is_nifty else int(max(
                    1, state["volume"] * self.rng.lognormal(0.0, 0.30)
                ))
                row = [
                    symbol, timestamp.strftime("%Y-%m-%d"),
                    timestamp.strftime("%H:%M:%S"),
                    f"{opening:.6f}", f"{high:.6f}", f"{low:.6f}",
                    f"{closing:.6f}", volume,
                ]
                target = self._test_path(key, nifty=is_nifty)
                self._append_row(target, row)
                state["price"] = closing
                state["timestamp"] = timestamp
                generated.append((symbol, closing))
            self.generated_tick_count += 1
            if (
                self.generated_tick_count
                % self.compact_test_files_every_ticks
                == 0
            ):
                self._compact_test_files()
            return timestamp, generated

    def _compact_test_files(self):
        instruments = [("NIFTY 50", True)] + [
            (symbol, False) for symbol in self.symbols
        ]
        for symbol, nifty in instruments:
            target = self._test_path(symbol, nifty=nifty)
            frame = self._read_market_file(
                target, symbol, limit=self.history_candles,
                require_volume=not nifty,
            )
            self._atomic_text_dataframe(frame, target)
            del frame

    def _load_cycle_market(self):
        # Hold the lock only while taking a consistent 81-instrument snapshot.
        # The generator can continue while the seven engines process it.
        with self._market_lock:
            shares = {}
            for symbol in self.symbols:
                shares[symbol] = self._read_market_file(
                    self._test_path(symbol), symbol,
                    limit=self.history_candles, require_volume=True,
                )
            nifty = self._read_market_file(
                self._test_path(nifty=True), "NIFTY 50",
                limit=self.history_candles, require_volume=False,
            )
            return shares, nifty

    @staticmethod
    def _atomic_report(frame, path, sheet_name="Summary"):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(
            prefix=f".{path.stem}_", suffix=".xlsx", dir=path.parent
        )
        os.close(descriptor)
        try:
            with pd.ExcelWriter(temporary, engine="openpyxl") as writer:
                frame.to_excel(writer, sheet_name=sheet_name, index=False)
                worksheet = writer.book[sheet_name]
                worksheet.freeze_panes = "A2"
                worksheet.auto_filter.ref = worksheet.dimensions
            os.replace(temporary, path)
        except Exception:
            if os.path.exists(temporary):
                os.unlink(temporary)
            raise

    def _build_engines(self):
        from engines.FiveMinuteMarketDataAligner import FiveMinuteMarketDataAligner
        from engines.FiveMinuteResidualEngine import FiveMinuteResidualEngine
        from engines.FiveMinuteRegimeEngine import FiveMinuteRegimeEngine
        from engines.FiveMinuteVolumeEngine import FiveMinuteVolumeEngine
        from engines.FiveMinuteVolatilityEngine import FiveMinuteVolatilityEngine
        from engines.FiveMinuteMathematicalScoreEngine import FiveMinuteMathematicalScoreEngine
        from engines.FiveMinuteCrossSectionalRanker import FiveMinuteCrossSectionalRanker

        return {
            "aligner": FiveMinuteMarketDataAligner(
                minimum_aligned_candles=450, minimum_alignment_ratio=0.90
            ),
            "residual": FiveMinuteResidualEngine(
                beta_window=375, beta_min_periods=350, impulse_window=3,
                zscore_window=60, zscore_min_periods=50,
                beta_minimum=-1.0, beta_maximum=3.0,
                variance_floor=1e-12, mad_floor=1e-12, zscore_clip=5.0,
                use_cython=self.use_cython, memory_optimized=True,
                retain_results_in_memory=True, save_excel=False,
                print_symbol_details=False,
            ),
            "regime": FiveMinuteRegimeEngine(
                autocorrelation_window=60, autocorrelation_min_periods=50,
                momentum_threshold=0.10, mean_reversion_threshold=-0.10,
                stability_candles=3, strength_reference=0.30,
                numerical_epsilon=1e-12, use_cython=self.use_cython,
                memory_optimized=True, retain_results_in_memory=True,
                save_excel=False,
            ),
            "volume": FiveMinuteVolumeEngine(
                rolling_volume_window=20, rolling_volume_min_periods=15,
                time_slot_lookback_days=10, time_slot_min_periods=5,
                momentum_minimum_rvol=1.25,
                momentum_minimum_time_rvol=1.20,
                reversion_minimum_rvol=1.00, maximum_volume_factor=3.0,
                volume_floor=1e-12, use_cython=self.use_cython,
                memory_optimized=True, retain_results_in_memory=True,
                save_full_history_to_excel=False, save_excel=False,
            ),
            "volatility": FiveMinuteVolatilityEngine(
                ewma_span=20, ewma_min_periods=15,
                minimum_normalized_volatility=0.001,
                high_normalized_volatility=0.005,
                maximum_normalized_volatility=0.015,
                numerical_epsilon=1e-12,
                reset_previous_close_at_session_start=True,
            ),
            "score": FiveMinuteMathematicalScoreEngine(
                residual_weight=0.35, regime_weight=0.25,
                volume_weight=0.20, consistency_weight=0.20,
                residual_zscore_reference=3.0, volume_factor_reference=2.0,
                momentum_consistency_window=5,
                reversion_consistency_window=3,
                minimum_mathematical_score=65.0,
                memory_optimized=True,
                report_output_file=str(self.paths["score"]),
                save_excel=True, print_symbol_details=False,
            ),
            "ranker": FiveMinuteCrossSectionalRanker(
                maximum_selected_symbols=5,
                mathematical_weight=0.60, residual_weight=0.25,
                volume_weight=0.15, maximum_staleness_minutes=5,
                mathematical_score_file=str(self.paths["score"]),
                accepted_sheet_name="Accepted_Setups",
                ranking_output_file=str(self.paths["ranking"]),
                save_excel=True, memory_optimized=True,
                print_symbol_details=False,
            ),
        }

    def _verify_backends(self, engines):
        checks = [
            ("Residual", engines["residual"].robust_stats_backend),
            ("Regime", engines["regime"].regime_backend),
            ("Volume", engines["volume"].volume_backend),
        ]
        if self.require_cython:
            failed = [name for name, backend in checks if backend != "CYTHON"]
            if failed:
                raise RuntimeError(f"Cython backend unavailable for: {failed}")
        return dict(checks)

    def run_one_cycle(self, generate_candle=True):
        if not self._cycle_lock.acquire(blocking=False):
            raise RuntimeError("Phase A cycle is already running")
        started = time.perf_counter()
        removed = []
        try:
            self.cycle_number += 1
            if generate_candle:
                timestamp, generated = self._generate_candles()
            else:
                with self._market_lock:
                    timestamp = self.market_state["__NIFTY__"]["timestamp"]
                generated = [("NIFTY 50", np.nan)] + [
                    (symbol, np.nan) for symbol in self.symbols
                ]
            shares_data, nifty_data = self._load_cycle_market()
            engines = self._build_engines()
            backends = self._verify_backends(engines)

            output_buffer = io.StringIO()
            stream = output_buffer if self.quiet else None
            context = redirect_stdout(stream) if stream is not None else _NullContext()
            with context:
                aligned_data, alignment_report = engines["aligner"].align_all(
                    shares_data=shares_data, nifty_data=nifty_data
                )
                del shares_data, nifty_data
                removed += ["shares_data", "nifty_data"]
                self._atomic_report(alignment_report, self.paths["alignment"], "Alignment")

                residual_data, residual_report = engines["residual"].calculate_all(
                    aligned_data=aligned_data
                )
                del aligned_data
                removed.append("aligned_data")
                self._atomic_report(residual_report, self.paths["residual"], "Residual_Summary")

                regime_data, regime_report = engines["regime"].calculate_all(
                    residual_data=residual_data
                )
                del residual_data, residual_report
                removed += ["residual_data", "residual_report"]
                self._atomic_report(regime_report, self.paths["regime"], "Regime_Summary")

                volume_data, volume_report = engines["volume"].calculate_all(
                    regime_data=regime_data
                )
                del regime_data, regime_report
                removed += ["regime_data", "regime_report"]
                self._atomic_report(volume_report, self.paths["volume"], "Volume_Summary")

                volatility_data, volatility_report = engines["volatility"].calculate_all(
                    volume_data=volume_data
                )
                del volume_data, volume_report
                removed += ["volume_data", "volume_report"]
                self._atomic_report(
                    volatility_report, self.paths["volatility"], "Volatility_Summary"
                )

                score_data, score_report = engines["score"].calculate_all(
                    volatility_data=volatility_data
                )
                del volatility_data, volatility_report
                removed += ["volatility_data", "volatility_report"]

                ranked_data, ranking_report = engines["ranker"].rank_from_excel()
                del score_data, score_report, ranked_data
                removed += ["score_data", "score_report", "ranked_data"]

            output_buffer.close()
            selected = int(
                ranking_report["Latest_Selected"].fillna(False).sum()
            ) if not ranking_report.empty else 0
            accepted = len(ranking_report)
            del alignment_report
            removed.append("alignment_report")

            runtime = time.perf_counter() - started
            result = {
                "Cycle": self.cycle_number,
                "Synthetic_DateTime": timestamp,
                "Shares_Generated": len(generated) - 1,
                "Accepted_Setups": accepted,
                "Selected_Shares": selected,
                "Runtime_Seconds": runtime,
                "Backends": str(backends),
                "Removed_Objects": ", ".join(removed),
                "Score_File": str(self.paths["score"]),
                "Ranking_File": str(self.paths["ranking"]),
                "Status": "COMPLETED",
            }
            self.last_result = result
            self.last_error = None
            self._atomic_report(pd.DataFrame([result]), self.paths["status"], "Phase_A")
            self.paths["state"].write_text(json.dumps({
                "cycle_number": self.cycle_number,
                "generated_tick_count": self.generated_tick_count,
                "synthetic_datetime": str(timestamp),
            }))
            del ranking_report, generated, engines
            if self.garbage_collect_each_cycle:
                gc.collect()
            print(
                f"Phase A cycle {self.cycle_number} | Synthetic={timestamp} | "
                f"Accepted={accepted} | Selected={selected} | "
                f"Runtime={runtime:.2f}s"
            )
            return result
        except Exception as error:
            self.last_error = f"{type(error).__name__}: {error}"
            failure = pd.DataFrame([{
                "Cycle": self.cycle_number,
                "Status": "FAILED",
                "Error": self.last_error,
            }])
            self._atomic_report(failure, self.paths["status"], "Phase_A")
            raise
        finally:
            self._cycle_lock.release()

    def _generator_loop(self):
        while not self._stop_event.wait(self.interval_seconds):
            try:
                self._generate_candles()
            except Exception as error:
                self.last_error = f"{type(error).__name__}: {error}"
                print(f"Synthetic candle generation failed: {self.last_error}")

    def _pipeline_loop(self):
        # Avoid a duplicate pipeline pass immediately after start's first run.
        if self._stop_event.wait(self.interval_seconds):
            return
        while not self._stop_event.is_set():
            started = time.monotonic()
            try:
                self.run_one_cycle(generate_candle=False)
            except Exception as error:
                print(f"Phase A cycle failed: {type(error).__name__}: {error}")
            remaining = max(0.0, self.interval_seconds - (time.monotonic() - started))
            self._stop_event.wait(remaining)

    def start(self, run_immediately=True):
        if self.is_running:
            raise RuntimeError("Phase A is already running")
        self._stop_event.clear()
        self._generator_thread = threading.Thread(
            target=self._generator_loop,
            name="FMRSS-Synthetic-Market-Generator",
            daemon=True,
        )
        # Start the one-second feed before the first potentially long pipeline
        # pass, so candle generation is never blocked by engine runtime.
        self._generator_thread.start()
        try:
            if run_immediately:
                self.run_one_cycle()
        except Exception:
            self._stop_event.set()
            self._generator_thread.join(timeout=10.0)
            raise
        self._pipeline_thread = threading.Thread(
            target=self._pipeline_loop,
            name="FMRSS-Forward-Test-Phase-A",
            daemon=True,
        )
        self._pipeline_thread.start()
        return self._pipeline_thread

    def stop(self, timeout=60.0):
        self._stop_event.set()
        for thread in [self._generator_thread, self._pipeline_thread]:
            if thread is not None:
                thread.join(timeout=timeout)
        return not self.is_running

    @property
    def is_running(self):
        return bool(
            (self._generator_thread and self._generator_thread.is_alive())
            or (self._pipeline_thread and self._pipeline_thread.is_alive())
        )


class _NullContext:
    def __enter__(self):
        return None

    def __exit__(self, exc_type, exc_value, traceback):
        return False


def main():
    """Terminal entry point. Stop safely with Ctrl+C."""
    import argparse

    parser = argparse.ArgumentParser(
        description=(
            "Run FMRSS Phase A with one synthetic five-minute candle "
            "generated per real-time interval."
        )
    )
    parser.add_argument(
        "--symbols-file",
        default="/home/devinderjeet/fmrss/config/shares.txt",
    )
    parser.add_argument(
        "--source-data-folder",
        default="/home/hadoop/shareMarket_Data",
    )
    parser.add_argument(
        "--nifty-filename",
        default="NIFTY%2050_5mins.txt",
    )
    parser.add_argument(
        "--tests-folder",
        default="/home/devinderjeet/fmrss/tests",
    )
    parser.add_argument("--history-candles", type=int, default=1500)
    parser.add_argument("--expected-share-count", type=int, default=80)
    parser.add_argument("--interval-seconds", type=float, default=1.0)
    parser.add_argument("--market-sigma", type=float, default=0.0012)
    parser.add_argument("--idiosyncratic-sigma", type=float, default=0.0018)
    parser.add_argument("--maximum-candle-move", type=float, default=0.025)
    parser.add_argument("--anchor-reversion-strength", type=float, default=0.01)
    parser.add_argument("--random-seed", type=int, default=42)
    parser.add_argument("--compact-every-ticks", type=int, default=100)
    parser.add_argument(
        "--reset-test-data",
        action="store_true",
        help="Discard earlier synthetic files and reseed them from real data.",
    )
    parser.add_argument(
        "--require-cython",
        action="store_true",
        help="Stop if a Residual, Regime, or Volume Cython backend is missing.",
    )
    parser.add_argument(
        "--show-engine-output",
        action="store_true",
        help="Show detailed output emitted by every Phase A engine.",
    )
    parser.add_argument(
        "--run-first-cycle-in-foreground",
        action="store_true",
        help="Finish the first pipeline cycle before returning to the run loop.",
    )
    arguments = parser.parse_args()

    phase_a = FiveMinuteForwardTestPhaseA(
        symbols_file=arguments.symbols_file,
        source_data_folder=arguments.source_data_folder,
        nifty_filename=arguments.nifty_filename,
        tests_folder=arguments.tests_folder,
        history_candles=arguments.history_candles,
        expected_share_count=arguments.expected_share_count,
        interval_seconds=arguments.interval_seconds,
        market_sigma=arguments.market_sigma,
        idiosyncratic_sigma=arguments.idiosyncratic_sigma,
        maximum_candle_move=arguments.maximum_candle_move,
        anchor_reversion_strength=arguments.anchor_reversion_strength,
        random_seed=arguments.random_seed,
        reset_test_data=arguments.reset_test_data,
        use_cython=True,
        require_cython=arguments.require_cython,
        quiet=not arguments.show_engine_output,
        garbage_collect_each_cycle=True,
        compact_test_files_every_ticks=arguments.compact_every_ticks,
    )

    print("=" * 100)
    print("FMRSS FORWARD TEST : PHASE A")
    print(f"Shares                 : {len(phase_a.symbols)}")
    print(f"Synthetic interval     : {phase_a.interval_seconds:.2f} second(s)")
    print(f"Tests folder           : {phase_a.tests_folder}")
    print("Stop command           : Ctrl+C")
    print("=" * 100)

    try:
        phase_a.start(
            run_immediately=arguments.run_first_cycle_in_foreground
        )
        while phase_a.is_running:
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\nStopping Phase A safely...")
    finally:
        stopped = phase_a.stop(timeout=60.0)
        print(f"Phase A stopped        : {stopped}")
        print(f"Synthetic ticks        : {phase_a.generated_tick_count}")
        print(f"Completed pipelines    : {phase_a.cycle_number}")
        if phase_a.last_error:
            print(f"Last error             : {phase_a.last_error}")


if __name__ == "__main__":
    main()
