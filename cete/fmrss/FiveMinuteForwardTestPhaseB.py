import argparse
import gc
import io
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
        trading_ledger_file=None,
        strategy_file=None,
        status_file=None,
        poll_interval_seconds=1.0,
        maximum_selected_symbols=5,
        initial_capital=100000.0,
        risk_per_trade=0.0025,
        maximum_allocation_per_trade=0.20,
        maximum_portfolio_exposure=0.80,
        maximum_open_positions=2,
        maximum_signal_age_minutes=180.0,
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
        session_exit_time="15:15",
        entry_confirmation_cycles=1,
        reversal_confirmation_cycles=2,
        reentry_cooldown_candles=6,
        stop_loss_cooldown_candles=12,
        mean_reversion_normalization_zscore=0.50,
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
        self.trading_ledger_file = Path(
            trading_ledger_file or folder / "Test_TradingLedger.xlsx"
        ).expanduser().resolve()
        self.strategy_file = Path(
            strategy_file
            or folder / "Test_ResidualStatisticalStrategy.xlsx"
        ).expanduser().resolve()
        self.status_file = Path(
            status_file or folder / "Test_PhaseB_Status.xlsx"
        ).expanduser().resolve()

        self.poll_interval_seconds = float(poll_interval_seconds)
        self.maximum_selected_symbols = int(maximum_selected_symbols)
        self.initial_capital = float(initial_capital)
        self.risk_per_trade = float(risk_per_trade)
        self.maximum_allocation_per_trade = float(
            maximum_allocation_per_trade
        )
        self.maximum_portfolio_exposure = float(maximum_portfolio_exposure)
        self.maximum_open_positions = int(maximum_open_positions)
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
        self.ranking_updates_processed = 0
        self.last_error = None
        self.last_result = None
        self._latest_approved_plans = []
        self._last_ranking_token = None
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
            reversal_confirmation_cycles=self.reversal_confirmation_cycles,
            reentry_cooldown_candles=self.reentry_cooldown_candles,
            stop_loss_cooldown_candles=self.stop_loss_cooldown_candles,
            mean_reversion_normalization_zscore=(
                self.mean_reversion_normalization_zscore
            ),
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
            maximum_selected_symbols=self.maximum_selected_symbols,
            momentum_minimum_abs_zscore=1.00,
            mean_reversion_minimum_abs_zscore=2.25,
            minimum_abs_impulse=0.0,
            allow_momentum_entries=False,
            allow_mean_reversion_entries=True,
            require_mean_reversion_turn=True,
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
            minimum_expected_profit_cost_multiple=3.0,
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
        token = self._file_token(self.ranking_file)
        if token is None or token == self._last_ranking_token:
            self._latest_approved_plans = []
            return False, 0, 0, 0, 0, 0

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

            # Commit the token only after both downstream workbooks succeed.
            self._last_ranking_token = token
            self.ranking_updates_processed += 1
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

    def _print_new_events(self, previous_open_ids, previous_closed_count):
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
            self.cycle_number += 1
            previous_open_ids = set(
                self.strategy.open_positions.get(
                    "Position_ID", pd.Series(dtype="string")
                ).dropna().astype(str)
            )
            previous_closed_count = len(self.strategy.closed_trades)

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
            self._print_new_events(previous_open_ids, previous_closed_count)
            self._print_open_positions()
            open_count = len(self.strategy.open_positions)
            closed_count = len(self.strategy.closed_trades)
            realized_pnl = float(self.strategy.realized_pnl)
            runtime = time.perf_counter() - started
            status = "COMPLETED" if update_error is None else "MONITOR_ONLY"
            result = {
                "Cycle": self.cycle_number,
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
                "Trading_Ledger_File": str(self.trading_ledger_file),
                "Strategy_File": str(self.strategy_file),
            }
            self.last_result = result
            self.last_error = update_error
            self._atomic_report(pd.DataFrame([result]), self.status_file)

            del strategy_data, strategy_report
            if self.garbage_collect_each_cycle:
                gc.collect()
            if ranking_updated or open_count or update_error:
                print(
                    f"Phase B cycle {self.cycle_number} | "
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
    parser.add_argument("--initial-capital", type=float, default=100000.0)
    parser.add_argument("--maximum-open-positions", type=int, default=2)
    parser.add_argument("--maximum-signal-age-minutes", type=float, default=180.0)
    parser.add_argument("--show-engine-output", action="store_true")
    arguments = parser.parse_args()

    phase_b = FiveMinuteForwardTestPhaseB(
        tests_folder=arguments.tests_folder,
        poll_interval_seconds=arguments.poll_seconds,
        initial_capital=arguments.initial_capital,
        maximum_open_positions=arguments.maximum_open_positions,
        maximum_signal_age_minutes=arguments.maximum_signal_age_minutes,
        quiet_engines=not arguments.show_engine_output,
    )
    phase_b.run_forever()


if __name__ == "__main__":
    main()
