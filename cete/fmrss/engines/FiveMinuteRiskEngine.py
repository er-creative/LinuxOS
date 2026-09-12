import io
import os
import tempfile
from collections import deque
from pathlib import Path

import numpy as np
import pandas as pd


class FiveMinuteRiskEngine:
    """Convert selected mathematical signals into bounded-risk trade plans.

    This class does not place orders. It writes approved and rejected risk
    decisions to TradingLedger.xlsx for the execution/strategy layer.
    """

    SIGNAL_COLUMNS = [
        "Symbol",
        "Decision_DateTime",
        "Ranking_Source_DateTime",
        "Rank",
        "Regime",
        "Residual_ZScore",
        "Mathematical_Score",
        "Cross_Sectional_Score",
        "Signal_Eligible",
        "Mathematical_Signal",
        "Signal_Strength",
        "Signal_Reason",
    ]

    MARKET_COLUMNS = [
        "Symbol", "Date", "Time", "Open", "High", "Low", "Close", "Volume"
    ]

    DECISION_COLUMNS = [
        "Decision_ID",
        "Symbol",
        "Decision_DateTime",
        "Signal_Source_DateTime",
        "Market_DateTime",
        "Signal_Age_Minutes",
        "Rank",
        "Regime",
        "Original_Signal",
        "Trade_Action",
        "Signal_Strength",
        "Entry_Price",
        "ATR_14",
        "Stop_Distance",
        "Stop_Loss",
        "Target_Price",
        "Reward_Risk_Ratio",
        "Quantity",
        "Planned_Notional",
        "Maximum_Loss",
        "Potential_Profit",
        "Estimated_Round_Trip_Cost",
        "Expected_Profit_Cost_Multiple",
        "Expected_Net_Profit",
        "Expected_Net_Reward_Risk",
        "Benchmark_Return",
        "Benchmark_Trend",
        "Portfolio_Exposure_After",
        "Trade_Approved",
        "Risk_Decision_Reason",
        "Status",
        "Calculation_Backend",
    ]

    def __init__(
        self,
        signals_file=(
            "/home/devinderjeet/fmrss/report/MathematicalSignals.xlsx"
        ),
        data_folder="/home/hadoop/shareMarket_Data",
        output_file="/home/devinderjeet/fmrss/report/TradingLedger.xlsx",
        signals_sheet="Selected_Signals",
        initial_capital=100000.0,
        risk_per_trade=0.0025,
        maximum_allocation_per_trade=0.20,
        maximum_portfolio_exposure=0.80,
        maximum_open_positions=2,
        maximum_entry_rank=2,
        maximum_signal_age_minutes=5.0,
        atr_window=14,
        atr_history_rows=80,
        mean_reversion_atr_multiplier=1.80,
        mean_reversion_reward_risk=1.75,
        momentum_atr_multiplier=1.20,
        momentum_reward_risk=2.00,
        minimum_stop_percent=0.003,
        maximum_stop_percent=0.03,
        minimum_price=1.0,
        estimated_transaction_cost_rate=0.0003,
        estimated_slippage_rate=0.0001,
        minimum_expected_profit_cost_multiple=4.0,
        minimum_expected_net_reward_risk=1.25,
        benchmark_filename="NIFTY%2050_5mins.txt",
        enable_benchmark_trend_filter=True,
        benchmark_trend_candles=6,
        maximum_adverse_benchmark_return=0.002,
        allow_momentum_entries=False,
        maximum_ledger_rows=5000,
        memory_optimized=True,
        retain_results_in_memory=False,
        save_excel=True,
        print_details=False,
    ):
        self._positive("initial_capital", initial_capital)
        self._fraction("risk_per_trade", risk_per_trade, upper=0.05)
        self._fraction(
            "maximum_allocation_per_trade", maximum_allocation_per_trade
        )
        self._fraction("maximum_portfolio_exposure", maximum_portfolio_exposure)
        self._positive("maximum_signal_age_minutes", maximum_signal_age_minutes,
                       allow_zero=True)
        self._positive("minimum_stop_percent", minimum_stop_percent)
        self._positive("maximum_stop_percent", maximum_stop_percent)
        self._positive(
            "minimum_expected_profit_cost_multiple",
            minimum_expected_profit_cost_multiple,
        )
        self._positive(
            "minimum_expected_net_reward_risk",
            minimum_expected_net_reward_risk,
        )
        self._positive(
            "maximum_adverse_benchmark_return",
            maximum_adverse_benchmark_return,
        )
        if minimum_stop_percent >= maximum_stop_percent:
            raise ValueError(
                "minimum_stop_percent must be less than maximum_stop_percent"
            )
        for name, value in {
            "maximum_open_positions": maximum_open_positions,
            "maximum_entry_rank": maximum_entry_rank,
            "atr_window": atr_window,
            "atr_history_rows": atr_history_rows,
            "maximum_ledger_rows": maximum_ledger_rows,
            "benchmark_trend_candles": benchmark_trend_candles,
        }.items():
            if not isinstance(value, (int, np.integer)) or value < 1:
                raise ValueError(f"{name} must be a positive integer")
        if atr_history_rows < atr_window + 1:
            raise ValueError("atr_history_rows must exceed atr_window")

        self.signals_file = str(signals_file)
        self.data_folder = str(data_folder)
        self.output_file = str(output_file)
        self.signals_sheet = str(signals_sheet)
        self.initial_capital = float(initial_capital)
        self.risk_per_trade = float(risk_per_trade)
        self.maximum_allocation_per_trade = float(maximum_allocation_per_trade)
        self.maximum_portfolio_exposure = float(maximum_portfolio_exposure)
        self.maximum_open_positions = int(maximum_open_positions)
        self.maximum_entry_rank = int(maximum_entry_rank)
        self.maximum_signal_age_minutes = float(maximum_signal_age_minutes)
        self.atr_window = int(atr_window)
        self.atr_history_rows = int(atr_history_rows)
        self.mean_reversion_atr_multiplier = float(
            mean_reversion_atr_multiplier
        )
        self.mean_reversion_reward_risk = float(
            mean_reversion_reward_risk
        )
        self.momentum_atr_multiplier = float(momentum_atr_multiplier)
        self.momentum_reward_risk = float(momentum_reward_risk)
        self.minimum_stop_percent = float(minimum_stop_percent)
        self.maximum_stop_percent = float(maximum_stop_percent)
        self.minimum_price = float(minimum_price)
        self.estimated_transaction_cost_rate = float(
            estimated_transaction_cost_rate
        )
        self.estimated_slippage_rate = float(estimated_slippage_rate)
        self.minimum_expected_profit_cost_multiple = float(
            minimum_expected_profit_cost_multiple
        )
        self.minimum_expected_net_reward_risk = float(
            minimum_expected_net_reward_risk
        )
        self.benchmark_filename = str(benchmark_filename)
        self.enable_benchmark_trend_filter = bool(
            enable_benchmark_trend_filter
        )
        self.benchmark_trend_candles = int(benchmark_trend_candles)
        self.maximum_adverse_benchmark_return = float(
            maximum_adverse_benchmark_return
        )
        self.allow_momentum_entries = bool(allow_momentum_entries)
        self.maximum_ledger_rows = int(maximum_ledger_rows)
        self.memory_optimized = bool(memory_optimized)
        self.retain_results_in_memory = bool(retain_results_in_memory)
        self.save_excel = bool(save_excel)
        self.print_details = bool(print_details)
        self.calculation_backend = "NUMPY_PANDAS_TAIL_ONLY"

    @staticmethod
    def _positive(name, value, allow_zero=False):
        valid = np.isfinite(value) and (value >= 0 if allow_zero else value > 0)
        if not valid:
            raise ValueError(f"{name} must be finite and positive")

    @staticmethod
    def _fraction(name, value, upper=1.0):
        if not np.isfinite(value) or not 0 < value <= upper:
            raise ValueError(f"{name} must be in (0, {upper}]")

    @staticmethod
    def _boolean(series):
        if pd.api.types.is_bool_dtype(series):
            return series.fillna(False)
        return series.astype("string").str.strip().str.upper().isin(
            {"TRUE", "1", "YES", "Y"}
        )

    def _load_signals(self):
        if not os.path.isfile(self.signals_file):
            raise FileNotFoundError(
                f"Mathematical signals workbook not found: {self.signals_file}"
            )
        frame = pd.read_excel(
            self.signals_file,
            sheet_name=self.signals_sheet,
            usecols=self.SIGNAL_COLUMNS,
            engine="openpyxl",
        )
        if frame.empty:
            return frame
        frame["Symbol"] = (
            frame["Symbol"].astype("string").str.strip().str.upper()
        )
        for column in ["Decision_DateTime", "Ranking_Source_DateTime"]:
            frame[column] = pd.to_datetime(frame[column], errors="coerce")
        for column in ["Regime", "Mathematical_Signal", "Signal_Reason"]:
            frame[column] = (
                frame[column].astype("string").str.strip().str.upper()
            )
        frame["Signal_Eligible"] = self._boolean(frame["Signal_Eligible"])
        for column in [
            "Rank", "Residual_ZScore", "Mathematical_Score",
            "Cross_Sectional_Score", "Signal_Strength",
        ]:
            frame[column] = pd.to_numeric(frame[column], errors="coerce")
            if self.memory_optimized:
                frame[column] = frame[column].astype("float32")
        frame.sort_values(["Rank", "Symbol"], inplace=True)
        frame.drop_duplicates("Symbol", keep="first", inplace=True)
        frame.reset_index(drop=True, inplace=True)
        return frame

    def _read_tail(self, symbol):
        path = os.path.join(self.data_folder, f"{symbol}_5mins.txt")
        if not os.path.isfile(path):
            raise FileNotFoundError(f"Market data file not found: {path}")
        # deque reads the file as a stream and retains only the required tail.
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            lines = deque(handle, maxlen=self.atr_history_rows + 2)
        if not lines:
            raise ValueError(f"Market data file is empty: {path}")
        frame = pd.read_csv(
            io.StringIO("".join(lines)),
            names=self.MARKET_COLUMNS,
            header=None,
        )
        frame["DateTime"] = pd.to_datetime(
            frame["Date"].astype(str).str.strip()
            + " "
            + frame["Time"].astype(str).str.strip(),
            errors="coerce",
            format="mixed",
        )
        for column in ["Open", "High", "Low", "Close", "Volume"]:
            frame[column] = pd.to_numeric(frame[column], errors="coerce")
        frame.dropna(
            subset=["DateTime", "High", "Low", "Close"], inplace=True
        )
        frame.sort_values("DateTime", inplace=True)
        frame.drop_duplicates("DateTime", keep="last", inplace=True)
        frame = frame.tail(self.atr_history_rows).copy()
        if len(frame) < self.atr_window + 1:
            raise ValueError(
                f"{symbol} has only {len(frame)} valid candles; "
                f"at least {self.atr_window + 1} are required"
            )
        return frame

    def _market_snapshot(self, symbol):
        frame = self._read_tail(symbol)
        previous_close = frame["Close"].shift(1)
        true_range = pd.concat(
            [
                frame["High"].sub(frame["Low"]).abs(),
                frame["High"].sub(previous_close).abs(),
                frame["Low"].sub(previous_close).abs(),
            ],
            axis=1,
        ).max(axis=1)
        atr = float(true_range.tail(self.atr_window).mean())
        result = {
            "Market_DateTime": pd.Timestamp(frame["DateTime"].iloc[-1]),
            "Entry_Price": float(frame["Close"].iloc[-1]),
            "ATR_14": atr,
        }
        del frame, previous_close, true_range
        return result

    def _benchmark_snapshot(self):
        """Read only the NIFTY tail required by the directional risk gate."""
        if not self.enable_benchmark_trend_filter:
            return {"Benchmark_Return": 0.0, "Benchmark_Trend": "DISABLED"}
        symbol = self.benchmark_filename
        suffix = "_5mins.txt"
        if symbol.endswith(suffix):
            symbol = symbol[:-len(suffix)]
        frame = self._read_tail(symbol)
        closes = frame["Close"].tail(self.benchmark_trend_candles + 1)
        if len(closes) < self.benchmark_trend_candles + 1:
            raise ValueError("insufficient NIFTY candles for trend filter")
        benchmark_return = float(closes.iloc[-1] / closes.iloc[0] - 1.0)
        threshold = self.maximum_adverse_benchmark_return
        trend = (
            "STRONGLY_BULLISH" if benchmark_return >= threshold
            else "STRONGLY_BEARISH" if benchmark_return <= -threshold
            else "NEUTRAL"
        )
        del frame, closes
        return {
            "Benchmark_Return": benchmark_return,
            "Benchmark_Trend": trend,
        }

    def _blank_decision(self, row):
        decision_time = row.Decision_DateTime
        symbol = row.Symbol
        return {
            "Decision_ID": (
                f"{pd.Timestamp(decision_time):%Y%m%d%H%M}_{symbol}"
                if pd.notna(decision_time)
                else f"INVALID_{symbol}"
            ),
            "Symbol": symbol,
            "Decision_DateTime": decision_time,
            "Signal_Source_DateTime": row.Ranking_Source_DateTime,
            "Market_DateTime": pd.NaT,
            "Signal_Age_Minutes": np.nan,
            "Rank": row.Rank,
            "Regime": row.Regime,
            "Original_Signal": row.Mathematical_Signal,
            "Trade_Action": "NO_TRADE",
            "Signal_Strength": row.Signal_Strength,
            "Entry_Price": np.nan,
            "ATR_14": np.nan,
            "Stop_Distance": np.nan,
            "Stop_Loss": np.nan,
            "Target_Price": np.nan,
            "Reward_Risk_Ratio": np.nan,
            "Quantity": 0,
            "Planned_Notional": 0.0,
            "Maximum_Loss": 0.0,
            "Potential_Profit": 0.0,
            "Estimated_Round_Trip_Cost": 0.0,
            "Expected_Profit_Cost_Multiple": np.nan,
            "Expected_Net_Profit": np.nan,
            "Expected_Net_Reward_Risk": np.nan,
            "Benchmark_Return": np.nan,
            "Benchmark_Trend": "NOT_EVALUATED",
            "Portfolio_Exposure_After": np.nan,
            "Trade_Approved": False,
            "Risk_Decision_Reason": "NOT_EVALUATED",
            "Status": "REJECTED",
            "Calculation_Backend": self.calculation_backend,
        }

    def _evaluate(self, signals):
        decisions = []
        used_exposure = 0.0
        approved_positions = 0
        benchmark = self._benchmark_snapshot()

        for row in signals.itertuples(index=False):
            item = self._blank_decision(row)
            try:
                snapshot = self._market_snapshot(row.Symbol)
                item.update(snapshot)
                item.update(benchmark)
                entry = snapshot["Entry_Price"]
                atr = snapshot["ATR_14"]
                market_time = snapshot["Market_DateTime"]
                source_time = row.Ranking_Source_DateTime
                age = (
                    (market_time - source_time).total_seconds() / 60.0
                    if pd.notna(market_time) and pd.notna(source_time)
                    else np.nan
                )
                item["Signal_Age_Minutes"] = age

                rejection = None
                if not bool(row.Signal_Eligible):
                    rejection = "SIGNAL_NOT_ELIGIBLE"
                elif row.Mathematical_Signal not in {"BUY", "SELL", "SELL_SHORT"}:
                    rejection = "NO_ACTIONABLE_SIGNAL"
                elif row.Regime not in {"MEAN_REVERSION", "MOMENTUM"}:
                    rejection = "INVALID_REGIME"
                elif row.Regime == "MOMENTUM" and not self.allow_momentum_entries:
                    rejection = "MOMENTUM_DISABLED"
                elif (
                    self.enable_benchmark_trend_filter
                    and row.Mathematical_Signal in {"SELL", "SELL_SHORT"}
                    and benchmark["Benchmark_Trend"] == "STRONGLY_BULLISH"
                ):
                    rejection = "SHORT_BLOCKED_BY_NIFTY_UPTREND"
                elif (
                    self.enable_benchmark_trend_filter
                    and row.Mathematical_Signal == "BUY"
                    and benchmark["Benchmark_Trend"] == "STRONGLY_BEARISH"
                ):
                    rejection = "BUY_BLOCKED_BY_NIFTY_DOWNTREND"
                elif not np.isfinite(row.Rank) or row.Rank > self.maximum_entry_rank:
                    rejection = "OUTSIDE_ENTRY_RANK_LIMIT"
                elif pd.isna(source_time) or not np.isfinite(age):
                    rejection = "INVALID_SIGNAL_TIMESTAMP"
                elif age < 0:
                    rejection = "SIGNAL_NEWER_THAN_MARKET_DATA"
                elif age > self.maximum_signal_age_minutes:
                    rejection = "STALE_SIGNAL"
                elif not np.isfinite(entry) or entry < self.minimum_price:
                    rejection = "INVALID_ENTRY_PRICE"
                elif not np.isfinite(atr) or atr <= 0:
                    rejection = "INVALID_ATR"
                elif approved_positions >= self.maximum_open_positions:
                    rejection = "MAXIMUM_POSITIONS_REACHED"

                if rejection:
                    item["Risk_Decision_Reason"] = rejection
                    decisions.append(item)
                    continue

                direction = 1 if row.Mathematical_Signal == "BUY" else -1
                if row.Regime == "MOMENTUM":
                    atr_multiplier = self.momentum_atr_multiplier
                    reward_risk = self.momentum_reward_risk
                else:
                    atr_multiplier = self.mean_reversion_atr_multiplier
                    reward_risk = self.mean_reversion_reward_risk

                stop_distance = max(
                    atr_multiplier * atr,
                    entry * self.minimum_stop_percent,
                )
                if stop_distance / entry > self.maximum_stop_percent:
                    item["Risk_Decision_Reason"] = "STOP_DISTANCE_TOO_LARGE"
                    decisions.append(item)
                    continue

                risk_budget = self.initial_capital * self.risk_per_trade
                risk_quantity = int(np.floor(risk_budget / stop_distance))
                allocation_quantity = int(
                    np.floor(
                        self.initial_capital
                        * self.maximum_allocation_per_trade
                        / entry
                    )
                )
                remaining_exposure = max(
                    0.0,
                    self.initial_capital * self.maximum_portfolio_exposure
                    - used_exposure,
                )
                exposure_quantity = int(np.floor(remaining_exposure / entry))
                quantity = min(
                    risk_quantity, allocation_quantity, exposure_quantity
                )
                if quantity < 1:
                    item["Risk_Decision_Reason"] = "QUANTITY_ZERO"
                    decisions.append(item)
                    continue

                stop = entry - direction * stop_distance
                target = entry + direction * reward_risk * stop_distance
                notional = entry * quantity
                maximum_loss = stop_distance * quantity
                potential_profit = reward_risk * maximum_loss
                estimated_cost = (
                    2.0
                    * notional
                    * (
                        self.estimated_transaction_cost_rate
                        + self.estimated_slippage_rate
                    )
                )
                profit_cost_multiple = potential_profit / max(
                    estimated_cost, 1e-12
                )
                expected_net_profit = potential_profit - estimated_cost
                expected_net_loss = maximum_loss + estimated_cost
                expected_net_reward_risk = expected_net_profit / max(
                    expected_net_loss, 1e-12
                )
                if (
                    profit_cost_multiple
                    < self.minimum_expected_profit_cost_multiple
                ):
                    item.update({
                        "Quantity": quantity,
                        "Planned_Notional": notional,
                        "Maximum_Loss": maximum_loss,
                        "Potential_Profit": potential_profit,
                        "Estimated_Round_Trip_Cost": estimated_cost,
                        "Expected_Profit_Cost_Multiple": profit_cost_multiple,
                        "Expected_Net_Profit": expected_net_profit,
                        "Expected_Net_Reward_Risk": expected_net_reward_risk,
                        "Risk_Decision_Reason": "EXPECTED_PROFIT_TOO_SMALL_FOR_COST",
                    })
                    decisions.append(item)
                    continue
                if expected_net_reward_risk < self.minimum_expected_net_reward_risk:
                    item.update({
                        "Quantity": quantity,
                        "Planned_Notional": notional,
                        "Maximum_Loss": maximum_loss,
                        "Potential_Profit": potential_profit,
                        "Estimated_Round_Trip_Cost": estimated_cost,
                        "Expected_Profit_Cost_Multiple": profit_cost_multiple,
                        "Expected_Net_Profit": expected_net_profit,
                        "Expected_Net_Reward_Risk": expected_net_reward_risk,
                        "Risk_Decision_Reason": "NET_REWARD_RISK_TOO_LOW",
                    })
                    decisions.append(item)
                    continue
                used_exposure += notional
                approved_positions += 1

                item.update(
                    {
                        "Trade_Action": (
                            "BUY" if direction == 1 else "SELL_SHORT"
                        ),
                        "Stop_Distance": stop_distance,
                        "Stop_Loss": stop,
                        "Target_Price": target,
                        "Reward_Risk_Ratio": reward_risk,
                        "Quantity": quantity,
                        "Planned_Notional": notional,
                        "Maximum_Loss": maximum_loss,
                        "Potential_Profit": potential_profit,
                        "Estimated_Round_Trip_Cost": estimated_cost,
                        "Expected_Profit_Cost_Multiple": profit_cost_multiple,
                        "Expected_Net_Profit": expected_net_profit,
                        "Expected_Net_Reward_Risk": expected_net_reward_risk,
                        "Portfolio_Exposure_After": (
                            used_exposure / self.initial_capital
                        ),
                        "Trade_Approved": True,
                        "Risk_Decision_Reason": "APPROVED",
                        "Status": "PLANNED",
                    }
                )
            except Exception as error:
                item["Risk_Decision_Reason"] = (
                    f"DATA_ERROR: {type(error).__name__}: {error}"
                )
            decisions.append(item)

        return pd.DataFrame(decisions, columns=self.DECISION_COLUMNS)

    def _existing_ledger(self):
        if not os.path.isfile(self.output_file):
            return pd.DataFrame(columns=self.DECISION_COLUMNS)
        try:
            frame = pd.read_excel(
                self.output_file,
                sheet_name="Trade_Plans",
                engine="openpyxl",
            )
            return frame.reindex(columns=self.DECISION_COLUMNS)
        except (ValueError, OSError):
            return pd.DataFrame(columns=self.DECISION_COLUMNS)

    def _save(self, current):
        target = Path(self.output_file).expanduser().resolve()
        target.parent.mkdir(parents=True, exist_ok=True)
        existing = self._existing_ledger()
        if existing.empty:
            history = current.copy()
        elif current.empty:
            history = existing
        else:
            history = pd.concat(
                [existing, current], ignore_index=True, copy=False
            )
        del existing
        history.drop_duplicates("Decision_ID", keep="last", inplace=True)
        history = history.tail(self.maximum_ledger_rows).copy()
        approved = current.loc[current["Trade_Approved"]].copy()
        rejected = current.loc[~current["Trade_Approved"]].copy()
        summary = pd.DataFrame(
            {
                "Metric": [
                    "Decision DateTime", "Signals Received", "Approved Trades",
                    "Rejected Trades", "Initial Capital", "Risk Per Trade",
                    "Planned Exposure", "Planned Maximum Loss",
                    "Calculation Backend",
                ],
                "Value": [
                    current["Decision_DateTime"].max() if not current.empty else pd.NaT,
                    len(current), len(approved), len(rejected),
                    self.initial_capital, self.risk_per_trade,
                    approved["Planned_Notional"].sum(),
                    approved["Maximum_Loss"].sum(), self.calculation_backend,
                ],
            }
        )

        descriptor, temporary = tempfile.mkstemp(
            prefix=".trading_ledger_", suffix=".xlsx", dir=target.parent
        )
        os.close(descriptor)
        try:
            with pd.ExcelWriter(temporary, engine="openpyxl") as writer:
                summary.to_excel(writer, sheet_name="Summary", index=False)
                current.to_excel(writer, sheet_name="All_Risk_Decisions", index=False)
                approved.to_excel(writer, sheet_name="Approved_Trades", index=False)
                rejected.to_excel(writer, sheet_name="Rejected_Trades", index=False)
                history.to_excel(writer, sheet_name="Trade_Plans", index=False)
                for worksheet in writer.book.worksheets:
                    worksheet.freeze_panes = "A2"
                    worksheet.auto_filter.ref = worksheet.dimensions
                    worksheet.sheet_view.showGridLines = False
            os.replace(temporary, target)
        except Exception:
            if os.path.exists(temporary):
                os.unlink(temporary)
            raise
        del history, approved, rejected, summary
        return str(target)

    def calculate_all(self):
        signals = self._load_signals()
        report = self._evaluate(signals)
        del signals

        saved = None
        if self.save_excel:
            saved = self._save(report)
            if not os.path.isfile(saved) or os.path.getsize(saved) == 0:
                raise RuntimeError("Trading ledger verification failed")

        report.attrs["Excel_File"] = saved
        report.attrs["Approved_Trades"] = int(
            report["Trade_Approved"].sum()
        ) if not report.empty else 0

        if self.print_details and not report.empty:
            print(
                report[
                    [
                        "Symbol", "Trade_Action", "Entry_Price", "Stop_Loss",
                        "Target_Price", "Quantity", "Trade_Approved",
                        "Risk_Decision_Reason",
                    ]
                ].to_string(index=False)
            )
        if saved:
            print(f"TradingLedger.xlsx file is created: {saved}")

        if self.retain_results_in_memory:
            risk_data = {
                row.Symbol: report.loc[report["Symbol"].eq(row.Symbol)].copy()
                for row in report.itertuples(index=False)
            }
        else:
            risk_data = {}
        return risk_data, report
