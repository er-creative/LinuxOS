import pandas as pd
# ==========================================
# Module 2 : Indicator Engine
# ==========================================

class IndicatorEngine:

    def __init__(self):

        self.ema_fast = 20
        self.ema_medium = 50
        self.ema_slow = 200

        self.rsi_period = 14

        self.macd_fast = 12
        self.macd_slow = 26
        self.macd_signal = 9
        self.atr_period = 14

        print("Indicator Engine Initialized")

    # =====================================================
    # EMA
    # =====================================================

    def calculate_ema(self, df):

        df["EMA20"] = (
            df["Close"]
            .ewm(span=self.ema_fast, adjust=False)
            .mean()
        )

        df["EMA50"] = (
            df["Close"]
            .ewm(span=self.ema_medium, adjust=False)
            .mean()
        )

        df["EMA200"] = (
            df["Close"]
            .ewm(span=self.ema_slow, adjust=False)
            .mean()
        )

        return df

    # =====================================================
    # RSI (Wilder's RSI)
    # =====================================================
    
    def calculate_rsi(self, df):
    
        delta = df["Close"].diff()
    
        gain = delta.clip(lower=0)
    
        loss = -delta.clip(upper=0)
    
        # Wilder's Moving Average (RMA)
        avg_gain = (
            gain
            .ewm(
                alpha=1 / self.rsi_period,
                adjust=False,
                min_periods=self.rsi_period
            )
            .mean()
        )
    
        avg_loss = (
            loss
            .ewm(
                alpha=1 / self.rsi_period,
                adjust=False,
                min_periods=self.rsi_period
            )
            .mean()
        )
    
        rs = avg_gain / avg_loss.replace(0, 1e-10)
    
        df["RSI"] = 100 - (100 / (1 + rs))
    
        # Optional helper columns
        df["RSI_BULLISH"] = df["RSI"] > 50
        df["RSI_BEARISH"] = df["RSI"] < 50
    
        df["RSI_OVERBOUGHT"] = df["RSI"] >= 70
        df["RSI_OVERSOLD"] = df["RSI"] <= 30
    
        return df    
    # =====================================================
    # MACD
    # =====================================================

    def calculate_macd(self, df):

        ema12 = (
            df["Close"]
            .ewm(span=self.macd_fast, adjust=False)
            .mean()
        )

        ema26 = (
            df["Close"]
            .ewm(span=self.macd_slow, adjust=False)
            .mean()
        )

        df["MACD"] = ema12 - ema26

        df["MACD_SIGNAL"] = (
            df["MACD"]
            .ewm(span=self.macd_signal, adjust=False)
            .mean()
        )

        df["MACD_HIST"] = (
            df["MACD"]
            - df["MACD_SIGNAL"]
        )

        return df


    # =====================================================
    # ATR (14)
    # =====================================================
    
    def calculate_atr(self, df):
    
        high_low = df["High"] - df["Low"]
    
        high_close = abs(df["High"] - df["Close"].shift(1))
    
        low_close = abs(df["Low"] - df["Close"].shift(1))
    
        tr = pd.concat(
            [high_low, high_close, low_close],
            axis=1
        ).max(axis=1)
    

            
        df["ATR"] = (
            tr
            .ewm(
                alpha=1 / self.atr_period,
                adjust=False,
                min_periods=self.atr_period
            )
            .mean()
        )
        # ATR Moving Average
        df["ATR_MA"] = (
            df["ATR"]
            .rolling(self.atr_period)
            .mean()
        )
    
        return df

    # =====================================================
    # ADX (Wilder's Method)
    # =====================================================
    
    def calculate_adx(self, df):
    
        period = self.atr_period
    
        high = df["High"]
        low = df["Low"]
        close = df["Close"]
    
        # -------------------------------------------------
        # Directional Movement
        # -------------------------------------------------
    
        up_move = high.diff()
    
        down_move = -low.diff()
    
        plus_dm = up_move.where(
            (up_move > down_move) & (up_move > 0),
            0.0
        )
    
        minus_dm = down_move.where(
            (down_move > up_move) & (down_move > 0),
            0.0
        )
    
        # -------------------------------------------------
        # True Range
        # -------------------------------------------------
    
        tr1 = high - low
    
        tr2 = (high - close.shift()).abs()
    
        tr3 = (low - close.shift()).abs()
    
        tr = pd.concat(
            [tr1, tr2, tr3],
            axis=1
        ).max(axis=1)
    
        # -------------------------------------------------
        # Wilder ATR
        # -------------------------------------------------
    
        atr = (
            tr
            .ewm(
                alpha=1 / period,
                adjust=False
            )
            .mean()
        )
    
        # -------------------------------------------------
        # Wilder Smoothed Directional Movement
        # -------------------------------------------------
    
        plus_dm_smoothed = (
            plus_dm
            .ewm(
                alpha=1 / period,
                adjust=False
            )
            .mean()
        )
    
        minus_dm_smoothed = (
            minus_dm
            .ewm(
                alpha=1 / period,
                adjust=False
            )
            .mean()
        )
    
        # -------------------------------------------------
        # Directional Indicators
        # -------------------------------------------------
    
        plus_di = 100 * (plus_dm_smoothed / atr)
    
        minus_di = 100 * (minus_dm_smoothed / atr)
    
        # -------------------------------------------------
        # DX
        # -------------------------------------------------
    
        dx = (
            (
                (plus_di - minus_di).abs()
                /
                (plus_di + minus_di).replace(0, 1e-10)
            )
            * 100
        )
    
        # -------------------------------------------------
        # ADX (Wilder)
        # -------------------------------------------------
    
        adx = (
            dx
            .ewm(
                alpha=1 / period,
                adjust=False
            )
            .mean()
        )
    
        # -------------------------------------------------
        # Save
        # -------------------------------------------------
    
        df["+DI"] = plus_di
    
        df["-DI"] = minus_di
    
        df["ADX"] = adx
    
        return df
    
    # =====================================================
    # Price Position (Daily)
    # =====================================================
    
    def calculate_vwap(self, df):
    
        """
        Daily timeframe replacement for VWAP.
    
        Since cumulative VWAP is meaningful only for intraday
        data, we compare price with EMA20 instead.
        """
    
        if "EMA20" not in df.columns:
            raise ValueError("EMA20 must be calculated before Price Position.")
    
        df["VWAP"] = df["EMA20"]
    
        df["ABOVE_VWAP"] = (
            df["Close"] > df["EMA20"]
        )
    
        return df

    # =====================================================
    # Bollinger Bands
    # =====================================================
    
    def calculate_bollinger(self, df):
    
        df["BB_MIDDLE"] = (
            df["Close"]
            .rolling(20)
            .mean()
        )
    
        std = (
            df["Close"]
            .rolling(20)
            .std(ddof=0)
        )
    
        df["BB_UPPER"] = (
            df["BB_MIDDLE"] +
            2 * std
        )
    
        df["BB_LOWER"] = (
            df["BB_MIDDLE"] -
            2 * std
        )
    
        return df


    # =====================================================
    # Volume MA
    # =====================================================
        
    # =====================================================
    # Volume Analysis
    # =====================================================

    def calculate_volume(self, df):

        # 20-Day Average Volume
        df["VOLUME_MA"] = (
            df["Volume"]
            .rolling(20)
            .mean()
        )

        # High Volume (50% above average)
        df["HIGH_VOLUME"] = (
            df["Volume"] >=
            (df["VOLUME_MA"] * 1.5)
        )

        # Low Volume
        df["LOW_VOLUME"] = (
            df["Volume"] <=
            (df["VOLUME_MA"] * 0.7)
        )

        # Relative Volume (RVOL)
        df["RVOL"] = (
            df["Volume"] /
            df["VOLUME_MA"]
        )

        return df

    # =====================================================
    # Market Structure (Swing Based)
    # =====================================================
    
    def calculate_market_structure(self, df, window=3):
    
        highs = df["High"]
        lows = df["Low"]
    
        df["Swing_High"] = False
        df["Swing_Low"] = False
    
        # -----------------------------
        # Detect Swing High / Low
        # -----------------------------
        for i in range(window, len(df) - window):
    
            if highs.iloc[i] == highs.iloc[i-window:i+window+1].max():
                df.loc[df.index[i], "Swing_High"] = True
    
            if lows.iloc[i] == lows.iloc[i-window:i+window+1].min():
                df.loc[df.index[i], "Swing_Low"] = True
    
        # -----------------------------
        # Initialize
        # -----------------------------
        df["Higher_High"] = False
        df["Higher_Low"] = False
        df["Lower_High"] = False
        df["Lower_Low"] = False
    
        last_swing_high = None
        last_swing_low = None
    
        # -----------------------------
        # Compare Swings
        # -----------------------------
        for i in range(len(df)):
    
            if df.iloc[i]["Swing_High"]:
    
                current = highs.iloc[i]
    
                if last_swing_high is not None:
    
                    if current > last_swing_high:
                        df.loc[df.index[i], "Higher_High"] = True
    
                    elif current < last_swing_high:
                        df.loc[df.index[i], "Lower_High"] = True
    
                last_swing_high = current
    
            if df.iloc[i]["Swing_Low"]:
    
                current = lows.iloc[i]
    
                if last_swing_low is not None:
    
                    if current > last_swing_low:
                        df.loc[df.index[i], "Higher_Low"] = True
    
                    elif current < last_swing_low:
                        df.loc[df.index[i], "Lower_Low"] = True
    
                last_swing_low = current
    
        return df

    # =====================================================
    # Validate Data
    # =====================================================
    
    def validate(self, df):
    
        required = [
            "Open",
            "High",
            "Low",
            "Close",
            "Volume"
        ]
    
        missing = [
            col
            for col in required
            if col not in df.columns
        ]
    
        if missing:
    
            raise ValueError(
                f"Missing Columns : {missing}"
            )
    
        if len(df) < 200:
    
            raise ValueError(
                "Need minimum 200 candles."
            )


    

    def calculate(self, df):
    
        df = df.copy()
    
        self.validate(df)
    
        df = self.calculate_ema(df)
    
        df = self.calculate_rsi(df)
    
        df = self.calculate_macd(df)
    
        df = self.calculate_atr(df)
    
        df = self.calculate_adx(df)
    
        df = self.calculate_vwap(df)
    
        df = self.calculate_bollinger(df)
    
        df = self.calculate_volume(df)
    
        df = self.calculate_market_structure(df)
    
        return df
