import os
import pandas as pd
from urllib.parse import unquote

class DataLoader:

    def __init__(
        self,
        symbols_file,
        data_folder,
        nifty_filename="NIFTY%2050_5mins.txt",
        candle_limit=1500
    ):

        self.symbols_file = symbols_file
        self.data_folder = data_folder
        self.nifty_filename = nifty_filename
        self.candle_limit = candle_limit

        self.columns = [
            "Symbol",
            "Date",
            "Time",
            "Open",
            "High",
            "Low",
            "Close",
            "Volume"
        ]

    # =====================================================
    # Read Symbols
    # =====================================================

    def load_symbols(self):

        if not os.path.isfile(self.symbols_file):
            raise FileNotFoundError(
                f"Symbols file not found:\n"
                f"{self.symbols_file}"
            )

        with open(
            self.symbols_file,
            "r",
            encoding="utf-8"
        ) as file:

            symbols = [
                line.strip().upper()
                for line in file
                if line.strip()
            ]

        # Remove duplicate symbols while preserving order
        symbols = list(dict.fromkeys(symbols))

        # NIFTY is used only as the market benchmark.
        # It must not be processed as a tradable share.
        benchmark_symbols = {
            "NIFTY",
            "NIFTY50"
        }

        tradable_symbols = []

        for symbol in symbols:

            # Examples:
            # NIFTY%2050 -> NIFTY50
            # NIFTY 50   -> NIFTY50
            # NIFTY_50   -> NIFTY50
            normalized_symbol = (
                unquote(symbol)
                .replace(" ", "")
                .replace("_", "")
                .upper()
            )

            if normalized_symbol in benchmark_symbols:
                continue

            tradable_symbols.append(symbol)

        print("=" * 70)
        print(
            f"Total Tradable Shares : "
            f"{len(tradable_symbols)}"
        )
        print("=" * 70)

        return tradable_symbols

    # =====================================================
    # Generic Five-Minute File Reader
    # =====================================================

    def _load_five_minute_file(
        self,
        filename,
        expected_symbol=None,
        require_volume=True
    ):

        if not os.path.isfile(filename):
            raise FileNotFoundError(
                f"Five-minute file not found:\n{filename}"
            )

        df = pd.read_csv(
            filename,
            names=self.columns,
            header=None,
            skiprows=1,
            low_memory=False
        )

        # Remove completely blank rows
        df.dropna(how="all", inplace=True)

        # Clean column names stored as text
        for column in ["Symbol", "Date", "Time"]:

            df[column] = (
                df[column]
                .astype("string")
                .str.strip()
            )

        # Create DateTime
        df["DateTime"] = pd.to_datetime(
            df["Date"] + " " + df["Time"],
            errors="coerce"
        )

        # Convert OHLC columns
        ohlc_columns = [
            "Open",
            "High",
            "Low",
            "Close"
        ]

        for column in ohlc_columns:

            df[column] = pd.to_numeric(
                df[column],
                errors="coerce"
            )

        # Volume is not required for NIFTY
        df["Volume"] = pd.to_numeric(
            df["Volume"],
            errors="coerce"
        )

        required_columns = [
            "DateTime",
            "Open",
            "High",
            "Low",
            "Close"
        ]

        if require_volume:
            required_columns.append("Volume")

        # Remove invalid rows
        df.dropna(
            subset=required_columns,
            inplace=True
        )

        # NIFTY volume may be unavailable
        if not require_volume:
            df["Volume"] = df["Volume"].fillna(0)

        # Remove invalid prices
        valid_prices = (
            (df["Open"] > 0)
            & (df["High"] > 0)
            & (df["Low"] > 0)
            & (df["Close"] > 0)
        )

        df = df.loc[valid_prices].copy()

        # Validate OHLC relationships
        valid_ohlc = (
            (df["High"] >= df["Low"])
            & (df["High"] >= df["Open"])
            & (df["High"] >= df["Close"])
            & (df["Low"] <= df["Open"])
            & (df["Low"] <= df["Close"])
        )

        df = df.loc[valid_ohlc].copy()

        # Use a consistent symbol value
        if expected_symbol is not None:
            df["Symbol"] = expected_symbol

        # Sort oldest to newest
        df.sort_values(
            "DateTime",
            inplace=True
        )

        # Keep the last version of duplicate candles
        df.drop_duplicates(
            subset=["DateTime"],
            keep="last",
            inplace=True
        )

        # Keep the latest required history
        if self.candle_limit is not None:

            df = df.tail(
                self.candle_limit
            ).copy()

        df.reset_index(
            drop=True,
            inplace=True
        )

        return df

    # =====================================================
    # Load One Share's Five-Minute Data
    # =====================================================

    def load_share(self, symbol):

        symbol = symbol.strip().upper()

        filename = os.path.join(
            self.data_folder,
            f"{symbol}_5mins.txt"
        )

        return self._load_five_minute_file(
            filename=filename,
            expected_symbol=symbol,
            require_volume=True
        )

    # =====================================================
    # Load NIFTY 50 Five-Minute Data
    # =====================================================

    def load_nifty(self):

        filename = os.path.join(
            self.data_folder,
            self.nifty_filename
        )

        df = self._load_five_minute_file(
            filename=filename,
            expected_symbol="NIFTY 50",
            require_volume=False
        )

        if df.empty:
            raise ValueError(
                "NIFTY 50 file contains no valid candles"
            )
        '''
        print(
            f"{'NIFTY 50':<15}"
            f"LOADED : {len(df)} candles | "
            f"{df['DateTime'].iloc[0]} → "
            f"{df['DateTime'].iloc[-1]}"
        )
        '''
        return df

    # =====================================================
    # Load All Shares
    # =====================================================

    def load_all_shares(self):

        symbols = self.load_symbols()
        all_data = {}

        for symbol in symbols:

            try:

                df = self.load_share(symbol)

                if df.empty:

                    print(
                        f"{symbol:<15}"
                        f"FAILED : No valid candles"
                    )

                    continue

                all_data[symbol] = df
                '''
                print(
                    f"{symbol:<15}"
                    f"LOADED : {len(df)} candles"
                )
                '''
            except Exception as error:

                print(
                    f"{symbol:<15}"
                    f"FAILED : {error}"
                )

        #print("=" * 70)
        #print(
        #    f"Successfully loaded shares : "
        #    f"{len(all_data)}/{len(symbols)}"

        #)
        #print("=" * 70)

        return all_data

    # =====================================================
    # Load Complete FMRSS Market Data
    # =====================================================

    def load_market_data(self):

        nifty_data = self.load_nifty()
        shares_data = self.load_all_shares()

        if not shares_data:
            raise RuntimeError(
                "No share data could be loaded"
            )

        return shares_data, nifty_data
