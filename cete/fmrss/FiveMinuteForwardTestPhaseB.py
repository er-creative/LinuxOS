import argparse
import gc
import io
import json
import os
import tempfile
import threading
import time
from contextlib import redirect_stdout
from pathlib import Path

import numpy as np
import pandas as pd


class FiveMinuteForwardTestPhaseB:
    """Update-driven paper execution for FMRSS forward-test Phase B.

    Ranking changes trigger Signal and Risk Engines. Open positions are checked
    against Phase A's synthetic five-minute candles every polling cycle.
    """

    def __init__(
        self,
        tests_folder="/home/devinderjeet/fmrss/tests",
        synthetic_data_folder=None,
        ranking_file=None,
        mathematical_score_file=None,
        signal_file=None,
        signal_state_file=None,
        trading_ledger_file=None,
        manual_ledger_file=None,
        position_journal_file=None,
        strategy_file=None,
        status_file=None,
        phase_a_state_file=None,
        poll_interval_seconds=1.0,
        maximum_selected_symbols=5,
        initial_capital=100000.0,
        risk_per_trade=0.0025,
        maximum_allocation_per_trade=0.20,
        maximum_portfolio_exposure=0.80,
        maximum_open_positions=2,
        maximum_entry_rank=5,
        maximum_signal_age_minutes=5.0,
        slippage_rate=0.0001,
        brokerage_rate=0.0003,
        brokerage_cap_per_order=20.0,
        stt_sell_rate=0.00025,
        nse_transaction_charge_rate=0.0000307,
        gst_rate=0.18,
        sebi_turnover_charge_rate=0.000001,
        stamp_duty_buy_rate=0.00003,
        enable_profit_protection=True,
        break_even_trigger_r=0.50,
        break_even_cost_buffer_multiplier=1.20,
        trailing_trigger_r=1.00,
        trailing_lock_r=0.50,
        trailing_distance_r=0.50,
        force_exit_at_session_end=True,
        session_exit_time="15:14",
        entry_confirmation_cycles=1,
        reversal_confirmation_cycles=2,
        reentry_cooldown_candles=6,
        stop_loss_cooldown_candles=12,
        mean_reversion_normalization_zscore=0.50,
        mean_reversion_arm_abs_zscore=2.50,
        mean_reversion_entry_abs_zscore=2.20,
        mean_reversion_minimum_remaining_abs_zscore=1.50,
        minimum_reversion_confirmations=2,
        maximum_armed_age_candles=12,
        minimum_expected_net_reward_risk=1.25,
        enable_benchmark_trend_filter=True,
        benchmark_trend_candles=6,
        maximum_adverse_benchmark_return=0.002,
        mean_reversion_maximum_holding_candles=12,
        momentum_maximum_holding_candles=18,
        display_open_positions_each_cycle=True,
        quiet_engines=True,
        garbage_collect_each_cycle=True,
    ):
        if poll_interval_seconds <= 0:
            raise ValueError("poll_interval_seconds must be positive")
        if maximum_selected_symbols < 1:
            raise ValueError("maximum_selected_symbols must be positive")
        if maximum_entry_rank < 1:
            raise ValueError("maximum_entry_rank must be positive")
        if maximum_entry_rank > maximum_selected_symbols:
            raise ValueError(
                "maximum_entry_rank cannot exceed maximum_selected_symbols"
            )

        folder = Path(tests_folder).expanduser().resolve()
        folder.mkdir(parents=True, exist_ok=True)
        self.tests_folder = folder
        self.synthetic_data_folder = Path(
            synthetic_data_folder
            or folder / "synthetic_market_data"
        ).expanduser().resolve()
        self.ranking_file = Path(
            ranking_file or folder / "Test_RankingShares.xlsx"
        ).expanduser().resolve()
        self.mathematical_score_file = Path(
            mathematical_score_file
            or folder / "Test_MathematicalScoreEngine.xlsx"
        ).expanduser().resolve()
        self.signal_file = Path(
            signal_file or folder / "Test_MathematicalSignals.xlsx"
        ).expanduser().resolve()
        self.signal_state_file = Path(
            signal_state_file
            or folder.parent / "state" / f"{folder.name}_SignalState.xlsx"
        ).expanduser().resolve()
        self.trading_ledger_file = Path(
            trading_ledger_file or folder / "Test_TradingLedger.xlsx"
        ).expanduser().resolve()
        # Separate execution ledger. The Risk Engine continues to own
        # Test_TradingLedger.xlsx without any change.
        self.manual_ledger_file = Path(
            manual_ledger_file or folder / "TradingLedge.xlsx"
        ).expanduser().resolve()
        # Recovery state deliberately lives outside the disposable tests
        # directory. Deleting tests_live therefore cannot erase open trades.
        self.position_journal_file = Path(
            position_journal_file
            or folder.parent / "state" / f"{folder.name}_OpenPositionJournal.xlsx"
        ).expanduser().resolve()
        self.strategy_file = Path(
            strategy_file
            or folder / "Test_ResidualStatisticalStrategy.xlsx"
        ).expanduser().resolve()
        self.status_file = Path(
            status_file or folder / "Test_PhaseB_Status.xlsx"
        ).expanduser().resolve()
        self.phase_a_state_file = Path(
            phase_a_state_file or folder / "phase_a_state.json"
        ).expanduser().resolve()
        self.phase_a_status_file = folder / "Test_PhaseA_Status.xlsx"

        self.poll_interval_seconds = float(poll_interval_seconds)
        self.maximum_selected_symbols = int(maximum_selected_symbols)
        self.initial_capital = float(initial_capital)
        self.risk_per_trade = float(risk_per_trade)
        self.maximum_allocation_per_trade = float(
            maximum_allocation_per_trade
        )
        self.maximum_portfolio_exposure = float(maximum_portfolio_exposure)
        self.maximum_open_positions = int(maximum_open_positions)
        self.maximum_entry_rank = int(maximum_entry_rank)
        self.maximum_signal_age_minutes = float(maximum_signal_age_minutes)
        self.slippage_rate = float(slippage_rate)
        self.brokerage_rate = float(brokerage_rate)
        self.brokerage_cap_per_order = float(brokerage_cap_per_order)
        self.stt_sell_rate = float(stt_sell_rate)
        self.nse_transaction_charge_rate = float(
            nse_transaction_charge_rate
        )
        self.gst_rate = float(gst_rate)
        self.sebi_turnover_charge_rate = float(
            sebi_turnover_charge_rate
        )
        self.stamp_duty_buy_rate = float(stamp_duty_buy_rate)
        self.enable_profit_protection = bool(enable_profit_protection)
        self.break_even_trigger_r = float(break_even_trigger_r)
        self.break_even_cost_buffer_multiplier = float(
            break_even_cost_buffer_multiplier
        )
        self.trailing_trigger_r = float(trailing_trigger_r)
        self.trailing_lock_r = float(trailing_lock_r)
        self.trailing_distance_r = float(trailing_distance_r)
        self.force_exit_at_session_end = bool(force_exit_at_session_end)
        self.session_exit_time = str(session_exit_time)
        self.entry_confirmation_cycles = int(entry_confirmation_cycles)
        self.reversal_confirmation_cycles = int(reversal_confirmation_cycles)
        self.reentry_cooldown_candles = int(reentry_cooldown_candles)
        self.stop_loss_cooldown_candles = int(stop_loss_cooldown_candles)
        self.mean_reversion_normalization_zscore = float(
            mean_reversion_normalization_zscore
        )
        self.mean_reversion_arm_abs_zscore = float(
            mean_reversion_arm_abs_zscore
        )
        self.mean_reversion_entry_abs_zscore = float(
            mean_reversion_entry_abs_zscore
        )
        self.mean_reversion_minimum_remaining_abs_zscore = float(
            mean_reversion_minimum_remaining_abs_zscore
        )
        self.minimum_reversion_confirmations = int(
            minimum_reversion_confirmations
        )
        self.maximum_armed_age_candles = int(maximum_armed_age_candles)
        self.minimum_expected_net_reward_risk = float(
            minimum_expected_net_reward_risk
        )
        self.enable_benchmark_trend_filter = bool(
            enable_benchmark_trend_filter
        )
        self.benchmark_trend_candles = int(benchmark_trend_candles)
        self.maximum_adverse_benchmark_return = float(
            maximum_adverse_benchmark_return
        )
        self.mean_reversion_maximum_holding_candles = int(
            mean_reversion_maximum_holding_candles
        )
        self.momentum_maximum_holding_candles = int(
            momentum_maximum_holding_candles
        )
        self.display_open_positions_each_cycle = bool(
            display_open_positions_each_cycle
        )
        self.quiet_engines = bool(quiet_engines)
        self.garbage_collect_each_cycle = bool(garbage_collect_each_cycle)

        self.cycle_number = 0
        self.monitor_cycle_number = 0
        self.phase_a_cycle = 0
        self.phase_a_market_datetime = pd.NaT
        self._last_snapshot_id = None
        self.ranking_updates_processed = 0
        self.last_error = None
        self.last_result = None
        self._latest_approved_plans = []
        self._last_ranking_token = None
        self._phase_a_snapshot_error = "Phase A snapshot has not been read"
        self._stop_event = threading.Event()
        self._thread = None
        self._cycle_lock = threading.Lock()
        self.strategy = self._create_strategy()

    @staticmethod
    def _file_token(path):
        if not Path(path).is_file():
            return None
        stat = Path(path).stat()
        return stat.st_mtime_ns, stat.st_size

    def _read_phase_a_snapshot(self):
        """Read Phase A's manifest, with its atomic status workbook as fallback."""
        errors = []
        if self.phase_a_state_file.is_file():
            try:
                with open(
                    self.phase_a_state_file, "r", encoding="utf-8"
                ) as handle:
                    state = json.load(handle)
                status = str(state.get("status", "")).strip().upper()
                cycle = int(state.get("cycle_number", state.get("Cycle", 0)))
                market_time = pd.to_datetime(
                    state.get(
                        "synthetic_datetime",
                        state.get("market_datetime"),
                    ),
                    errors="coerce",
                )
                if status != "COMPLETED":
                    errors.append(f"manifest status is {status or 'missing'}")
                elif cycle < 1:
                    errors.append("manifest cycle_number is missing or zero")
                elif pd.isna(market_time):
                    errors.append("manifest market timestamp is invalid")
                else:
                    snapshot_id = (
                        state.get("snapshot_id")
                        or f"{cycle}:{pd.Timestamp(market_time).isoformat()}"
                    )
                    self._phase_a_snapshot_error = ""
                    return {
                        "cycle": cycle,
                        "market_datetime": pd.Timestamp(market_time),
                        "snapshot_id": str(snapshot_id),
                        "ranking_token": (
                            int(state["ranking_mtime_ns"]),
                            int(state["ranking_size"]),
                        ) if "ranking_mtime_ns" in state
                        and "ranking_size" in state else None,
                        "score_token": (
                            int(state["score_mtime_ns"]),
                            int(state["score_size"]),
                        ) if "score_mtime_ns" in state
                        and "score_size" in state else None,
                    }
            except (OSError, ValueError, TypeError, json.JSONDecodeError) as error:
                errors.append(
                    f"cannot parse {self.phase_a_state_file.name}: {error}"
                )
        else:
            errors.append(f"{self.phase_a_state_file.name} is missing")

        # Recovery path for an older Phase A copy or a temporarily unavailable
        # JSON manifest. Phase A writes this workbook atomically after completing
        # the ranking pipeline, so its last row is a safe completed snapshot.
        if self.phase_a_status_file.is_file():
            try:
                status_frame = pd.read_excel(
                    self.phase_a_status_file,
                    sheet_name="Phase_A",
                    engine="openpyxl",
                )
                if not status_frame.empty:
                    row = status_frame.iloc[-1]
                    status = str(row.get("Status", "")).strip().upper()
                    cycle = int(row.get("Cycle", 0))
                    market_time = pd.to_datetime(
                        row.get("Synthetic_DateTime"), errors="coerce"
                    )
                    if status == "COMPLETED" and cycle >= 1 \
                            and pd.notna(market_time):
                        token = self._file_token(self.phase_a_status_file)
                        self._phase_a_snapshot_error = ""
                        del status_frame
                        return {
                            "cycle": cycle,
                            "market_datetime": pd.Timestamp(market_time),
                            "snapshot_id": (
                                f"status:{cycle}:"
                                f"{pd.Timestamp(market_time).isoformat()}:"
                                f"{token}"
                            ),
                            "ranking_token": None,
                            "score_token": None,
                        }
                    errors.append("Phase A status workbook has no completed row")
                else:
                    errors.append("Phase A status workbook is empty")
                del status_frame
            except (OSError, ValueError, TypeError, KeyError) as error:
                errors.append(
                    f"cannot read {self.phase_a_status_file.name}: {error}"
                )
        else:
            errors.append(f"{self.phase_a_status_file.name} is missing")

        self._phase_a_snapshot_error = "; ".join(errors)
        return None

    def _ranking_decision_datetime(self):
        try:
            frame = pd.read_excel(
                self.ranking_file,
                sheet_name="Summary",
                usecols=["Metric", "Value"],
                engine="openpyxl",
            )
            values = dict(zip(frame["Metric"].astype(str), frame["Value"]))
            return pd.to_datetime(values.get("Decision DateTime"), errors="coerce")
        except (OSError, ValueError, KeyError):
            return pd.NaT

    def _selected_ranking_count(self):
        """Return selected rows without requiring Signal Engine to accept zero."""
        try:
            selected = pd.read_excel(
                self.ranking_file,
                sheet_name="Selected_Shares",
                usecols=["Latest_Selected"],
                engine="openpyxl",
            )
        except ValueError:
            selected = pd.read_excel(
                self.ranking_file,
                sheet_name="All_Shares",
                usecols=["Latest_Selected"],
                engine="openpyxl",
            )
        if selected.empty:
            return 0
        values = selected["Latest_Selected"]
        if values.dtype == bool:
            return int(values.sum())
        return int(
            values.astype("string").str.strip().str.upper()
            .isin({"TRUE", "1", "YES"}).sum()
        )

    def _commit_snapshot(self, snapshot, ranking_token):
        self._last_ranking_token = ranking_token
        self._last_snapshot_id = snapshot["snapshot_id"]
        self.phase_a_cycle = snapshot["cycle"]
        self.phase_a_market_datetime = snapshot["market_datetime"]
        self.cycle_number = self.phase_a_cycle
        self.ranking_updates_processed += 1

    @staticmethod
    def _atomic_report(frame, path):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(
            prefix=f".{path.stem}_", suffix=".xlsx", dir=path.parent
        )
        os.close(descriptor)
        try:
            with pd.ExcelWriter(temporary, engine="openpyxl") as writer:
                frame.to_excel(writer, sheet_name="Phase_B", index=False)
                worksheet = writer.book["Phase_B"]
                worksheet.freeze_panes = "A2"
                worksheet.auto_filter.ref = worksheet.dimensions
            os.replace(temporary, path)
        except Exception:
            if os.path.exists(temporary):
                os.unlink(temporary)
            raise

    def _create_strategy(self):
        from engines.FiveMinuteResidualStatisticalStrategy import (
            FiveMinuteResidualStatisticalStrategy,
        )

        return FiveMinuteResidualStatisticalStrategy(
            trading_ledger_file=str(self.trading_ledger_file),
            mathematical_score_file=str(self.mathematical_score_file),
            data_folder=str(self.synthetic_data_folder),
            output_file=str(self.strategy_file),
            manual_ledger_file=str(self.manual_ledger_file),
            position_journal_file=str(self.position_journal_file),
            approved_sheet="Approved_Trades",
            initial_capital=self.initial_capital,
            maximum_open_positions=self.maximum_open_positions,
            slippage_rate=self.slippage_rate,
            brokerage_rate=self.brokerage_rate,
            brokerage_cap_per_order=self.brokerage_cap_per_order,
            stt_sell_rate=self.stt_sell_rate,
            nse_transaction_charge_rate=(
                self.nse_transaction_charge_rate
            ),
            gst_rate=self.gst_rate,
            sebi_turnover_charge_rate=self.sebi_turnover_charge_rate,
            stamp_duty_buy_rate=self.stamp_duty_buy_rate,
            enable_profit_protection=self.enable_profit_protection,
            break_even_trigger_r=self.break_even_trigger_r,
            break_even_cost_buffer_multiplier=(
                self.break_even_cost_buffer_multiplier
            ),
            trailing_trigger_r=self.trailing_trigger_r,
            trailing_lock_r=self.trailing_lock_r,
            trailing_distance_r=self.trailing_distance_r,
            force_exit_at_session_end=self.force_exit_at_session_end,
            session_exit_time=self.session_exit_time,
            # Ranking and risk approval control new entries, not the lifecycle
            # of a position that is already open.
            exit_when_risk_approval_removed=False,
            entry_confirmation_cycles=self.entry_confirmation_cycles,
            maximum_entry_rank=self.maximum_entry_rank,
            # Use one age limit for signal validation, risk approval and
            # execution. This prevents Risk from approving a setup that the
            # strategy immediately rejects as stale.
            maximum_entry_age_minutes=self.maximum_signal_age_minutes,
            # A mean-reversion entry is emitted after an armed residual moves
            # inward. Execution must therefore validate the crossback band,
            # not demand the original 2.50 arming extreme again.
            minimum_entry_abs_zscore=(
                self.mean_reversion_minimum_remaining_abs_zscore
            ),
            maximum_entry_abs_zscore=(
                self.mean_reversion_entry_abs_zscore
            ),
            minimum_residual_quality_score=50.0,
            maximum_entry_deviation_atr=0.20,
            reversal_confirmation_cycles=self.reversal_confirmation_cycles,
            reentry_cooldown_candles=self.reentry_cooldown_candles,
            stop_loss_cooldown_candles=self.stop_loss_cooldown_candles,
            mean_reversion_normalization_zscore=(
                self.mean_reversion_normalization_zscore
            ),
            require_profitable_residual_normalization=True,
            minimum_normalization_net_pnl=0.0,
            exit_on_residual_divergence=False,
            mean_reversion_maximum_holding_candles=(
                self.mean_reversion_maximum_holding_candles
            ),
            momentum_maximum_holding_candles=(
                self.momentum_maximum_holding_candles
            ),
            residual_adverse_expansion=0.75,
            maximum_consecutive_losses=3,
            daily_loss_limit_fraction=0.01,
            conservative_intrabar_order=True,
            market_tail_rows=20,
            maximum_closed_trades=5000,
            maximum_events=10000,
            memory_optimized=True,
            retain_results_in_memory=False,
            save_excel=True,
            print_details=False,
        )

    def _create_signal_engine(self):
        from engines.FiveMinuteMathematicalSignalEngine import (
            FiveMinuteMathematicalSignalEngine,
        )

        return FiveMinuteMathematicalSignalEngine(
            ranking_file=str(self.ranking_file),
            mathematical_score_file=str(self.mathematical_score_file),
            output_file=str(self.signal_file),
            signal_state_file=str(self.signal_state_file),
            maximum_selected_symbols=self.maximum_selected_symbols,
            momentum_minimum_abs_zscore=1.00,
            mean_reversion_minimum_abs_zscore=2.50,
            minimum_abs_impulse=0.0,
            allow_momentum_entries=False,
            allow_mean_reversion_entries=True,
            require_mean_reversion_turn=True,
            require_previous_zscore_turn=True,
            mean_reversion_arm_abs_zscore=(
                self.mean_reversion_arm_abs_zscore
            ),
            mean_reversion_entry_abs_zscore=(
                self.mean_reversion_entry_abs_zscore
            ),
            mean_reversion_minimum_remaining_abs_zscore=(
                self.mean_reversion_minimum_remaining_abs_zscore
            ),
            minimum_reversion_confirmations=(
                self.minimum_reversion_confirmations
            ),
            maximum_armed_age_candles=self.maximum_armed_age_candles,
            mathematical_strength_weight=0.70,
            cross_sectional_strength_weight=0.30,
            score_match_tolerance=0.05,
            allow_ranking_snapshot_fallback=True,
            memory_optimized=True,
            save_excel=True,
            print_signal_details=False,
        )

    def _create_risk_engine(self):
        from engines.FiveMinuteRiskEngine import FiveMinuteRiskEngine

        return FiveMinuteRiskEngine(
            signals_file=str(self.signal_file),
            data_folder=str(self.synthetic_data_folder),
            output_file=str(self.trading_ledger_file),
            signals_sheet="Selected_Signals",
            initial_capital=self.initial_capital,
            risk_per_trade=self.risk_per_trade,
            maximum_allocation_per_trade=self.maximum_allocation_per_trade,
            maximum_portfolio_exposure=self.maximum_portfolio_exposure,
            maximum_open_positions=self.maximum_open_positions,
            maximum_entry_rank=self.maximum_entry_rank,
            # Phase A advances five simulated minutes every real second. Its
            # calculation may therefore make a snapshot tens of simulated
            # minutes old before Phase B receives it.
            maximum_signal_age_minutes=self.maximum_signal_age_minutes,
            atr_window=14,
            atr_history_rows=80,
            mean_reversion_atr_multiplier=1.80,
            mean_reversion_reward_risk=1.75,
            momentum_atr_multiplier=1.20,
            momentum_reward_risk=2.00,
            minimum_stop_percent=0.003,
            maximum_stop_percent=0.03,
            minimum_price=1.0,
            # Approximation used only for the pre-trade cost/profit gate. The
            # execution strategy calculates each equity-intraday charge exactly.
            # Approximate all statutory equity-intraday charges as an
            # equivalent per-side rate for Risk Engine screening. Exact
            # component charges are calculated when the position closes.
            estimated_transaction_cost_rate=0.0005315,
            estimated_slippage_rate=self.slippage_rate,
            minimum_expected_profit_cost_multiple=4.0,
            minimum_expected_net_reward_risk=(
                self.minimum_expected_net_reward_risk
            ),
            benchmark_filename="NIFTY%2050_5mins.txt",
            enable_benchmark_trend_filter=(
                self.enable_benchmark_trend_filter
            ),
            benchmark_trend_candles=self.benchmark_trend_candles,
            maximum_adverse_benchmark_return=(
                self.maximum_adverse_benchmark_return
            ),
            allow_momentum_entries=False,
            maximum_ledger_rows=5000,
            memory_optimized=True,
            retain_results_in_memory=False,
            save_excel=True,
            print_details=False,
        )

    def _validate_phase_a_files(self):
        missing = [
            str(path) for path in [
                self.ranking_file,
                self.mathematical_score_file,
            ] if not path.is_file()
        ]
        if missing:
            raise FileNotFoundError(
                "Phase A output files are unavailable: " + ", ".join(missing)
            )
        if not self.synthetic_data_folder.is_dir():
            raise FileNotFoundError(
                f"Synthetic data folder not found: {self.synthetic_data_folder}"
            )

    def _process_new_ranking(self):
        self._validate_phase_a_files()
        snapshot = self._read_phase_a_snapshot()
        if snapshot is None:
            self._latest_approved_plans = []
            raise RuntimeError(
                "Phase A completed snapshot is unavailable: "
                f"{self._phase_a_snapshot_error}"
            )
        if snapshot["snapshot_id"] == self._last_snapshot_id:
            self._latest_approved_plans = []
            return False, 0, 0, 0, 0, 0
        token = self._file_token(self.ranking_file)
        if token is None or token == self._last_ranking_token:
            self._latest_approved_plans = []
            return False, 0, 0, 0, 0, 0
        score_token = self._file_token(self.mathematical_score_file)
        if snapshot.get("ranking_token") is not None \
                and token != snapshot["ranking_token"]:
            self._latest_approved_plans = []
            return False, 0, 0, 0, 0, 0
        if snapshot.get("score_token") is not None \
                and score_token != snapshot["score_token"]:
            self._latest_approved_plans = []
            return False, 0, 0, 0, 0, 0

        # A completed Phase A cycle with no selected setup is still a valid
        # cycle. Commit it and report zero signals instead of remaining at 0.
        if self._selected_ranking_count() == 0:
            confirmed_snapshot = self._read_phase_a_snapshot()
            if confirmed_snapshot is None \
                    or confirmed_snapshot["snapshot_id"] != snapshot["snapshot_id"]:
                return False, 0, 0, 0, 0, 0
            self._latest_approved_plans = []
            self._commit_snapshot(snapshot, token)
            return True, 0, 0, 0, 0, 0

        signal_engine = self._create_signal_engine()
        risk_engine = self._create_risk_engine()
        output = io.StringIO()
        stream = output if self.quiet_engines else None
        context = redirect_stdout(stream) if stream is not None else _NullContext()
        try:
            with context:
                signal_data, signal_report = signal_engine.calculate_all()
                risk_data, risk_report = risk_engine.calculate_all()
            signal_count = len(signal_report)
            signal_values = (
                signal_report["Mathematical_Signal"]
                .astype("string")
                .str.strip()
                .str.upper()
            ) if not signal_report.empty else pd.Series(dtype="string")
            buy_count = int(signal_values.eq("BUY").sum())
            sell_count = int(
                signal_values.isin({"SELL", "SELL_SHORT"}).sum()
            )
            no_trade_count = int(signal_count - buy_count - sell_count)
            approved_count = int(
                risk_report["Trade_Approved"].fillna(False).sum()
            ) if not risk_report.empty else 0
            if approved_count:
                approved = risk_report.loc[
                    risk_report["Trade_Approved"].fillna(False)
                ]
                self._latest_approved_plans = []
                for row in approved.itertuples(index=False):
                    action = str(row.Trade_Action)
                    decision_id = str(row.Decision_ID)
                    self._latest_approved_plans.append({
                        "Trade_ID": f"{decision_id}_{action}",
                        "Symbol": str(row.Symbol),
                        "Signal": action,
                        "Entry_Price": float(row.Entry_Price),
                        "Stop_Loss": float(row.Stop_Loss),
                        "Target_Price": float(row.Target_Price),
                        "Quantity": int(row.Quantity),
                    })
                del approved
            else:
                self._latest_approved_plans = []

            for required in [self.signal_file, self.trading_ledger_file]:
                if not required.is_file() or required.stat().st_size == 0:
                    raise RuntimeError(f"Phase B output verification failed: {required}")

            # Phase A may complete another cycle while Signal/Risk are being
            # calculated. Never commit a mixture of two snapshots.
            confirmed_snapshot = self._read_phase_a_snapshot()
            if (
                confirmed_snapshot is None
                or confirmed_snapshot["snapshot_id"] != snapshot["snapshot_id"]
            ):
                self._latest_approved_plans = []
                return False, 0, 0, 0, 0, 0

            # Commit the token only after both downstream workbooks succeed.
            self._commit_snapshot(snapshot, token)
            del signal_data, signal_report, risk_data, risk_report
            del signal_engine, risk_engine
            return (
                True,
                signal_count,
                buy_count,
                sell_count,
                no_trade_count,
                approved_count,
            )
        finally:
            output.close()

    def _print_approved_plans(self):
        for plan in self._latest_approved_plans:
            plan_status = (
                "READY_FOR_ENTRY"
                if self.entry_confirmation_cycles == 1
                else (
                    "WAITING_FOR_"
                    f"{self.entry_confirmation_cycles}_CYCLE_CONFIRMATION"
                )
            )
            print(
                f"PLAN  | TradeID={plan['Trade_ID']} | "
                f"{plan['Symbol']:<12} | Signal={plan['Signal']:<10} | "
                f"Entry={plan['Entry_Price']:.4f} | "
                f"StopLoss={plan['Stop_Loss']:.4f} | "
                f"Target={plan['Target_Price']:.4f} | "
                f"Qty={plan['Quantity']} | Status={plan_status}"
            )

    def _print_new_events(
        self,
        previous_open_ids,
        previous_closed_count,
        previous_event_count,
    ):
        current_open = self.strategy.open_positions
        if not current_open.empty:
            new_open = current_open.loc[
                ~current_open["Position_ID"].astype(str).isin(previous_open_ids)
            ]
            for row in new_open.itertuples(index=False):
                print(
                    f"ENTRY | TradeID={row.Position_ID} | "
                    f"{row.Symbol:<12} | {row.Entry_Action:<10} | "
                    f"Price={float(row.Entry_Price):.4f} | "
                    f"Stop={float(row.Stop_Loss):.4f} | "
                    f"Target={float(row.Target_Price):.4f} | Qty={int(row.Quantity)}"
                )

        closed = self.strategy.closed_trades
        if len(closed) > previous_closed_count:
            new_closed = closed.iloc[previous_closed_count:]
            for row in new_closed.itertuples(index=False):
                print(
                    f"EXIT  | TradeID={row.Position_ID} | "
                    f"{row.Symbol:<12} | Exit={float(row.Exit_Price):.4f} | "
                    f"StopLoss={float(row.Stop_Loss):.4f} | "
                    f"Target={float(row.Target_Price):.4f} | "
                    f"Reason={row.Exit_Reason} | "
                    f"Brokerage="
                    f"{float(row.Entry_Brokerage + row.Exit_Brokerage):.2f} | "
                    f"STT={float(row.STT):.2f} | "
                    f"GST={float(row.GST):.2f} | "
                    f"OtherCharges="
                    f"{float(row.Exchange_Transaction_Charges + row.SEBI_Charges + row.Stamp_Duty):.2f} | "
                    f"Slippage={float(row.Slippage_Cost):.2f} | "
                    f"TotalCost={float(row.Total_Trading_Cost):.2f} | "
                    f"MFE={float(row.Maximum_Favorable_Excursion):.2f} | "
                    f"BreakEven={bool(row.BreakEven_Activated)} | "
                    f"Trailing={bool(row.Trailing_Stop_Activated)} | "
                    f"TradePnL={float(row.Net_PnL):.2f} | "
                    f"TotalCapital={float(row.Equity_After):.2f}"
                )

        events = self.strategy.events
        if len(events) > previous_event_count:
            new_events = events.iloc[previous_event_count:]
            rejected = new_events.loc[
                new_events["Event"].isin(["ENTRY_SKIPPED", "ENTRY_ERROR"])
            ]
            for row in rejected.itertuples(index=False):
                label = "CANCEL" if row.Event == "ENTRY_SKIPPED" else "ERROR "
                price = (
                    f"{float(row.Price):.4f}"
                    if pd.notna(row.Price)
                    else "N/A"
                )
                print(
                    f"{label} | TradeID={row.Position_ID or '-'} | "
                    f"{row.Symbol:<12} | Price={price} | Reason={row.Reason}"
                )

    def _print_open_positions(self):
        if not self.display_open_positions_each_cycle \
                or self.strategy.open_positions.empty:
            return
        for row in self.strategy.open_positions.sort_values(
            ["Rank", "Symbol"]
        ).itertuples(index=False):
            print(
                f"OPEN  | TradeID={row.Position_ID} | "
                f"{row.Symbol:<12} | {row.Entry_Action:<10} | "
                f"Entry={float(row.Entry_Price):.4f} | "
                f"StopLoss={float(row.Stop_Loss):.4f} | "
                f"Target={float(row.Target_Price):.4f} | "
                f"Current={float(row.Current_Price):.4f} | "
                f"GrossPnL={float(row.Unrealized_PnL):.2f} | "
                f"EstCost={float(row.Estimated_Exit_Charges):.2f} | "
                f"EstNetPnL={float(row.Estimated_Net_PnL):.2f} | "
                f"MFE={float(row.Maximum_Favorable_Excursion):.2f} | "
                f"BreakEven={bool(row.BreakEven_Activated)} | "
                f"Trailing={bool(row.Trailing_Stop_Activated)} | "
                f"StopMode={row.Stop_Adjustment_Reason} | "
                f"Candles={int(row.Candles_Held)}"
            )

    def run_one_cycle(self):
        if not self._cycle_lock.acquire(blocking=False):
            raise RuntimeError("Phase B cycle is already running")
        started = time.perf_counter()
        try:
            self.monitor_cycle_number += 1
            previous_open_ids = set(
                self.strategy.open_positions.get(
                    "Position_ID", pd.Series(dtype="string")
                ).dropna().astype(str)
            )
            previous_closed_count = len(self.strategy.closed_trades)
            previous_event_count = len(self.strategy.events)

            ranking_updated = False
            signal_count = 0
            buy_count = 0
            sell_count = 0
            no_trade_count = 0
            approved_count = 0
            update_error = None
            try:
                (
                    ranking_updated,
                    signal_count,
                    buy_count,
                    sell_count,
                    no_trade_count,
                    approved_count,
                ) = self._process_new_ranking()
            except Exception as error:
                update_error = f"{type(error).__name__}: {error}"

            strategy_data = {}
            strategy_report = None
            if self.trading_ledger_file.is_file():
                output = io.StringIO()
                stream = output if self.quiet_engines else None
                context = (
                    redirect_stdout(stream) if stream is not None else _NullContext()
                )
                try:
                    with context:
                        strategy_data, strategy_report = self.strategy.run_cycle()
                finally:
                    output.close()

            if ranking_updated:
                self._print_approved_plans()
            self._print_new_events(
                previous_open_ids,
                previous_closed_count,
                previous_event_count,
            )
            self._print_open_positions()
            open_count = len(self.strategy.open_positions)
            closed_count = len(self.strategy.closed_trades)
            realized_pnl = float(self.strategy.realized_pnl)
            runtime = time.perf_counter() - started
            status = "COMPLETED" if update_error is None else "MONITOR_ONLY"
            result = {
                "Cycle": self.cycle_number,
                "Phase_A_Cycle": self.phase_a_cycle,
                "Phase_A_Market_DateTime": self.phase_a_market_datetime,
                "Monitor_Cycle": self.monitor_cycle_number,
                "Ranking_Updated": ranking_updated,
                "Ranking_Updates_Processed": self.ranking_updates_processed,
                "Signals": signal_count,
                "Buy_Signals": buy_count,
                "Sell_Signals": sell_count,
                "No_Trade_Signals": no_trade_count,
                "Actionable_Signals": buy_count + sell_count,
                "Approved_Plans": approved_count,
                "Open_Positions": open_count,
                "Closed_Trades": closed_count,
                "Realized_PnL": realized_pnl,
                "Runtime_Seconds": runtime,
                "Status": status,
                "Update_Error": update_error or "",
                "Signal_File": str(self.signal_file),
                "Signal_State_File": str(self.signal_state_file),
                "Maximum_Entry_Rank": self.maximum_entry_rank,
                "Maximum_Entry_Age_Minutes": self.maximum_signal_age_minutes,
                "Trading_Ledger_File": str(self.trading_ledger_file),
                "Manual_Execution_Ledger_File": str(self.manual_ledger_file),
                "Position_Recovery_Journal": str(self.position_journal_file),
                "Strategy_File": str(self.strategy_file),
            }
            self.last_result = result
            self.last_error = update_error
            self._atomic_report(pd.DataFrame([result]), self.status_file)

            del strategy_data, strategy_report
            if self.garbage_collect_each_cycle:
                gc.collect()
            if update_error:
                print(
                    "PHASE B UPDATE ERROR | "
                    f"{update_error}"
                )
            if ranking_updated or open_count or update_error:
                print(
                    f"Phase B cycle {self.cycle_number} "
                    f"| Market={self.phase_a_market_datetime} | "
                    f"RankingUpdated={ranking_updated} | "
                    f"Signals={signal_count} "
                    f"[BUY={buy_count} | SELL={sell_count} | "
                    f"NO_TRADE={no_trade_count}] | "
                    f"Approved={approved_count} | Open={open_count} | "
                    f"Closed={closed_count} | PnL={realized_pnl:.2f} | "
                    f"Runtime={runtime:.2f}s"
                )
                print()
            return result
        finally:
            self._cycle_lock.release()

    def run_forever(self):
        self._stop_event.clear()
        print("=" * 100)
        print("FMRSS FORWARD TEST : PHASE B")
        print(f"Ranking file       : {self.ranking_file}")
        print(f"Phase A state      : {self.phase_a_state_file}")
        print(f"Manual ledger      : {self.manual_ledger_file}")
        print(f"Recovery journal   : {self.position_journal_file}")
        print(f"Signal state       : {self.signal_state_file}")
        print(f"Polling interval   : {self.poll_interval_seconds:.2f} second(s)")
        print("Execution mode     : PAPER ONLY")
        print("Stop command       : Ctrl+C")
        print("=" * 100)
        try:
            while not self._stop_event.is_set():
                started = time.monotonic()
                try:
                    self.run_one_cycle()
                except Exception as error:
                    self.last_error = f"{type(error).__name__}: {error}"
                    print(f"Phase B cycle failed: {self.last_error}")
                remaining = max(
                    0.0,
                    self.poll_interval_seconds - (time.monotonic() - started),
                )
                self._stop_event.wait(remaining)
        except KeyboardInterrupt:
            print("\nStopping Phase B safely...")
        finally:
            self._stop_event.set()
            print(f"Completed cycles          : {self.cycle_number}")
            print(f"Monitor polls             : {self.monitor_cycle_number}")
            print(f"Ranking updates processed : {self.ranking_updates_processed}")
            print(f"Open positions            : {len(self.strategy.open_positions)}")
            print(f"Closed trades             : {len(self.strategy.closed_trades)}")
            print(f"Realized PnL              : {self.strategy.realized_pnl:.2f}")

    def _background_loop(self):
        self.run_forever()

    def start(self):
        if self.is_running:
            raise RuntimeError("Phase B is already running")
        self._stop_event.clear()
        self._thread = threading.Thread(
            target=self._background_loop,
            name="FMRSS-Forward-Test-Phase-B",
            daemon=True,
        )
        self._thread.start()
        return self._thread

    def stop(self, timeout=60.0):
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=timeout)
        return not self.is_running

    @property
    def is_running(self):
        return bool(self._thread and self._thread.is_alive())


class _NullContext:
    def __enter__(self):
        return None

    def __exit__(self, exc_type, exc_value, traceback):
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Run FMRSS synthetic forward-test Phase B."
    )
    parser.add_argument(
        "--tests-folder", default="/home/devinderjeet/fmrss/tests"
    )
    parser.add_argument("--poll-seconds", type=float, default=1.0)
    parser.add_argument(
        "--market-data-folder", default=None,
        help=("Price folder used for entries/exits. Omit for Phase A's "
              "synthetic_market_data folder."),
    )
    parser.add_argument("--initial-capital", type=float, default=100000.0)
    parser.add_argument("--maximum-open-positions", type=int, default=2)
    parser.add_argument(
        "--maximum-entry-rank",
        type=int,
        default=5,
        help=(
            "Highest selected rank eligible for entry. This does not change "
            "the maximum number of simultaneously open positions."
        ),
    )
    parser.add_argument("--maximum-signal-age-minutes", type=float, default=5.0)
    parser.add_argument(
        "--signal-state-file", default=None,
        help=(
            "Persistent armed-signal workbook. Defaults outside tests-folder "
            "to <tests-folder-parent>/state/<tests-folder-name>_SignalState.xlsx."
        ),
    )
    parser.add_argument(
        "--mean-reversion-arm-zscore", type=float, default=2.50,
    )
    parser.add_argument(
        "--mean-reversion-entry-zscore", type=float, default=2.20,
    )
    parser.add_argument(
        "--mean-reversion-minimum-zscore", type=float, default=1.50,
    )
    parser.add_argument(
        "--minimum-reversion-confirmations", type=int, default=2,
        help="Required confirmations out of Z-score, residual return and price turn.",
    )
    parser.add_argument(
        "--maximum-armed-age-candles", type=int, default=12,
    )
    parser.add_argument(
        "--minimum-net-reward-risk", type=float, default=1.25,
        help="Minimum expected reward/risk after estimated round-trip costs.",
    )
    parser.add_argument(
        "--disable-nifty-trend-filter", action="store_true",
        help="Disable the adverse NIFTY direction entry filter.",
    )
    parser.add_argument(
        "--nifty-trend-candles", type=int, default=6,
        help="Completed NIFTY candles used for the directional filter.",
    )
    parser.add_argument(
        "--maximum-adverse-nifty-return", type=float, default=0.002,
        help="Block counter-trend entries beyond this absolute NIFTY return.",
    )
    parser.add_argument(
        "--session-exit-time",
        default="15:14",
        help=(
            "Intraday exit cutoff in HH:MM. With five-minute files, the "
            "latest available completed candle price is used."
        ),
    )
    parser.add_argument(
        "--manual-ledger-file",
        default=None,
        help=(
            "Editable execution ledger. Defaults to "
            "<tests-folder>/TradingLedge.xlsx."
        ),
    )
    parser.add_argument(
        "--position-journal-file",
        default=None,
        help=(
            "Persistent recovery workbook stored outside tests-folder. "
            "Defaults to <tests-folder-parent>/state/"
            "<tests-folder-name>_OpenPositionJournal.xlsx."
        ),
    )
    parser.add_argument("--show-engine-output", action="store_true")
    arguments = parser.parse_args()

    phase_b = FiveMinuteForwardTestPhaseB(
        tests_folder=arguments.tests_folder,
        synthetic_data_folder=arguments.market_data_folder,
        poll_interval_seconds=arguments.poll_seconds,
        initial_capital=arguments.initial_capital,
        maximum_open_positions=arguments.maximum_open_positions,
        maximum_entry_rank=arguments.maximum_entry_rank,
        maximum_signal_age_minutes=arguments.maximum_signal_age_minutes,
        signal_state_file=arguments.signal_state_file,
        mean_reversion_arm_abs_zscore=(
            arguments.mean_reversion_arm_zscore
        ),
        mean_reversion_entry_abs_zscore=(
            arguments.mean_reversion_entry_zscore
        ),
        mean_reversion_minimum_remaining_abs_zscore=(
            arguments.mean_reversion_minimum_zscore
        ),
        minimum_reversion_confirmations=(
            arguments.minimum_reversion_confirmations
        ),
        maximum_armed_age_candles=arguments.maximum_armed_age_candles,
        minimum_expected_net_reward_risk=arguments.minimum_net_reward_risk,
        enable_benchmark_trend_filter=(
            not arguments.disable_nifty_trend_filter
        ),
        benchmark_trend_candles=arguments.nifty_trend_candles,
        maximum_adverse_benchmark_return=(
            arguments.maximum_adverse_nifty_return
        ),
        session_exit_time=arguments.session_exit_time,
        manual_ledger_file=arguments.manual_ledger_file,
        position_journal_file=arguments.position_journal_file,
        quiet_engines=not arguments.show_engine_output,
    )
    phase_b.run_forever()


if __name__ == "__main__":
    main()
