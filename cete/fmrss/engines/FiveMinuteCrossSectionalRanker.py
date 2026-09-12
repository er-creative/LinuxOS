import os
import tempfile
from pathlib import Path

import numpy as np
import pandas as pd


class FiveMinuteCrossSectionalRanker:
    """Rank only accepted setups stored by MathematicalScoreEngine.

    The engine reads the compact ``Accepted_Setups`` worksheet, calculates a
    cross-sectional score, selects the best N symbols, and atomically writes
    ``RankingShares.xlsx``.  No full candle history is loaded.
    """

    DEFAULT_SCORE_FILE = (
        "/home/devinderjeet/fmrss/report/MathematicalScoreEngine.xlsx"
    )
    DEFAULT_OUTPUT_FILE = (
        "/home/devinderjeet/fmrss/report/RankingShares.xlsx"
    )
    DEFAULT_SHEET = "Accepted_Setups"

    INPUT_COLUMNS = [
        "Symbol",
        "Status",
        "Latest_DateTime",
        "Latest_Stable_Regime",
        "Latest_Residual_ZScore",
        "Latest_Volume_Factor",
        "Latest_Mathematical_Score",
        "Latest_Residual_Quality_Score",
        "Latest_Expected_Move_Percent",
        "Latest_Edge_To_Cost_Ratio",
        "Latest_Setup_Ready",
        "Latest_Setup_Accepted",
        "Latest_Volatility_Eligible",
        "Latest_Volume_Confirmed",
        "Latest_Rejection_Reason",
    ]

    OUTPUT_COLUMNS = [
        "Symbol",
        "Status",
        "Decision_DateTime",
        "Latest_DateTime",
        "Data_Age_Minutes",
        "Latest_Regime",
        "Mathematical_Setup_Accepted",
        "Residual_ZScore",
        "Volume_Factor",
        "Latest_Mathematical_Score",
        "Mathematical_Percentile",
        "Residual_Percentile",
        "Volume_Percentile",
        "Quality_Percentile",
        "Economic_Edge_Percentile",
        "Latest_Cross_Sectional_Score",
        "Latest_Rank",
        "Latest_Candidate_Count",
        "Latest_Eligible",
        "Latest_Selected",
        "Latest_Selection_Reason",
        "Score_Rejection_Reason",
        "Calculation_Backend",
    ]

    DIRECTIONAL_REGIMES = {"MOMENTUM", "MEAN_REVERSION"}

    def __init__(
        self,
        maximum_selected_symbols=5,
        mathematical_weight=0.45,
        residual_weight=0.15,
        volume_weight=0.10,
        quality_weight=0.20,
        economic_edge_weight=0.10,
        maximum_staleness_minutes=15,
        mathematical_score_file=DEFAULT_SCORE_FILE,
        accepted_sheet_name=DEFAULT_SHEET,
        ranking_output_file=DEFAULT_OUTPUT_FILE,
        save_excel=True,
        memory_optimized=True,
        print_symbol_details=False,
    ):
        if not isinstance(maximum_selected_symbols, (int, np.integer)):
            raise TypeError("maximum_selected_symbols must be an integer")
        if maximum_selected_symbols < 1:
            raise ValueError("maximum_selected_symbols must be at least 1")

        weights = np.asarray(
            [mathematical_weight, residual_weight, volume_weight,
             quality_weight, economic_edge_weight],
            dtype=np.float64,
        )
        if not np.isfinite(weights).all() or (weights < 0).any():
            raise ValueError("Ranking weights must be finite and non-negative")
        if not np.isclose(weights.sum(), 1.0, atol=1e-9, rtol=0.0):
            raise ValueError("Ranking weights must sum to 1.0")
        if maximum_staleness_minutes is not None and maximum_staleness_minutes < 0:
            raise ValueError("maximum_staleness_minutes cannot be negative")

        self.maximum_selected_symbols = int(maximum_selected_symbols)
        self.mathematical_weight = float(mathematical_weight)
        self.residual_weight = float(residual_weight)
        self.volume_weight = float(volume_weight)
        self.quality_weight = float(quality_weight)
        self.economic_edge_weight = float(economic_edge_weight)
        self.maximum_staleness_minutes = maximum_staleness_minutes
        self.mathematical_score_file = str(mathematical_score_file)
        self.accepted_sheet_name = str(accepted_sheet_name)
        self.ranking_output_file = str(ranking_output_file)
        self.save_excel = bool(save_excel)
        self.memory_optimized = bool(memory_optimized)
        self.print_symbol_details = bool(print_symbol_details)
        self.calculation_backend = "NUMPY_PANDAS"

    @staticmethod
    def _as_boolean(series):
        if pd.api.types.is_bool_dtype(series):
            return series.fillna(False)
        normalized = series.astype("string").str.strip().str.upper()
        return normalized.isin({"TRUE", "1", "YES", "Y"})

    @staticmethod
    def _percentile(series):
        # Average percentile makes tied inputs receive the same component score.
        return series.rank(method="average", pct=True).mul(100.0)

    def _resolve_sheet(self, file_path, requested_sheet):
        with pd.ExcelFile(file_path, engine="openpyxl") as workbook:
            lookup = {name.casefold(): name for name in workbook.sheet_names}
        key = requested_sheet.casefold()
        if key not in lookup:
            raise ValueError(
                f"Worksheet {requested_sheet!r} is missing from {file_path}. "
                f"Available worksheets: {list(lookup.values())}"
            )
        return lookup[key]

    def _load_accepted_setups(self, file_path, sheet_name):
        if not os.path.isfile(file_path):
            raise FileNotFoundError(f"Mathematical score workbook not found: {file_path}")

        actual_sheet = self._resolve_sheet(file_path, sheet_name)
        header = pd.read_excel(
            file_path,
            sheet_name=actual_sheet,
            nrows=0,
            engine="openpyxl",
        )
        missing = [column for column in self.INPUT_COLUMNS if column not in header]
        if missing:
            raise ValueError(
                f"Worksheet {actual_sheet!r} is missing required columns: {missing}"
            )

        frame = pd.read_excel(
            file_path,
            sheet_name=actual_sheet,
            usecols=self.INPUT_COLUMNS,
            engine="openpyxl",
        )
        if frame.empty:
            return frame

        frame["Symbol"] = (
            frame["Symbol"].astype("string").str.strip().str.upper()
        )
        frame["Status"] = (
            frame["Status"].astype("string").str.strip().str.upper()
        )
        frame["Latest_Stable_Regime"] = (
            frame["Latest_Stable_Regime"]
            .astype("string")
            .str.strip()
            .str.upper()
        )
        frame["Latest_DateTime"] = pd.to_datetime(
            frame["Latest_DateTime"], errors="coerce"
        )

        numeric = [
            "Latest_Residual_ZScore",
            "Latest_Volume_Factor",
            "Latest_Mathematical_Score",
            "Latest_Residual_Quality_Score",
            "Latest_Expected_Move_Percent",
            "Latest_Edge_To_Cost_Ratio",
        ]
        for column in numeric:
            frame[column] = pd.to_numeric(frame[column], errors="coerce")
            if self.memory_optimized:
                frame[column] = frame[column].astype("float32")

        for column in [
            "Latest_Setup_Ready",
            "Latest_Setup_Accepted",
            "Latest_Volatility_Eligible",
            "Latest_Volume_Confirmed",
        ]:
            frame[column] = self._as_boolean(frame[column])

        # The worksheet should already contain accepted rows. Rechecking protects
        # the ranker from a manually edited or stale workbook.
        frame = frame.loc[
            frame["Status"].eq("CALCULATED")
            & frame["Latest_Setup_Ready"]
            & frame["Latest_Setup_Accepted"]
        ].copy()
        frame.drop_duplicates(subset=["Symbol"], keep="last", inplace=True)
        frame.reset_index(drop=True, inplace=True)
        return frame

    def _empty_report(self):
        return pd.DataFrame(columns=self.OUTPUT_COLUMNS)

    def _rank(self, accepted):
        if accepted.empty:
            return self._empty_report()

        decision_time = accepted["Latest_DateTime"].max()
        accepted["Data_Age_Minutes"] = (
            (decision_time - accepted["Latest_DateTime"]).dt.total_seconds() / 60.0
        )

        finite = accepted[
            [
                "Latest_Residual_ZScore",
                "Latest_Volume_Factor",
                "Latest_Mathematical_Score",
                "Latest_Residual_Quality_Score",
                "Latest_Expected_Move_Percent",
                "Latest_Edge_To_Cost_Ratio",
            ]
        ].notna().all(axis=1)
        directional = accepted["Latest_Stable_Regime"].isin(
            self.DIRECTIONAL_REGIMES
        )
        timestamp_valid = accepted["Latest_DateTime"].notna()
        if self.maximum_staleness_minutes is None:
            fresh = timestamp_valid
        else:
            fresh = timestamp_valid & accepted["Data_Age_Minutes"].le(
                float(self.maximum_staleness_minutes)
            )

        eligible = finite & directional & fresh
        candidates = accepted.loc[eligible].copy()

        if candidates.empty:
            result = accepted.copy()
            result["Mathematical_Percentile"] = np.nan
            result["Residual_Percentile"] = np.nan
            result["Volume_Percentile"] = np.nan
            result["Quality_Percentile"] = np.nan
            result["Economic_Edge_Percentile"] = np.nan
            result["Latest_Cross_Sectional_Score"] = np.nan
            result["Latest_Rank"] = pd.NA
        else:
            candidates["Mathematical_Percentile"] = self._percentile(
                candidates["Latest_Mathematical_Score"]
            )
            candidates["Residual_Percentile"] = self._percentile(
                candidates["Latest_Residual_ZScore"].abs()
            )
            candidates["Volume_Percentile"] = self._percentile(
                candidates["Latest_Volume_Factor"]
            )
            candidates["Quality_Percentile"] = self._percentile(
                candidates["Latest_Residual_Quality_Score"]
            )
            candidates["Economic_Edge_Percentile"] = self._percentile(
                candidates["Latest_Edge_To_Cost_Ratio"]
            )
            candidates["Latest_Cross_Sectional_Score"] = (
                self.mathematical_weight
                * candidates["Mathematical_Percentile"]
                + self.residual_weight * candidates["Residual_Percentile"]
                + self.volume_weight * candidates["Volume_Percentile"]
                + self.quality_weight * candidates["Quality_Percentile"]
                + self.economic_edge_weight * candidates["Economic_Edge_Percentile"]
            )
            candidates.sort_values(
                by=[
                    "Latest_Cross_Sectional_Score",
                    "Latest_Mathematical_Score",
                    "Symbol",
                ],
                ascending=[False, False, True],
                kind="mergesort",
                inplace=True,
            )
            candidates["Latest_Rank"] = np.arange(
                1, len(candidates) + 1, dtype=np.int16
            )
            derived = candidates.set_index("Symbol")[
                [
                    "Mathematical_Percentile",
                    "Residual_Percentile",
                    "Volume_Percentile",
                    "Quality_Percentile",
                    "Economic_Edge_Percentile",
                    "Latest_Cross_Sectional_Score",
                    "Latest_Rank",
                ]
            ]
            result = accepted.drop(
                columns=[
                    "Mathematical_Percentile",
                    "Residual_Percentile",
                    "Volume_Percentile",
                    "Quality_Percentile",
                    "Economic_Edge_Percentile",
                    "Latest_Cross_Sectional_Score",
                    "Latest_Rank",
                ],
                errors="ignore",
            ).join(derived, on="Symbol")

        result["Latest_Eligible"] = eligible.to_numpy(dtype=bool)
        result["Latest_Selected"] = (
            result["Latest_Eligible"]
            & pd.to_numeric(result["Latest_Rank"], errors="coerce").le(
                self.maximum_selected_symbols
            )
        )
        candidate_count = int(eligible.sum())
        result["Latest_Candidate_Count"] = candidate_count

        conditions = [
            ~timestamp_valid,
            timestamp_valid & ~fresh,
            fresh & ~directional,
            fresh & directional & ~finite,
            result["Latest_Selected"],
            result["Latest_Eligible"],
        ]
        choices = [
            "INVALID_TIMESTAMP",
            "STALE_INPUT",
            "INVALID_REGIME",
            "INVALID_RANKING_FEATURES",
            "SELECTED",
            "OUTSIDE_TOP_N",
        ]
        result["Latest_Selection_Reason"] = np.select(
            conditions, choices, default="NOT_ELIGIBLE"
        )

        result["Decision_DateTime"] = decision_time
        result["Latest_Regime"] = result["Latest_Stable_Regime"]
        result["Mathematical_Setup_Accepted"] = result[
            "Latest_Setup_Accepted"
        ]
        result["Residual_ZScore"] = result["Latest_Residual_ZScore"]
        result["Volume_Factor"] = result["Latest_Volume_Factor"]
        result["Score_Rejection_Reason"] = result[
            "Latest_Rejection_Reason"
        ].fillna("ACCEPTED")
        result["Calculation_Backend"] = self.calculation_backend

        result.sort_values(
            by=["Latest_Eligible", "Latest_Rank", "Symbol"],
            ascending=[False, True, True],
            na_position="last",
            kind="mergesort",
            inplace=True,
        )
        result.reset_index(drop=True, inplace=True)
        return result[self.OUTPUT_COLUMNS].copy()

    @staticmethod
    def _summary(report):
        selected = int(report["Latest_Selected"].sum()) if not report.empty else 0
        eligible = int(report["Latest_Eligible"].sum()) if not report.empty else 0
        decision = (
            report["Decision_DateTime"].max() if not report.empty else pd.NaT
        )
        return pd.DataFrame(
            {
                "Metric": [
                    "Decision DateTime",
                    "Accepted Shares Read",
                    "Eligible Ranking Candidates",
                    "Selected Shares",
                ],
                "Value": [decision, len(report), eligible, selected],
            }
        )

    def _write_excel(self, report, output_file):
        target = Path(output_file).expanduser().resolve()
        target.parent.mkdir(parents=True, exist_ok=True)
        selected = report.loc[report["Latest_Selected"]].copy()
        summary = self._summary(report)
        reason_summary = (
            report["Latest_Selection_Reason"]
            .value_counts(dropna=False)
            .rename_axis("Reason")
            .reset_index(name="Share_Count")
        )

        file_descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{target.stem}_", suffix=".xlsx", dir=str(target.parent)
        )
        os.close(file_descriptor)
        try:
            with pd.ExcelWriter(temporary_name, engine="openpyxl") as writer:
                summary.to_excel(writer, sheet_name="Summary", index=False)
                report.to_excel(writer, sheet_name="All_Shares", index=False)
                selected.to_excel(writer, sheet_name="Selected_Shares", index=False)
                reason_summary.to_excel(
                    writer, sheet_name="Reason_Summary", index=False
                )
                for worksheet in writer.book.worksheets:
                    worksheet.freeze_panes = "A2"
                    worksheet.auto_filter.ref = worksheet.dimensions
                    for column_cells in worksheet.iter_cols():
                        width = min(
                            32,
                            max(
                                10,
                                max(
                                    len(str(cell.value)) if cell.value is not None else 0
                                    for cell in column_cells
                                )
                                + 2,
                            ),
                        )
                        worksheet.column_dimensions[
                            column_cells[0].column_letter
                        ].width = width
            os.replace(temporary_name, target)
        except Exception:
            if os.path.exists(temporary_name):
                os.unlink(temporary_name)
            raise
        return str(target)

    def rank_from_excel(
        self,
        mathematical_score_file=None,
        sheet_name=None,
        output_file=None,
    ):
        source = mathematical_score_file or self.mathematical_score_file
        sheet = sheet_name or self.accepted_sheet_name
        destination = output_file or self.ranking_output_file

        accepted = self._load_accepted_setups(source, sheet)
        report = self._rank(accepted)
        if self.save_excel:
            written_file = self._write_excel(report, destination)
            report.attrs["Excel_File"] = written_file

        # Only compact one-row records are returned; candle history is never held.
        ranked_data = {
            row.Symbol: pd.DataFrame([row._asdict()])
            for row in report.itertuples(index=False)
        }

        if self.print_symbol_details:
            for row in report.itertuples(index=False):
                rank = "-" if pd.isna(row.Latest_Rank) else int(row.Latest_Rank)
                print(
                    f"{row.Symbol:<15} Rank={rank} | "
                    f"Selected={bool(row.Latest_Selected)} | "
                    f"Reason={row.Latest_Selection_Reason}"
                )
        return ranked_data, report

    # Alias for callers that prefer the calculate_all naming convention.
    def calculate_all_from_excel(self, **kwargs):
        return self.rank_from_excel(**kwargs)

    def rank_all(self, score_data):
        """Rank accepted latest rows already held in memory.

        This compatibility path avoids a workbook read in notebooks.  The
        production Phase-A path can continue to use ``rank_from_excel``.
        """
        rows = []
        for symbol, frame in score_data.items():
            if not isinstance(frame, pd.DataFrame) or frame.empty:
                continue
            latest = frame.iloc[-1]
            row = {"Symbol": symbol, "Status": "CALCULATED"}
            for column in self.INPUT_COLUMNS:
                if column not in row:
                    source = column.removeprefix("Latest_")
                    row[column] = latest.get(source, latest.get(column, np.nan))
            rows.append(row)
        accepted = pd.DataFrame(rows)
        if accepted.empty:
            report = self._empty_report()
        else:
            accepted["Latest_Setup_Ready"] = self._as_boolean(
                accepted["Latest_Setup_Ready"]
            )
            accepted["Latest_Setup_Accepted"] = self._as_boolean(
                accepted["Latest_Setup_Accepted"]
            )
            accepted = accepted.loc[
                accepted["Latest_Setup_Ready"]
                & accepted["Latest_Setup_Accepted"]
            ].copy()
            report = self._rank(accepted)
        if self.save_excel:
            report.attrs["Excel_File"] = self._write_excel(
                report, self.ranking_output_file
            )
        ranked = {
            row.Symbol: pd.DataFrame([row._asdict()])
            for row in report.itertuples(index=False)
        }
        return ranked, report
