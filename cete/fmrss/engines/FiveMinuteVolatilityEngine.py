import numpy as np
import pandas as pd


class FiveMinuteVolatilityEngine:

    # =====================================================
    # Required Input Columns
    # =====================================================

    REQUIRED_COLUMNS = [
        "Symbol",
        "DateTime",
        "High",
        "Low",
        "Close",
        "Residual_Return"
    ]

    # =====================================================
    # Constructor
    # =====================================================

    def __init__(
        self,
        ewma_span=20,
        ewma_min_periods=15,
        minimum_normalized_volatility=0.001,
        high_normalized_volatility=0.005,
        maximum_normalized_volatility=0.015,
        numerical_epsilon=1e-12,
        reset_previous_close_at_session_start=True
    ):

        if ewma_span < 2:
            raise ValueError(
                "ewma_span must be at least 2"
            )

        if not 1 <= ewma_min_periods <= ewma_span:
            raise ValueError(
                "ewma_min_periods must be between "
                "1 and ewma_span"
            )

        if minimum_normalized_volatility <= 0:
            raise ValueError(
                "minimum_normalized_volatility must be positive"
            )

        if high_normalized_volatility <= minimum_normalized_volatility:
            raise ValueError(
                "high_normalized_volatility must be greater than "
                "minimum_normalized_volatility"
            )

        if maximum_normalized_volatility <= high_normalized_volatility:
            raise ValueError(
                "maximum_normalized_volatility must be greater than "
                "high_normalized_volatility"
            )

        if numerical_epsilon <= 0:
            raise ValueError(
                "numerical_epsilon must be positive"
            )

        self.ewma_span = int(ewma_span)

        self.ewma_min_periods = int(
            ewma_min_periods
        )

        self.minimum_normalized_volatility = float(
            minimum_normalized_volatility
        )

        self.high_normalized_volatility = float(
            high_normalized_volatility
        )

        self.maximum_normalized_volatility = float(
            maximum_normalized_volatility
        )

        self.numerical_epsilon = float(
            numerical_epsilon
        )

        self.reset_previous_close_at_session_start = bool(
            reset_previous_close_at_session_start
        )

        # NumPy vector operations and pandas EWM already execute in
        # compiled code. A separate Cython extension would add import and
        # maintenance overhead without removing a Python rolling callback.
        self.calculation_backend = "NUMPY_PANDAS"

    # =====================================================
    # Validate Input
    # =====================================================

    def _validate_input(self, symbol, df):

        if not isinstance(df, pd.DataFrame):
            raise TypeError(
                f"{symbol} data must be a pandas DataFrame"
            )

        if df.empty:
            raise ValueError(
                f"{symbol} volume data is empty"
            )

        missing_columns = [
            column
            for column in self.REQUIRED_COLUMNS
            if column not in df.columns
        ]

        if missing_columns:
            raise ValueError(
                f"{symbol} is missing required columns: "
                f"{missing_columns}"
            )

    # =====================================================
    # Prepare Input
    # =====================================================

    def _prepare_input(self, symbol, df):

        self._validate_input(
            symbol=symbol,
            df=df
        )

        result = df.copy()

        result["DateTime"] = pd.to_datetime(
            result["DateTime"],
            errors="coerce"
        )

        numeric_columns = [
            "High",
            "Low",
            "Close",
            "Residual_Return"
        ]

        for column in numeric_columns:
            result[column] = pd.to_numeric(
                result[column],
                errors="coerce"
            )

        result[numeric_columns] = (
            result[numeric_columns]
            .replace(
                [np.inf, -np.inf],
                np.nan
            )
        )

        invalid_timestamp = result["DateTime"].isna()

        invalid_price = (
            result["High"].isna()
            | result["Low"].isna()
            | result["Close"].isna()
            | (result["High"] <= 0)
            | (result["Low"] <= 0)
            | (result["Close"] <= 0)
            | (result["High"] < result["Low"])
            | (result["High"] < result["Close"])
            | (result["Low"] > result["Close"])
        )

        invalid_rows = invalid_timestamp | invalid_price

        if invalid_rows.any():
            result.drop(
                index=result.index[invalid_rows],
                inplace=True
            )

        result["Symbol"] = (
            str(symbol)
            .strip()
            .upper()
        )

        result.sort_values(
            "DateTime",
            inplace=True
        )

        result.drop_duplicates(
            subset=["DateTime"],
            keep="last",
            inplace=True
        )

        result.reset_index(
            drop=True,
            inplace=True
        )

        if result.empty:
            raise ValueError(
                f"{symbol} has no valid OHLC rows"
            )

        return result

    # =====================================================
    # Previous Close
    # =====================================================

    def _calculate_previous_close(self, df):

        if self.reset_previous_close_at_session_start:

            trading_date = df["DateTime"].dt.normalize()

            previous_close = (
                df["Close"]
                .groupby(
                    trading_date,
                    sort=False
                )
                .shift(1)
            )

        else:
            previous_close = df["Close"].shift(1)

        df["Previous_Close"] = previous_close

        return df

    # =====================================================
    # True Range
    # =====================================================

    @staticmethod
    def _calculate_true_range(df):

        high = df["High"].to_numpy(
            dtype=np.float64,
            copy=False
        )

        low = df["Low"].to_numpy(
            dtype=np.float64,
            copy=False
        )

        previous_close = df["Previous_Close"].to_numpy(
            dtype=np.float64,
            copy=False
        )

        high_low_range = high - low

        high_previous_range = np.abs(
            high - previous_close
        )

        low_previous_range = np.abs(
            low - previous_close
        )

        # At the first candle of a session, Previous_Close is NaN and the
        # five-minute high-low range is used instead of an overnight gap.
        missing_previous = ~np.isfinite(
            previous_close
        )

        high_previous_range[missing_previous] = (
            high_low_range[missing_previous]
        )

        low_previous_range[missing_previous] = (
            high_low_range[missing_previous]
        )

        true_range = high_low_range.copy()

        np.maximum(
            true_range,
            high_previous_range,
            out=true_range
        )

        np.maximum(
            true_range,
            low_previous_range,
            out=true_range
        )

        df["True_Range"] = true_range

        return df

    # =====================================================
    # EWMA and Normalized Volatility
    # =====================================================

    def _calculate_normalized_volatility(self, df):

        ewma_range = (
            df["True_Range"]
            .ewm(
                span=self.ewma_span,
                adjust=False,
                min_periods=self.ewma_min_periods,
                ignore_na=True
            )
            .mean()
        )

        close_values = df["Close"].to_numpy(
            dtype=np.float64,
            copy=False
        )

        ewma_values = ewma_range.to_numpy(
            dtype=np.float64,
            copy=False
        )

        normalized = np.full(
            len(df),
            np.nan,
            dtype=np.float64
        )

        valid = (
            np.isfinite(ewma_values)
            & np.isfinite(close_values)
            & (close_values > self.numerical_epsilon)
        )

        np.divide(
            ewma_values,
            close_values,
            out=normalized,
            where=valid
        )

        df["EWMA_Range"] = ewma_values

        df["Normalized_Volatility"] = normalized

        df["Volatility_Features_Ready"] = valid

        return df

    # =====================================================
    # Volatility-Adjusted Residual
    # =====================================================

    def _calculate_adjusted_residual(self, df):

        residual = df["Residual_Return"].to_numpy(
            dtype=np.float64,
            copy=False
        )

        normalized = df[
            "Normalized_Volatility"
        ].to_numpy(
            dtype=np.float64,
            copy=False
        )

        adjusted = np.full(
            len(df),
            np.nan,
            dtype=np.float64
        )

        valid = (
            df["Volatility_Features_Ready"].to_numpy(
                dtype=bool,
                copy=False
            )
            & np.isfinite(residual)
            & (normalized > self.numerical_epsilon)
        )

        np.divide(
            residual,
            normalized,
            out=adjusted,
            where=valid
        )

        df["Volatility_Adjusted_Residual"] = adjusted

        return df

    # =====================================================
    # Volatility State and Eligibility
    # =====================================================

    def _classify_volatility(self, df):

        ready = df["Volatility_Features_Ready"]
        volatility = df["Normalized_Volatility"]

        state = np.select(
            condlist=[
                ~ready,
                volatility < self.minimum_normalized_volatility,
                volatility < self.high_normalized_volatility,
                volatility <= self.maximum_normalized_volatility
            ],
            choicelist=[
                "NOT_READY",
                "TOO_LOW",
                "NORMAL",
                "HIGH"
            ],
            default="EXTREME"
        )

        df["Volatility_State"] = pd.Categorical(
            state,
            categories=[
                "NOT_READY",
                "TOO_LOW",
                "NORMAL",
                "HIGH",
                "EXTREME"
            ]
        )

        df["Volatility_Eligible"] = (
            ready
            & (
                volatility
                >= self.minimum_normalized_volatility
            )
            & (
                volatility
                <= self.maximum_normalized_volatility
            )
        )

        return df

    # =====================================================
    # Calculate One Symbol
    # =====================================================

    def calculate_symbol(
        self,
        symbol,
        volume_df
    ):

        symbol = (
            str(symbol)
            .strip()
            .upper()
        )

        df = self._prepare_input(
            symbol=symbol,
            df=volume_df
        )

        df = self._calculate_previous_close(df)

        df = self._calculate_true_range(df)

        df = self._calculate_normalized_volatility(df)

        df = self._calculate_adjusted_residual(df)

        df = self._classify_volatility(df)

        ready_rows = int(
            df["Volatility_Features_Ready"].sum()
        )

        latest = df.iloc[-1]

        report = {
            "Symbol": symbol,
            "Status": (
                "CALCULATED"
                if ready_rows > 0
                else "NOT_READY"
            ),
            "Input_Rows": len(df),
            "Volatility_Ready_Rows": ready_rows,
            "Latest_DateTime": latest["DateTime"],
            "Latest_True_Range": latest["True_Range"],
            "Latest_EWMA_Range": latest["EWMA_Range"],
            "Latest_Normalized_Volatility": (
                latest["Normalized_Volatility"]
            ),
            "Latest_Adjusted_Residual": (
                latest["Volatility_Adjusted_Residual"]
            ),
            "Latest_Volatility_State": str(
                latest["Volatility_State"]
            ),
            "Latest_Volatility_Eligible": bool(
                latest["Volatility_Eligible"]
            ),
            "Calculation_Backend": (
                self.calculation_backend
            )
        }

        return df, report

    # =====================================================
    # Calculate All Symbols
    # =====================================================

    def calculate_all(self, volume_data):

        if not isinstance(volume_data, dict):
            raise TypeError(
                "volume_data must be a dictionary "
                "containing {symbol: DataFrame}"
            )

        if not volume_data:
            raise ValueError(
                "volume_data is empty"
            )

        volatility_data = {}
        reports = []

        print("=" * 115)
        print("FMRSS : CALCULATING VOLATILITY FEATURES")
        print(
            "Calculation backend : "
            f"{self.calculation_backend}"
        )
        print("=" * 115)

        for symbol, volume_df in volume_data.items():

            symbol = (
                str(symbol)
                .strip()
                .upper()
            )

            try:

                result_df, report = self.calculate_symbol(
                    symbol=symbol,
                    volume_df=volume_df
                )

                reports.append(report)

                if report["Status"] == "NOT_READY":

                    print(
                        f"{symbol:<15}"
                        "NOT READY : No valid volatility rows"
                    )

                    continue

                volatility_data[symbol] = result_df

                normalized = report[
                    "Latest_Normalized_Volatility"
                ]

                normalized_text = (
                    f"{normalized:.6f}"
                    if pd.notna(normalized)
                    else "NaN"
                )

                print(
                    f"{symbol:<15}"
                    f"CALCULATED : "
                    f"NV={normalized_text} | "
                    f"State={report['Latest_Volatility_State']} | "
                    f"Eligible={report['Latest_Volatility_Eligible']}"
                )

            except Exception as error:

                reports.append({
                    "Symbol": symbol,
                    "Status": "FAILED",
                    "Input_Rows": (
                        len(volume_df)
                        if isinstance(
                            volume_df,
                            pd.DataFrame
                        )
                        else 0
                    ),
                    "Volatility_Ready_Rows": 0,
                    "Latest_DateTime": pd.NaT,
                    "Latest_True_Range": np.nan,
                    "Latest_EWMA_Range": np.nan,
                    "Latest_Normalized_Volatility": np.nan,
                    "Latest_Adjusted_Residual": np.nan,
                    "Latest_Volatility_State": "NOT_READY",
                    "Latest_Volatility_Eligible": False,
                    "Calculation_Backend": self.calculation_backend,
                    "Error": str(error)
                })

                print(
                    f"{symbol:<15}"
                    f"FAILED : {error}"
                )

        report_df = pd.DataFrame(reports)

        if not report_df.empty:

            report_df.sort_values(
                by=["Status", "Symbol"],
                inplace=True
            )

            report_df.reset_index(
                drop=True,
                inplace=True
            )

        print("=" * 115)
        print(
            f"Successfully calculated : "
            f"{len(volatility_data)}/{len(volume_data)}"
        )
        print("=" * 115)

        if not volatility_data:
            raise RuntimeError(
                "Volatility features could not be "
                "calculated for any symbol"
            )

        return volatility_data, report_df
