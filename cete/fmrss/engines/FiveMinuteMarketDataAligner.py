import pandas as pd


class FiveMinuteMarketDataAligner:

    # =====================================================
    # Required Columns
    # =====================================================

    SHARE_REQUIRED_COLUMNS = [
        "Symbol",
        "Date",
        "Time",
        "DateTime",
        "Open",
        "High",
        "Low",
        "Close",
        "Volume"
    ]

    NIFTY_REQUIRED_COLUMNS = [
        "DateTime",
        "Open",
        "High",
        "Low",
        "Close"
    ]

    FINAL_COLUMNS = [
        "Symbol",
        "Date",
        "Time",
        "DateTime",
        "Open",
        "High",
        "Low",
        "Close",
        "Volume",
        "Nifty_Open",
        "Nifty_High",
        "Nifty_Low",
        "Nifty_Close"
    ]

    # =====================================================
    # Constructor
    # =====================================================

    def __init__(
        self,
        minimum_aligned_candles=450,
        minimum_alignment_ratio=0.90
    ):

        if minimum_aligned_candles < 2:
            raise ValueError(
                "minimum_aligned_candles must be at least 2"
            )

        if not 0 < minimum_alignment_ratio <= 1:
            raise ValueError(
                "minimum_alignment_ratio must be between "
                "0 and 1"
            )

        self.minimum_aligned_candles = (
            int(minimum_aligned_candles)
        )

        self.minimum_alignment_ratio = (
            float(minimum_alignment_ratio)
        )

    # =====================================================
    # Validate DataFrame
    # =====================================================

    @staticmethod
    def _validate_dataframe(
        df,
        dataframe_name,
        required_columns
    ):

        if not isinstance(df, pd.DataFrame):
            raise TypeError(
                f"{dataframe_name} must be a pandas DataFrame"
            )

        if df.empty:
            raise ValueError(
                f"{dataframe_name} is empty"
            )

        missing_columns = [
            column
            for column in required_columns
            if column not in df.columns
        ]

        if missing_columns:
            raise ValueError(
                f"{dataframe_name} is missing columns: "
                f"{missing_columns}"
            )

    # =====================================================
    # Prepare Share Data
    # =====================================================

    def _prepare_share_data(self, symbol, share_df):

        self._validate_dataframe(
            df=share_df,
            dataframe_name=f"{symbol} share data",
            required_columns=self.SHARE_REQUIRED_COLUMNS
        )

        df = share_df[
            self.SHARE_REQUIRED_COLUMNS
        ].copy()

        # Ensure a valid DateTime column
        df["DateTime"] = pd.to_datetime(
            df["DateTime"],
            errors="coerce"
        )

        # Convert price and volume columns
        numeric_columns = [
            "Open",
            "High",
            "Low",
            "Close",
            "Volume"
        ]

        for column in numeric_columns:

            df[column] = pd.to_numeric(
                df[column],
                errors="coerce"
            )

        # Remove invalid required values
        df.dropna(
            subset=[
                "DateTime",
                "Open",
                "High",
                "Low",
                "Close",
                "Volume"
            ],
            inplace=True
        )

        # Ensure the requested symbol is used consistently
        df["Symbol"] = str(symbol).strip().upper()

        # Remove invalid prices and volume
        valid_values = (
            (df["Open"] > 0)
            & (df["High"] > 0)
            & (df["Low"] > 0)
            & (df["Close"] > 0)
            & (df["Volume"] >= 0)
        )

        df = df.loc[valid_values].copy()

        # Validate OHLC relationships
        valid_ohlc = (
            (df["High"] >= df["Low"])
            & (df["High"] >= df["Open"])
            & (df["High"] >= df["Close"])
            & (df["Low"] <= df["Open"])
            & (df["Low"] <= df["Close"])
        )

        df = df.loc[valid_ohlc].copy()

        # Remove timezone information if present
        if isinstance(
            df["DateTime"].dtype,
            pd.DatetimeTZDtype
        ):
            df["DateTime"] = (
                df["DateTime"]
                .dt.tz_localize(None)
            )

        # Sort and deduplicate
        df.sort_values(
            "DateTime",
            inplace=True
        )

        df.drop_duplicates(
            subset=["DateTime"],
            keep="last",
            inplace=True
        )

        df.reset_index(
            drop=True,
            inplace=True
        )

        if df.empty:
            raise ValueError(
                f"{symbol} contains no valid share candles"
            )

        return df

    # =====================================================
    # Prepare NIFTY Data
    # =====================================================

    def _prepare_nifty_data(self, nifty_df):

        self._validate_dataframe(
            df=nifty_df,
            dataframe_name="NIFTY 50 data",
            required_columns=self.NIFTY_REQUIRED_COLUMNS
        )

        df = nifty_df[
            self.NIFTY_REQUIRED_COLUMNS
        ].copy()

        # Ensure a valid DateTime column
        df["DateTime"] = pd.to_datetime(
            df["DateTime"],
            errors="coerce"
        )

        nifty_price_columns = [
            "Open",
            "High",
            "Low",
            "Close"
        ]

        for column in nifty_price_columns:

            df[column] = pd.to_numeric(
                df[column],
                errors="coerce"
            )

        df.dropna(
            subset=[
                "DateTime",
                "Open",
                "High",
                "Low",
                "Close"
            ],
            inplace=True
        )

        valid_values = (
            (df["Open"] > 0)
            & (df["High"] > 0)
            & (df["Low"] > 0)
            & (df["Close"] > 0)
        )

        df = df.loc[valid_values].copy()

        valid_ohlc = (
            (df["High"] >= df["Low"])
            & (df["High"] >= df["Open"])
            & (df["High"] >= df["Close"])
            & (df["Low"] <= df["Open"])
            & (df["Low"] <= df["Close"])
        )

        df = df.loc[valid_ohlc].copy()

        # Remove timezone information if present
        if isinstance(
            df["DateTime"].dtype,
            pd.DatetimeTZDtype
        ):
            df["DateTime"] = (
                df["DateTime"]
                .dt.tz_localize(None)
            )

        df.sort_values(
            "DateTime",
            inplace=True
        )

        df.drop_duplicates(
            subset=["DateTime"],
            keep="last",
            inplace=True
        )

        # Rename NIFTY columns before merging
        df.rename(
            columns={
                "Open": "Nifty_Open",
                "High": "Nifty_High",
                "Low": "Nifty_Low",
                "Close": "Nifty_Close"
            },
            inplace=True
        )

        df.reset_index(
            drop=True,
            inplace=True
        )

        if df.empty:
            raise ValueError(
                "NIFTY 50 contains no valid candles"
            )

        return df

    # =====================================================
    # Calculate Alignment Base
    # =====================================================

    @staticmethod
    def _calculate_overlap_counts(
        share_df,
        nifty_df
    ):

        overlap_start = max(
            share_df["DateTime"].min(),
            nifty_df["DateTime"].min()
        )

        overlap_end = min(
            share_df["DateTime"].max(),
            nifty_df["DateTime"].max()
        )

        if overlap_start > overlap_end:
            return {
                "overlap_start": pd.NaT,
                "overlap_end": pd.NaT,
                "share_overlap_candles": 0,
                "nifty_overlap_candles": 0,
                "alignment_base": 0
            }

        share_overlap_count = int(
            share_df["DateTime"]
            .between(
                overlap_start,
                overlap_end,
                inclusive="both"
            )
            .sum()
        )

        nifty_overlap_count = int(
            nifty_df["DateTime"]
            .between(
                overlap_start,
                overlap_end,
                inclusive="both"
            )
            .sum()
        )

        alignment_base = min(
            share_overlap_count,
            nifty_overlap_count
        )

        return {
            "overlap_start": overlap_start,
            "overlap_end": overlap_end,
            "share_overlap_candles": (
                share_overlap_count
            ),
            "nifty_overlap_candles": (
                nifty_overlap_count
            ),
            "alignment_base": alignment_base
        }

    # =====================================================
    # Align One Share with NIFTY
    # =====================================================

    def align_share(
        self,
        symbol,
        share_df,
        prepared_nifty_df
    ):

        symbol = str(symbol).strip().upper()

        share = self._prepare_share_data(
            symbol=symbol,
            share_df=share_df
        )

        overlap = self._calculate_overlap_counts(
            share_df=share,
            nifty_df=prepared_nifty_df
        )

        # Exact inner join
        aligned_df = pd.merge(
            share,
            prepared_nifty_df,
            on="DateTime",
            how="inner",
            validate="one_to_one"
        )

        aligned_df.sort_values(
            "DateTime",
            inplace=True
        )

        aligned_df.drop_duplicates(
            subset=["DateTime"],
            keep="last",
            inplace=True
        )

        aligned_df.reset_index(
            drop=True,
            inplace=True
        )

        matched_candles = len(aligned_df)
        alignment_base = overlap["alignment_base"]

        if alignment_base > 0:

            alignment_ratio = (
                matched_candles / alignment_base
            )

        else:

            alignment_ratio = 0.0

        alignment_ratio = min(
            alignment_ratio,
            1.0
        )

        unmatched_share_candles = max(
            overlap["share_overlap_candles"]
            - matched_candles,
            0
        )

        unmatched_nifty_candles = max(
            overlap["nifty_overlap_candles"]
            - matched_candles,
            0
        )

        status = "ALIGNED"
        rejection_reason = ""

        if matched_candles == 0:

            status = "REJECTED"
            rejection_reason = "NO_MATCHING_TIMESTAMPS"

        elif matched_candles < (
            self.minimum_aligned_candles
        ):

            status = "REJECTED"
            rejection_reason = (
                "INSUFFICIENT_ALIGNED_HISTORY"
            )

        elif alignment_ratio < (
            self.minimum_alignment_ratio
        ):

            status = "REJECTED"
            rejection_reason = "LOW_ALIGNMENT_RATIO"

        report = {
            "Symbol": symbol,
            "Status": status,
            "Rejection_Reason": rejection_reason,
            "Share_Total_Candles": len(share),
            "Nifty_Total_Candles": len(
                prepared_nifty_df
            ),
            "Share_Overlap_Candles": (
                overlap["share_overlap_candles"]
            ),
            "Nifty_Overlap_Candles": (
                overlap["nifty_overlap_candles"]
            ),
            "Matched_Candles": matched_candles,
            "Unmatched_Share_Candles": (
                unmatched_share_candles
            ),
            "Unmatched_Nifty_Candles": (
                unmatched_nifty_candles
            ),
            "Alignment_Ratio": alignment_ratio,
            "Overlap_Start": overlap["overlap_start"],
            "Overlap_End": overlap["overlap_end"],
            "Aligned_Start": (
                aligned_df["DateTime"].min()
                if not aligned_df.empty
                else pd.NaT
            ),
            "Aligned_End": (
                aligned_df["DateTime"].max()
                if not aligned_df.empty
                else pd.NaT
            )
        }

        if status == "REJECTED":
            return None, report

        # Keep columns in a predictable order
        aligned_df = aligned_df[
            self.FINAL_COLUMNS
        ].copy()

        return aligned_df, report

    # =====================================================
    # Align All Shares with NIFTY
    # =====================================================

    def align_all(
        self,
        shares_data,
        nifty_data
    ):

        if not isinstance(shares_data, dict):
            raise TypeError(
                "shares_data must be a dictionary containing "
                "{symbol: DataFrame}"
            )

        if not shares_data:
            raise ValueError(
                "shares_data is empty"
            )

        prepared_nifty = self._prepare_nifty_data(
            nifty_data
        )

        aligned_data = {}
        reports = []

        print("=" * 90)
        print("FMRSS : ALIGNING SHARES WITH NIFTY 50")
        print("=" * 90)

        for symbol, share_df in shares_data.items():

            symbol = str(symbol).strip().upper()

            try:

                aligned_df, report = self.align_share(
                    symbol=symbol,
                    share_df=share_df,
                    prepared_nifty_df=prepared_nifty
                )

                reports.append(report)

                if aligned_df is None:

                    print(
                        f"{symbol:<15}"
                        f"REJECTED : "
                        f"{report['Rejection_Reason']} | "
                        f"Matched="
                        f"{report['Matched_Candles']} | "
                        f"Ratio="
                        f"{report['Alignment_Ratio']:.2%}"
                    )

                    continue

                aligned_data[symbol] = aligned_df

                print(
                    f"{symbol:<15}"
                    f"ALIGNED  : "
                    f"{len(aligned_df)} candles | "
                    f"Ratio="
                    f"{report['Alignment_Ratio']:.2%}"
                )

            except Exception as error:

                reports.append({
                    "Symbol": symbol,
                    "Status": "FAILED",
                    "Rejection_Reason": str(error),
                    "Share_Total_Candles": (
                        len(share_df)
                        if isinstance(
                            share_df,
                            pd.DataFrame
                        )
                        else 0
                    ),
                    "Nifty_Total_Candles": (
                        len(prepared_nifty)
                    ),
                    "Share_Overlap_Candles": 0,
                    "Nifty_Overlap_Candles": 0,
                    "Matched_Candles": 0,
                    "Unmatched_Share_Candles": 0,
                    "Unmatched_Nifty_Candles": 0,
                    "Alignment_Ratio": 0.0,
                    "Overlap_Start": pd.NaT,
                    "Overlap_End": pd.NaT,
                    "Aligned_Start": pd.NaT,
                    "Aligned_End": pd.NaT
                })

                print(
                    f"{symbol:<15}"
                    f"FAILED   : {error}"
                )

        report_df = pd.DataFrame(reports)

        if not report_df.empty:

            report_df.sort_values(
                by=[
                    "Status",
                    "Symbol"
                ],
                inplace=True
            )

            report_df.reset_index(
                drop=True,
                inplace=True
            )

        print("=" * 90)
        print(
            f"Successfully aligned : "
            f"{len(aligned_data)}/{len(shares_data)}"
        )
        print("=" * 90)

        if not aligned_data:
            raise RuntimeError(
                "No shares could be aligned with NIFTY 50"
            )

        return aligned_data, report_df