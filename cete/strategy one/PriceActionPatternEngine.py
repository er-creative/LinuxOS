# ==========================================================
# Module 5 : Price Action Pattern Engine
#
# ==========================================================

import pandas as pd
import numpy as np
import config


class PriceActionPatternEngine:
    
    # ======================================================
    # Constructor
    # ======================================================
    
    def __init__(
        self,
        swing_window=5,
        breakout_lookback=20,
        volume_multiplier=1.20,
        pattern_tolerance=0.02,
        max_pattern_bars=80,
        neckline_confirmation=2,
        minimum_pattern_bars=None
     
    ):
    ########2. Constructor Settings######
        # -----------------------------------------
        # Swing Detection
        # -----------------------------------------
    
        self.swing_window = swing_window
    
        # -----------------------------------------
        # Breakout / Breakdown
        # -----------------------------------------
    
        self.breakout_lookback = breakout_lookback
    
        self.volume_multiplier = volume_multiplier
    
        # -----------------------------------------
        # Double Top / Double Bottom
        # -----------------------------------------
    
        # Maximum percentage difference
        # allowed between two peaks/bottoms
        self.pattern_tolerance = pattern_tolerance
    
        # Maximum candles between two peaks
        self.max_pattern_bars = max_pattern_bars
    
        # Minimum candles between two peaks
        if minimum_pattern_bars is None:
    
            self.minimum_pattern_bars = (
                self.swing_window * 2
            )
    
        else:
    
            self.minimum_pattern_bars = (
                minimum_pattern_bars
            )
    
        # Number of consecutive closes required
        # below neckline (Double Top)
        # or above neckline (Double Bottom)
        self.neckline_confirmation = (
            neckline_confirmation
        )

        # ======================================================
        # Pattern Score Weights
        # ======================================================
        
        self.pattern_weights = {
        
            "quality": 0.35,
        
            "breakout": 0.25,
        
            "volume": 0.20,
        
            "trend": 0.20
        
        }
        print("Price Action Pattern Engine initialize")
    #├── 3. Basic validation / utility helpers#########
    # ------------------------------------------------------
    # Validate Data
    # ------------------------------------------------------

    def validate(self, df):

        required_columns = [

            "High",

            "Low",

            "Close"

        ]

        for col in required_columns:

            if col not in df.columns:

                raise ValueError(
                    f"Missing column : {col}"
                )

        minimum_rows = (

            self.swing_window * 2

        ) + 1

        if len(df) < minimum_rows:

            raise ValueError(

                f"Need at least {minimum_rows} candles."

            )

        return True


    # ======================================================
    # Check if Two Prices are Within Tolerance
    # ======================================================

    def _within_tolerance(
        self,
        price1,
        price2,
        tolerance=None
    ):

        """
        Check whether two prices are close enough.

        Parameters
        ----------
        price1 : float

        price2 : float

        tolerance : float, optional

            Maximum allowed percentage difference.

            Default:
                self.pattern_tolerance

        Returns
        -------
        bool

        Example
        -------

        price1 = 500

        price2 = 505

        tolerance = 0.02   # 2%

        Difference = 5

        Average Price = 502.5

        Percentage Difference = 5 / 502.5 = 0.00995

        Result = True
        """

        # --------------------------------------------------
        # Default Tolerance
        # --------------------------------------------------

        if tolerance is None:

            tolerance = self.pattern_tolerance

        # --------------------------------------------------
        # Validate Prices
        # --------------------------------------------------

        if price1 <= 0 or price2 <= 0:

            return False

        # --------------------------------------------------
        # Calculate Percentage Difference
        # --------------------------------------------------

        difference = abs(price1 - price2)

        average_price = (price1 + price2) / 2

        if average_price == 0:

            return False

        percentage = difference / average_price

        # --------------------------------------------------
        # Compare Against Tolerance
        # --------------------------------------------------

        return percentage <= tolerance
        
        # ======================================================
        # Confidence Weights
        # ======================================================
        
        self.confidence_weights = {
        
            "geometry": 0.30,
        
            "volume": 0.20,
        
            "breakout": 0.20,
        
            "trend": 0.15,
        
            "momentum": 0.15
        
        }
    
        print(
            "Price Action Pattern Engine Initialized"
        )
    ##########################3##
    #├── 4. Swing-point helpers
    ###########################
    # ------------------------------------------------------
    # Detect Swing Highs
    # ------------------------------------------------------

    def detect_swing_highs(self, df):

        """
        Returns a list of swing highs.

        Example

        [
            {
                "index": 125,
                "price": 512.45
            },
            ...
        ]

        """

        highs = []

        w = self.swing_window

        for i in range(

            w,

            len(df) - w

        ):

            current = df.iloc[i]["High"]

            left = df.iloc[
                i-w:i
            ]["High"]

            right = df.iloc[
                i+1:i+w+1
            ]["High"]

            if (

                current > left.max()

                and

                current >= right.max()

            ):

                highs.append({

                    "index": i,

                    "price": current

                })

        return highs


    # ------------------------------------------------------
    # Detect Swing Lows
    # ------------------------------------------------------

    def detect_swing_lows(self, df):

        """
        Returns a list of swing lows.

        Example

        [
            {
                "index": 102,
                "price": 485.60
            }
        ]

        """

        lows = []

        w = self.swing_window

        for i in range(

            w,

            len(df) - w

        ):

            current = df.iloc[i]["Low"]

            left = df.iloc[
                i-w:i
            ]["Low"]

            right = df.iloc[
                i+1:i+w+1
            ]["Low"]

            if (

                current <= left.min()

                and

                current <= right.min()

            ):

                lows.append({

                    "index": i,

                    "price": current

                })

        return lows


    # ======================================================
    # Find Recent Peaks
    # ======================================================
    
    def _find_recent_peaks(
        self,
        df,
        count=5
    ):
    
        """
        Returns the latest swing highs.
    
        Example
    
        [
            {
                "index":120,
                "price":512.35
            },
            ...
        ]
        """
    
        swing_highs = self.detect_swing_highs(df)
    
        if len(swing_highs) == 0:
    
            return []
    
        recent = sorted(
            swing_highs,
            key=lambda x: x["index"]
        )[-count:]
    
        return recent   


    # ======================================================
    # Find Recent Troughs
    # ======================================================
    
    def _find_recent_troughs(
        self,
        df,
        count=5
    ):
    
        """
        Returns the latest swing lows.
    
        Example
    
        [
            {
                "index":105,
                "price":488.50
            }
        ]
        """
    
        swing_lows = self.detect_swing_lows(df)
    
        if len(swing_lows) == 0:
    
            return []
    
        recent = swing_lows[-count:]
    
        return recent


    
    # ======================================================
    # ├── 5. Pattern utility helpers
    # ======================================================



    # ======================================================
    # Find Neckline
    # ======================================================

    def _find_neckline(
        self,
        df,
        left_index,
        right_index,
        pattern="double_top"
    ):

        """
        Find neckline between two swing points.

        Parameters
        ----------
        df : DataFrame

        left_index : int
            Position index of first swing point.

        right_index : int
            Position index of second swing point.

        pattern : str
            "double_top"
            "double_bottom"

        Returns
        -------
        dict

        Example
        -------
        {
            "index": 125,
            "price": 482.50
        }
        """

        # --------------------------------------------------
        # Validation
        # --------------------------------------------------

        if left_index is None or right_index is None:

            return None

        if left_index >= right_index:

            return None

        if left_index < 0:

            return None

        if right_index >= len(df):

            return None

        # --------------------------------------------------
        # Extract Pattern Section
        # --------------------------------------------------

        section = df.iloc[
            left_index:right_index + 1
        ].copy()

        if section.empty:

            return None

        # --------------------------------------------------
        # Double Top
        # Neckline = Lowest Low
        # --------------------------------------------------

        if pattern == "double_top":

            if "Low" not in section.columns:

                return None

            section = section.dropna(
                subset=["Low"]
            )

            if section.empty:

                return None

            neckline_index = section["Low"].idxmin()

            return {

                "index": neckline_index,

                "price": float(
                    section.loc[
                        neckline_index,
                        "Low"
                    ]
                )

            }

        # --------------------------------------------------
        # Double Bottom
        # Neckline = Highest High
        # --------------------------------------------------

        elif pattern == "double_bottom":

            if "High" not in section.columns:

                return None

            section = section.dropna(
                subset=["High"]
            )

            if section.empty:

                return None

            neckline_index = section["High"].idxmax()

            return {

                "index": neckline_index,

                "price": float(
                    section.loc[
                        neckline_index,
                        "High"
                    ]
                )

            }

        # --------------------------------------------------
        # Unsupported Pattern
        # --------------------------------------------------

        return None

        
    # ======================================================
    # Detect Trendline
    # ======================================================
    
    def _calculate_trendline(self, points):
    
        """
        Calculates slope and intercept from swing points.
    
        Parameters
        ----------
        points : list
    
            [
                {"index":10,"price":100},
                {"index":25,"price":110},
                ...
            ]
    
        Returns
        -------
        {
            "slope": ...,
            "intercept": ...
        }
    
        """
    
        if len(points) < 2:
            return None
    
        x = np.array(
            [p["index"] for p in points]
        )
    
        y = np.array(
            [p["price"] for p in points]
        )
    
        slope, intercept = np.polyfit(
            x,
            y,
            1
        )
    
        return {
    
            "slope": slope,
    
            "intercept": intercept
    
        }

    # ======================================================
    # Generate Valid Pattern Pairs
    # ======================================================

    def _generate_pattern_pairs(
        self,
        swing_points
    ):

        """
        Generate all valid swing-point pairs.

        Used By
        -------
        • Double Top
        • Double Bottom
        • Future two-point patterns

        Conditions
        ----------
        1. Minimum separation = minimum_pattern_bars
        2. Maximum separation = max_pattern_bars

        Parameters
        ----------
        swing_points : list

            Example

            [
                {"index": 25, "price": 520},
                {"index": 48, "price": 518},
                {"index": 70, "price": 522}
            ]

        Returns
        -------
        list

            [
                (point1, point2),
                (point1, point3),
                (point2, point3)
            ]
        """

        # --------------------------------------------------
        # Validation
        # --------------------------------------------------

        if not swing_points:

            return []

        if len(swing_points) < 2:

            return []

        pairs = []

        total_points = len(swing_points)

        # --------------------------------------------------
        # Generate Pairs
        # --------------------------------------------------

        for i in range(total_points - 1):

            left_point = swing_points[i]

            left_index = left_point["index"]

            for j in range(i + 1, total_points):

                right_point = swing_points[j]

                right_index = right_point["index"]

                # ------------------------------------------
                # Invalid Order
                # ------------------------------------------

                if right_index <= left_index:

                    continue

                bars_between = right_index - left_index

                # ------------------------------------------
                # Too Close
                # ------------------------------------------

                if bars_between < self.minimum_pattern_bars:

                    continue

                # ------------------------------------------
                # Too Far
                # Since swing points are chronological,
                # remaining points will also exceed limit.
                # ------------------------------------------

                if bars_between > self.max_pattern_bars:

                    break

                # ------------------------------------------
                # Valid Pair
                # ------------------------------------------

                pairs.append(

                    (
                        left_point,
                        right_point
                    )

                )

        return pairs

    # ==========================================================
    # ├── 6. Market structure
    # ==========================================================

    # ==========================================================
    # Detect Higher High Higher Low
    # ==========================================================
    
    def detect_higher_high_higher_low(self, df):
    
        """
        Detect bullish market structure.
    
        Returns
        -------
        {
            "Detected": bool,
            "Pattern": "Higher High Higher Low",
            "Bullish": bool,
            "Score": int,
            "Reasons": list
        }
        """
    
        highs = self.detect_swing_highs(df)
    
        lows = self.detect_swing_lows(df)
    
        if len(highs) < 2 or len(lows) < 2:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bullish": False,
    
                "Score": 0,
    
                "Reasons": []
    
            }
    
        last_high = highs[-1]["price"]
        previous_high = highs[-2]["price"]
    
        last_low = lows[-1]["price"]
        previous_low = lows[-2]["price"]
    
        if (
    
            last_high > previous_high
    
            and
    
            last_low > previous_low
    
        ):
    
            return {
    
                "Detected": True,
    
                "Pattern": "Higher High Higher Low",
    
                "Bullish": True,
    
                "Score": config.HHHL_SCORE,
    
                "Reasons": [
    
                    "Higher High",
    
                    "Higher Low"
    
                ]
    
            }
    
        return {
    
            "Detected": False,
    
            "Pattern": None,
    
            "Bullish": False,
    
            "Score": 0,
    
            "Reasons": []
    
        }
    
    
    # ==========================================================
    # Detect Lower High Lower Low
    # ==========================================================
    
    def detect_lower_high_lower_low(self, df):
    
        """
        Detect bearish market structure.
        """
    
        highs = self.detect_swing_highs(df)
    
        lows = self.detect_swing_lows(df)
    
        if len(highs) < 2 or len(lows) < 2:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bearish": False,
    
                "Score": 0,
    
                "Reasons": []
    
            }
    
        last_high = highs[-1]["price"]
        previous_high = highs[-2]["price"]
    
        last_low = lows[-1]["price"]
        previous_low = lows[-2]["price"]
    
        if (
    
            last_high < previous_high
    
            and
    
            last_low < previous_low
    
        ):
    
            return {
    
                "Detected": True,
    
                "Pattern": "Lower High Lower Low",
    
                "Bearish": True,
    
                "Score": -config.LHLL_SCORE,
    
                "Reasons": [
    
                    "Lower High",
    
                    "Lower Low"
    
                ]
    
            }
    
        return {
    
            "Detected": False,
    
            "Pattern": None,
    
            "Bearish": False,
    
            "Score": 0,
    
            "Reasons": []
    
        }
    # ==========================================================
    # ├── 7. Breakout / Breakdown
    # ==========================================================        

    # ==========================================================
    # Detect Breakout
    # ==========================================================


    def detect_breakout(self, df):
    
        """
        Detect Bullish Breakout
    
        Conditions
        ----------
        1. Latest Close > Latest Swing High
        2. Bullish Candle
        3. Volume Confirmation
    
        Returns
        -------
        Dictionary
        """
    
        self.validate(df)
    
        # --------------------------------------------------
        # Need enough candles
        # --------------------------------------------------
    
        if len(df) < self.breakout_lookback:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bullish": False,
    
                "Score": 0,
    
                "Resistance": None,
    
                "Breakout Strength": 0,
    
                "Reasons": [
    
                    "Insufficient Data"
    
                ]
    
            }
    
        # --------------------------------------------------
        # Find Swing Highs
        # --------------------------------------------------
    
        swing_highs = self.detect_swing_highs(df)
    
        if len(swing_highs) == 0:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bullish": False,
    
                "Score": 0,
    
                "Resistance": None,
    
                "Breakout Strength": 0,
    
                "Reasons": [
    
                    "No Swing High Found"
    
                ]
    
            }
    
        # --------------------------------------------------
        # Ignore Current Candle Swing High
        # --------------------------------------------------
    
        latest_bar = len(df) - 1
    
        valid_highs = [
    
            high
    
            for high in swing_highs
    
            if high["index"] < latest_bar
    
        ]
    
        if len(valid_highs) == 0:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bullish": False,
    
                "Score": 0,
    
                "Resistance": None,
    
                "Breakout Strength": 0,
    
                "Reasons": [
    
                    "No Completed Swing High"
    
                ]
    
            }
    
        # --------------------------------------------------
        # Latest Resistance
        # --------------------------------------------------
    
        latest_swing = valid_highs[-1]
    
        resistance = latest_swing["price"]
    
        resistance_index = latest_swing["index"]
    
        # --------------------------------------------------
        # Latest Candle
        # --------------------------------------------------
    
        latest = df.iloc[-1]
    
        close = latest["Close"]
    
        open_price = latest["Open"]
    
        volume = latest["Volume"]
    
        volume_ma = latest["VOLUME_MA"]
    
        reasons = []
    
        score = 0
    
        detected = True
    
        # --------------------------------------------------
        # Breakout Confirmation
        # --------------------------------------------------
    
        if close > resistance:
    
            score += config.BREAKOUT_SCORE
    
            reasons.append(
    
                "Close Above Swing High"
    
            )
    
        else:
    
            detected = False
    
        # --------------------------------------------------
        # Bullish Candle
        # --------------------------------------------------
    
        if close > open_price:
    
            score += 5
    
            reasons.append(
    
                "Bullish Candle"
    
            )
    
        else:
    
            detected = False
    
        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------
    
        if volume >= (
    
            volume_ma *
    
            self.volume_multiplier
    
        ):
    
            score += 5
    
            reasons.append(
    
                "High Volume"
    
            )
    
        else:
    
            reasons.append(
    
                "Weak Volume"
    
            )
    
        # --------------------------------------------------
        # Breakout Strength
        # --------------------------------------------------
    
        breakout_strength = (
    
            (
    
                close -
    
                resistance
    
            )
    
            /
    
            resistance
    
        ) * 100
    
        # --------------------------------------------------
        # Final Result
        # --------------------------------------------------
    
        if detected:
    
            confidence = min(

                100,
            
                score * 4
            
            )
            
            return self.create_pattern_result(
            
                detected=True,
            
                pattern="Breakout",
            
                bullish=True,
            
                bearish=False,
            
                score=score,
            
                confidence=confidence,
            
                reasons=reasons,
            
                details={
            
                    "Resistance": resistance,
            
                    "Breakout Strength":
                        breakout_strength,
            
                    "Breakout Price": close
            
                }
            
            )
    
        return {
    
            "Detected": False,
    
            "Pattern": None,
    
            "Bullish": False,
    
            "Score": 0,
    
            "Resistance": round(
    
                resistance,
    
                2
    
            ),
    
            "Resistance Index": resistance_index,
    
            "Breakout Price": round(
    
                close,
    
                2
    
            ),
    
            "Breakout Strength": round(
    
                breakout_strength,
    
                2
    
            ),
    
            "Reasons": reasons
    
        }
    # ==========================================================
    # Detect Breakdown
    # ==========================================================
    
    def detect_breakdown(self, df):
    
        """
        Detect Bearish Breakdown
    
        Conditions
        ----------
        1. Close < Latest Swing Low
        2. Bearish Candle
        3. Volume Confirmation
    
        Returns
        -------
        Dictionary
        """
    
        self.validate(df)
    
        # --------------------------------------------------
        # Need enough candles
        # --------------------------------------------------
    
        if len(df) < self.breakout_lookback:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bearish": False,
    
                "Score": 0,
    
                "Support": None,
    
                "Breakdown Strength": 0,
    
                "Reasons": [
    
                    "Insufficient Data"
    
                ]
    
            }
    
        # --------------------------------------------------
        # Find Swing Lows
        # --------------------------------------------------
    
        swing_lows = self.detect_swing_lows(df)
    
        if len(swing_lows) == 0:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bearish": False,
    
                "Score": 0,
    
                "Support": None,
    
                "Breakdown Strength": 0,
    
                "Reasons": [
    
                    "No Swing Low Found"
    
                ]
    
            }
    
        # --------------------------------------------------
        # Ignore Current Candle Swing Low
        # --------------------------------------------------
    
        latest_bar = len(df) - 1
    
        valid_lows = [
    
            low
    
            for low in swing_lows
    
            if low["index"] < latest_bar
    
        ]
    
        if len(valid_lows) == 0:
    
            return {
    
                "Detected": False,
    
                "Pattern": None,
    
                "Bearish": False,
    
                "Score": 0,
    
                "Support": None,
    
                "Breakdown Strength": 0,
    
                "Reasons": [
    
                    "No Completed Swing Low"
    
                ]
    
            }
    
        # --------------------------------------------------
        # Latest Support
        # --------------------------------------------------
    
        latest_swing = valid_lows[-1]
    
        support = latest_swing["price"]
    
        support_index = latest_swing["index"]
    
        # --------------------------------------------------
        # Latest Candle
        # --------------------------------------------------
    
        latest = df.iloc[-1]
    
        close = latest["Close"]
    
        open_price = latest["Open"]
    
        volume = latest["Volume"]
    
        volume_ma = latest["VOLUME_MA"]
    
        reasons = []
    
        score = 0
    
        detected = True
    
        # --------------------------------------------------
        # Close Below Support
        # --------------------------------------------------
    
        if close < support:
    
            score += 10
    
            reasons.append(
    
                "Close Below Swing Low"
    
            )
    
        else:
    
            detected = False
    
        # --------------------------------------------------
        # Bearish Candle
        # --------------------------------------------------
    
        if close < open_price:
    
            score += 5
    
            reasons.append(
    
                "Bearish Candle"
    
            )
    
        else:
    
            detected = False
    
        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------
    
        if volume >= (
    
            volume_ma *
    
            self.volume_multiplier
    
        ):
    
            score += 5
    
            reasons.append(
    
                "High Volume"
    
            )
    
        else:
    
            reasons.append(
    
                "Weak Volume"
    
            )
    
        # --------------------------------------------------
        # Breakdown Strength
        # --------------------------------------------------
    
        breakdown_strength = (
    
            (
    
                support -
    
                close
    
            )
    
            /
    
            support
    
        ) * 100
    
        # --------------------------------------------------
        # Final Result
        # --------------------------------------------------
    
        if detected:
    
            return {
    
                "Detected": True,
    
                "Pattern": "Breakdown",
    
                "Bearish": True,
    
                "Score": score,
    
                "Support": round(
    
                    support,
    
                    2
    
                ),
    
                "Support Index": support_index,
    
                "Breakdown Price": round(
    
                    close,
    
                    2
    
                ),
    
                "Breakdown Strength": round(
    
                    breakdown_strength,
    
                    2
    
                ),
    
                "Reasons": reasons
    
            }
    
        return {
    
            "Detected": False,
    
            "Pattern": None,
    
            "Bearish": False,
    
            "Score": 0,
    
            "Support": round(
    
                support,
    
                2
    
            ),
    
            "Support Index": support_index,
    
            "Breakdown Price": round(
    
                close,
    
                2
    
            ),
    
            "Breakdown Strength": round(
    
                breakdown_strength,
    
                2
    
            ),
    
            "Reasons": reasons
    
        }
    # ==========================================================
    #├── 8. Classical reversal patterns
    # ==========================================================   
    # ======================================================
    # Detect Double Top
    # ======================================================

    def detect_double_top(self, df):

        """
        Detect Double Top Pattern

        Daily Timeframe

        Confirmations:
            ✓ Similar Peaks
            ✓ Neckline
            ✓ Neckline Breakdown
            ✓ High Volume
            ✓ Bearish Breakdown Candle
        """

        # --------------------------------------------------
        # Validation
        # --------------------------------------------------

        self.validate(df)

        # --------------------------------------------------
        # Recent Peaks
        # --------------------------------------------------

        peaks = self._find_recent_peaks(
            df,
            count=5
        )

        if len(peaks) < 2:

            return self.create_pattern_result(

                detected=False,

                pattern="Double Top",

                bullish=False,

                bearish=True,

                score=0,

                confidence=0,

                reasons=[
                    "Less than two swing highs"
                ]
            )

        # --------------------------------------------------
        # Generate Peak Pairs
        # --------------------------------------------------

        pairs = self._generate_pattern_pairs(peaks)

        if len(pairs) == 0:

            return self.create_pattern_result(

                detected=False,

                pattern="Double Top",

                bullish=False,

                bearish=True,

                score=0,

                confidence=0,

                reasons=[
                    "No valid peak pair found"
                ]
            )

        # --------------------------------------------------
        # Best Pattern
        # --------------------------------------------------

        best_pattern = None

        best_score = -1

        # --------------------------------------------------
        # Analyse Every Pair
        # --------------------------------------------------

        for peak1, peak2 in pairs:

            peak1_index = peak1["index"]
            peak2_index = peak2["index"]

            peak1_price = peak1["price"]
            peak2_price = peak2["price"]

            bars_between = peak2_index - peak1_index

            # ----------------------------------------------
            # Maximum Pattern Length
            # ----------------------------------------------

            if bars_between > self.max_pattern_bars:

                continue

            # ----------------------------------------------
            # Peak Similarity
            # ----------------------------------------------

            if not self._within_tolerance(
                peak1_price,
                peak2_price
            ):

                continue

            # ----------------------------------------------
            # Neckline
            # ----------------------------------------------

            neckline = self._find_neckline(

                df,

                peak1_index,

                peak2_index,

                pattern="double_top"

            )

            if neckline is None:

                continue

            neckline_price = neckline["price"]

            # ----------------------------------------------
            # Neckline Confirmation
            # ----------------------------------------------

            recent_closes = df["Close"].tail(
                self.neckline_confirmation
            )

            if not (recent_closes < neckline_price).all():

                continue

            latest = df.iloc[-1]

            latest_close = latest["Close"]

            latest_open = latest["Open"]

            # ----------------------------------------------
            # Bearish Breakdown Candle
            # ----------------------------------------------

            if latest_close >= latest_open:

                continue

            # ----------------------------------------------
            # Volume Confirmation
            # ----------------------------------------------

            volume_confirmed = False

            if "RVOL" in df.columns:

                if latest["RVOL"] >= 1.5:

                    volume_confirmed = True

            elif "VOLUME_MA20" in df.columns:

                if latest["Volume"] > latest["VOLUME_MA20"]:

                    volume_confirmed = True

            if not volume_confirmed:

                continue

            # ----------------------------------------------
            # Breakdown Strength
            # ----------------------------------------------

            breakdown_strength = (

                (neckline_price - latest_close)

                / neckline_price

            ) * 100

            # ----------------------------------------------
            # Pattern Score
            # ----------------------------------------------

            score = 0

            # Similar Peaks

            score += 20

            score += max(

                0,

                10 - abs(
                    peak1_price - peak2_price
                )

            )

            # Neckline

            score += 20

            # Breakdown

            score += min(

                breakdown_strength * 2,

                20

            )

            # Volume

            score += 15

            # Breakdown Candle

            score += 10

            # ----------------------------------------------
            # Store Best Pattern
            # ----------------------------------------------

            if score > best_score:

                best_score = score

                best_pattern = {

                    "Peak1": peak1,

                    "Peak2": peak2,

                    "Peak1 Price": round(
                        peak1_price,
                        2
                    ),

                    "Peak2 Price": round(
                        peak2_price,
                        2
                    ),

                    "Bars Between": bars_between,

                    "Neckline": neckline,

                    "Neckline Price": round(
                        neckline_price,
                        2
                    ),

                    "Latest Close": round(
                        latest_close,
                        2
                    ),

                    "Breakdown Strength": round(
                        breakdown_strength,
                        2
                    ),

                    "Score": round(
                        score,
                        2
                    ),

                    "Reasons": [

                        "Two Similar Peaks",

                        "Neckline Formed",

                        "Neckline Breakdown Confirmed",

                        "High Volume Breakdown",

                        "Bearish Breakdown Candle"

                    ]

                }

        # --------------------------------------------------
        # No Pattern
        # --------------------------------------------------

        if best_pattern is None:

            return self.create_pattern_result(

                detected=False,

                pattern="Double Top",

                bullish=False,

                bearish=True,

                score=0,

                confidence=0,

                reasons=[
                    "No Double Top"
                ]
            )

        # --------------------------------------------------
        # Confidence
        # --------------------------------------------------

        confidence = round(

            (best_pattern["Score"] / 95) * 100,

            2

        )

        # --------------------------------------------------
        # Return
        # --------------------------------------------------

        return self.create_pattern_result(

            detected=True,

            pattern="Double Top",

            bullish=False,

            bearish=True,

            score=best_pattern["Score"],

            confidence=confidence,

            reasons=best_pattern["Reasons"],

            details={

                "Peak1": best_pattern["Peak1"],

                "Peak2": best_pattern["Peak2"],

                "Neckline": best_pattern["Neckline"],

                "Breakdown Strength":
                    best_pattern["Breakdown Strength"]

            }

        )


    # ======================================================
    # Detect Double Bottom
    # Part 1
    # ======================================================

    def detect_double_bottom(self, df):

        """
        Detect Double Bottom Pattern

        Daily Timeframe

        Part 1
        --------
        ✓ Validation
        ✓ Swing Lows
        ✓ Pair Generation
        ✓ Pair Filtering
        ✓ Neckline Detection
        """

        # --------------------------------------------------
        # Validation
        # --------------------------------------------------

        self.validate(df)

        # --------------------------------------------------
        # Find Recent Swing Lows
        # --------------------------------------------------

        troughs = self._find_recent_troughs(
            df,
            count=5
        )

        if len(troughs) < 2:

            return self.create_pattern_result(

                detected=False,

                pattern="Double Bottom",

                bullish=True,

                bearish=False,

                score=0,

                confidence=0,

                reasons=[
                    "Less than two swing lows"
                ]
            )

        # --------------------------------------------------
        # Generate Trough Pairs
        # --------------------------------------------------

        pairs = self._generate_pattern_pairs(
            troughs
        )

        if not pairs:

            return self.create_pattern_result(

                detected=False,

                pattern="Double Bottom",

                bullish=True,

                bearish=False,

                score=0,

                confidence=0,

                reasons=[
                    "No valid trough pair found"
                ]
            )

        # --------------------------------------------------
        # Variables
        # --------------------------------------------------

        best_pattern = None

        best_score = -1

        # --------------------------------------------------
        # Analyse Every Pair
        # --------------------------------------------------

        for bottom1, bottom2 in pairs:

            bottom1_index = bottom1["index"]
            bottom2_index = bottom2["index"]

            bottom1_price = bottom1["price"]
            bottom2_price = bottom2["price"]

            bars_between = bottom2_index - bottom1_index

            # ----------------------------------------------
            # Pattern Too Large
            # ----------------------------------------------

            if bars_between > self.max_pattern_bars:

                continue

            # ----------------------------------------------
            # Similar Bottoms
            # ----------------------------------------------

            if not self._within_tolerance(

                bottom1_price,

                bottom2_price

            ):

                continue

            # ----------------------------------------------
            # Find Neckline
            # ----------------------------------------------

            neckline = self._find_neckline(

                df,

                bottom1_index,

                bottom2_index,

                pattern="double_bottom"

            )

            if neckline is None:

                continue

            neckline_price = neckline["price"]

            # ==================================================
            # Part 2 Starts Here
            # ==================================================
            #
            # Breakout Confirmation
            # Volume Confirmation
            # Bullish Candle Confirmation
            # Breakout Strength
            # Pattern Score
            # Store Best Pattern
            #
            # ----------------------------------------------
            # Breakout Confirmation
            # ----------------------------------------------

            recent_closes = df["Close"].tail(
                self.neckline_confirmation
            )

            breakout_confirmed = (
                recent_closes > neckline_price
            ).all()

            if not breakout_confirmed:

                continue

            latest = df.iloc[-1]

            latest_open = latest["Open"]
            latest_close = latest["Close"]
            latest_high = latest["High"]
            latest_volume = latest["Volume"]

            # ----------------------------------------------
            # Bullish Breakout Candle
            # ----------------------------------------------

            bullish_candle = (

                latest_close > latest_open

                and

                latest_close > neckline_price

            )

            if not bullish_candle:

                continue

            # ----------------------------------------------
            # Volume Confirmation
            # ----------------------------------------------

            volume_confirmed = False

            if "RVOL" in df.columns:

                if latest["RVOL"] >= 1.5:

                    volume_confirmed = True

            elif "VOLUME_MA20" in df.columns:

                if latest_volume > latest["VOLUME_MA20"]:

                    volume_confirmed = True

            elif "Volume_MA20" in df.columns:

                if latest_volume > latest["Volume_MA20"]:

                    volume_confirmed = True

            elif "VolumeEMA20" in df.columns:

                if latest_volume > latest["VolumeEMA20"]:

                    volume_confirmed = True

            if not volume_confirmed:

                continue

            # ----------------------------------------------
            # Breakout Strength
            # ----------------------------------------------

            breakout_strength = (

                (latest_close - neckline_price)

                /

                neckline_price

            ) * 100

            # ----------------------------------------------
            # Pattern Score
            # ----------------------------------------------

            score = 0

            # Similar Bottoms
            score += 20

            score += max(

                0,

                10 - abs(
                    bottom1_price - bottom2_price
                )

            )

            # Neckline Formation
            score += 20

            # Breakout Strength
            score += min(

                breakout_strength * 2,

                20

            )

            # Volume Confirmation
            score += 15

            # Bullish Breakout Candle
            score += 10

            # Pattern Duration Bonus
            if 10 <= bars_between <= 40:

                score += 5

            # ----------------------------------------------
            # Store Best Pattern
            # ----------------------------------------------

            if score > best_score:

                best_score = score

                best_pattern = {

                    "Bottom1": bottom1,

                    "Bottom2": bottom2,

                    "Bottom1 Price": round(
                        bottom1_price,
                        2
                    ),

                    "Bottom2 Price": round(
                        bottom2_price,
                        2
                    ),

                    "Bars Between": bars_between,

                    "Neckline": neckline,

                    "Neckline Price": round(
                        neckline_price,
                        2
                    ),

                    "Latest Close": round(
                        latest_close,
                        2
                    ),

                    "Breakout Strength": round(
                        breakout_strength,
                        2
                    ),

                    "Score": round(
                        score,
                        2
                    ),

                    "Reasons": [

                        "Two Similar Bottoms",

                        "Neckline Formed",

                        "Neckline Breakout Confirmed",

                        "High Volume Breakout",

                        "Bullish Breakout Candle"

                    ]

                }
                
        # --------------------------------------------------
        # No Pattern Found
        # --------------------------------------------------

        if best_pattern is None:

            return self.create_pattern_result(

                detected=False,

                pattern="Double Bottom",

                bullish=True,

                bearish=False,

                score=0,

                confidence=0,

                reasons=[

                    "No Valid Double Bottom Found"

                ]

            )

        # --------------------------------------------------
        # Confidence Calculation
        # --------------------------------------------------

        confidence = round(

            min(

                (best_pattern["Score"] / 100) * 100,

                100

            ),

            2

        )

        # --------------------------------------------------
        # Final Result
        # --------------------------------------------------

        return self.create_pattern_result(

            detected=True,

            pattern="Double Bottom",

            bullish=True,

            bearish=False,

            score=best_pattern["Score"],

            confidence=confidence,

            reasons=best_pattern["Reasons"],

            details={

                "Bottom1": best_pattern["Bottom1"],

                "Bottom2": best_pattern["Bottom2"],

                "Bottom1 Price": best_pattern["Bottom1 Price"],

                "Bottom2 Price": best_pattern["Bottom2 Price"],

                "Bars Between": best_pattern["Bars Between"],

                "Neckline": best_pattern["Neckline"],

                "Neckline Price": best_pattern["Neckline Price"],

                "Latest Close": best_pattern["Latest Close"],

                "Breakout Strength": best_pattern["Breakout Strength"],

                "Score": best_pattern["Score"]

            }

        )

    # --------------------------------------------------
    # Detect Head and Shoulders
    # --------------------------------------------------
    def detect_head_and_shoulders(self, df):

        """
        Detect Head & Shoulders Pattern

        Daily Timeframe

        Confirmations
        -------------
        ✓ Three swing highs
        ✓ Head higher than shoulders
        ✓ Shoulder symmetry
        ✓ Sloping neckline
        ✓ Neckline breakdown
        ✓ Volume confirmation
        ✓ Bearish candle confirmation
        """

        self.validate(df)

        peaks = self._find_recent_peaks(
            df,
            count=7
        )

        if len(peaks) < 3:

            return self.create_pattern_result(
                detected=False,
                pattern="Head & Shoulders",
                bullish=False,
                bearish=True,
                reasons=["Less than three swing highs"]
            )

        best_pattern = None
        best_score = -1

        for i in range(len(peaks)-2):

            left = peaks[i]
            head = peaks[i+1]
            right = peaks[i+2]

            left_price = left["price"]
            head_price = head["price"]
            right_price = right["price"]

            # -----------------------------
            # Head must be highest
            # -----------------------------

            if head_price <= left_price:
                continue

            if head_price <= right_price:
                continue

            # -----------------------------
            # Shoulder symmetry
            # -----------------------------

            if not self._within_tolerance(
                left_price,
                right_price
            ):
                continue

            # -----------------------------
            # Neckline points
            # -----------------------------

            valley1 = df.iloc[
                left["index"]:head["index"]+1
            ]["Low"].idxmin()

            valley2 = df.iloc[
                head["index"]:right["index"]+1
            ]["Low"].idxmin()

            neckline_left = df.loc[valley1,"Low"]
            neckline_right = df.loc[valley2,"Low"]

            # -----------------------------
            # Neckline slope
            # -----------------------------

            slope = (
                neckline_right -
                neckline_left
            ) / max(
                1,
                valley2-valley1
            )

            latest_index = len(df)-1

            neckline_price = (
                neckline_right +
                slope * (
                    latest_index-valley2
                )
            )

            latest = df.iloc[-1]

            # -----------------------------
            # Breakdown confirmation
            # -----------------------------

            recent = df.tail(
                self.neckline_confirmation
            )

            if not (
                recent["Close"] <
                neckline_price
            ).all():
                continue

            # -----------------------------
            # Bearish candle
            # -----------------------------

            if latest["Close"] >= latest["Open"]:
                continue

            # -----------------------------
            # Volume confirmation
            # -----------------------------

            volume_ok = False

            if "RVOL" in df.columns:

                volume_ok = (
                    latest["RVOL"] >= 1.5
                )

            elif "VOLUME_MA20" in df.columns:

                volume_ok = (
                    latest["Volume"] >
                    latest["VOLUME_MA20"]
                )

            if not volume_ok:
                continue

            # -----------------------------
            # Breakdown strength
            # -----------------------------

            breakdown = (
                neckline_price -
                latest["Close"]
            ) / neckline_price * 100

            # -----------------------------
            # Score
            # -----------------------------

            score = 0

            score += 20      # Three Peaks

            score += 20      # Head Highest

            score += 15      # Shoulder Symmetry

            score += 15      # Neckline

            score += min(
                breakdown*2,
                20
            )

            score += 10      # Volume

            if score > best_score:

                best_score = score

                best_pattern = {

                    "LeftShoulder":left,

                    "Head":head,

                    "RightShoulder":right,

                    "NecklineLeft":neckline_left,

                    "NecklineRight":neckline_right,

                    "NecklineSlope":round(slope,5),

                    "BreakdownStrength":round(
                        breakdown,
                        2
                    ),

                    "Score":round(
                        score,
                        2
                    ),

                    "Reasons":[

                        "Three Swing Highs",

                        "Head Higher Than Shoulders",

                        "Shoulders Symmetric",

                        "Sloping Neckline",

                        "Neckline Breakdown",

                        "High Volume"

                    ]

                }

        if best_pattern is None:

            return self.create_pattern_result(

                detected=False,

                pattern="Head & Shoulders",

                bullish=False,

                bearish=True,

                reasons=[
                    "No Valid Head & Shoulders"
                ]

            )

        confidence = round(

            best_pattern["Score"]/100*100,

            2

        )

        return self.create_pattern_result(

            detected=True,

            pattern="Head & Shoulders",

            bullish=False,

            bearish=True,

            score=best_pattern["Score"],

            confidence=confidence,

            reasons=best_pattern["Reasons"],

            details=best_pattern

        )

    # ==========================================================
    # #├── 9. Continuation / consolidation patterns
    # ==========================================================
    # ======================================================
    # Detect Rectangle
    # ======================================================
    
    def detect_rectangle(self, df):
    
        """
        Detect Rectangle Pattern
    
        Conditions
        ----------
        ✓ Multiple resistance touches
        ✓ Multiple support touches
        ✓ Horizontal range
        ✓ Breakout confirmation
        ✓ Volume confirmation
        """
    
        self.validate(df)
    
        highs = self._find_recent_peaks(df, count=8)
        lows = self._find_recent_troughs(df, count=8)
    
        if len(highs) < 2 or len(lows) < 2:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rectangle",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=["Not enough swing points"]
    
            )
    
        # --------------------------------------------------
        # Rectangle Levels
        # --------------------------------------------------
    
        resistance = np.mean([p["price"] for p in highs])
    
        support = np.mean([p["price"] for p in lows])
    
        # --------------------------------------------------
        # Horizontal Validation
        # --------------------------------------------------
    
        for p in highs:
    
            if not self._within_tolerance(
                p["price"],
                resistance
            ):
    
                return self.create_pattern_result(
    
                    detected=False,
    
                    pattern="Rectangle",
    
                    reasons=["Resistance not horizontal"]
    
                )
    
        for p in lows:
    
            if not self._within_tolerance(
                p["price"],
                support
            ):
    
                return self.create_pattern_result(
    
                    detected=False,
    
                    pattern="Rectangle",
    
                    reasons=["Support not horizontal"]
    
                )
    
        # --------------------------------------------------
        # Touch Count
        # --------------------------------------------------
    
        resistance_touches = len(highs)
    
        support_touches = len(lows)
    
        if resistance_touches < 2 or support_touches < 2:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rectangle",
    
                reasons=["Insufficient touches"]
    
            )
    
        # --------------------------------------------------
        # Latest Candle
        # --------------------------------------------------
    
        latest = df.iloc[-1]
    
        latest_close = latest["Close"]
    
        latest_open = latest["Open"]
    
        latest_volume = latest["Volume"]
    
        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------
    
        volume_confirmed = False
    
        if "RVOL" in df.columns:
    
            volume_confirmed = latest["RVOL"] >= 1.5
    
        elif "VOLUME_MA20" in df.columns:
    
            volume_confirmed = (
    
                latest_volume >
    
                latest["VOLUME_MA20"]
    
            )
    
        elif "VOLUME_MA" in df.columns:
    
            volume_confirmed = (
    
                latest_volume >
    
                latest["VOLUME_MA"]
    
            )
    
        # --------------------------------------------------
        # Breakout
        # --------------------------------------------------
    
        bullish = False
    
        bearish = False
    
        score = 0
    
        reasons = [
    
            f"{resistance_touches} Resistance Touches",
    
            f"{support_touches} Support Touches",
    
            "Horizontal Trading Range"
    
        ]
    
        score += 30
    
        if latest_close > resistance:
    
            bullish = True
    
            score += 20
    
            reasons.append("Bullish Breakout")
    
            if latest_close > latest_open:
    
                score += 10
    
                reasons.append("Bullish Breakout Candle")
    
        elif latest_close < support:
    
            bearish = True
    
            score += 20
    
            reasons.append("Bearish Breakdown")
    
            if latest_close < latest_open:
    
                score += 10
    
                reasons.append("Bearish Breakdown Candle")
    
        else:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rectangle",
    
                reasons=["No breakout"]
    
            )
    
        # --------------------------------------------------
        # Volume
        # --------------------------------------------------
    
        if volume_confirmed:
    
            score += 15
    
            reasons.append("High Breakout Volume")
    
        else:
    
            reasons.append("Weak Volume")
    
        # --------------------------------------------------
        # Confidence
        # --------------------------------------------------
    
        confidence = min(
    
            round(score * 100 / 75),
    
            100
    
        )
    
        # --------------------------------------------------
        # Return
        # --------------------------------------------------
    
        return self.create_pattern_result(
    
            detected=True,
    
            pattern="Rectangle",
    
            bullish=bullish,
    
            bearish=bearish,
    
            score=score,
    
            confidence=confidence,
    
            reasons=reasons,
    
            details={
    
                "Resistance": round(resistance, 2),
    
                "Support": round(support, 2),
    
                "Resistance Touches": resistance_touches,
    
                "Support Touches": support_touches,
    
                "Latest Close": round(latest_close, 2)
    
            },
    
            bars=len(df),
    
            latest_close=latest_close,
    
            latest_date=df.iloc[-1]["Date"]
            if "Date" in df.columns else None
    
        )

        
    # ======================================================
    # Detect Triangle
    # ======================================================

    def detect_triangle(self, df):

        """
        Detect

        • Ascending Triangle

        • Descending Triangle

        • Symmetrical Triangle

        Daily Timeframe

        Confirmations
        -------------
        ✓ Regression trendlines
        ✓ Breakout candle
        ✓ Volume expansion
        """

        self.validate(df)

        # --------------------------------------------------
        # Recent Swing Points
        # --------------------------------------------------

        highs = self._find_recent_peaks(
            df,
            count=5
        )

        lows = self._find_recent_troughs(
            df,
            count=5
        )

        if len(highs) < 2 or len(lows) < 2:

            return self.create_pattern_result(

                detected=False,

                pattern="Triangle",

                bullish=False,

                bearish=False,

                score=0,

                confidence=0,

                reasons=[
                    "Not enough swing points"
                ]
            )

        # --------------------------------------------------
        # Regression Trendlines
        # --------------------------------------------------

        high_line = self._calculate_trendline(highs)

        low_line = self._calculate_trendline(lows)

        high_slope = high_line["slope"]
        low_slope = low_line["slope"]

        latest_index = len(df) - 1

        upper_price = (

            high_slope * latest_index

            +

            high_line["intercept"]

        )

        lower_price = (

            low_slope * latest_index

            +

            low_line["intercept"]

        )

        latest = df.iloc[-1]

        latest_close = latest["Close"]
        latest_open = latest["Open"]
        latest_volume = latest["Volume"]

        # --------------------------------------------------
        # Pattern Type
        # --------------------------------------------------

        pattern = None

        bullish = False
        bearish = False

        score = 0

        reasons = []

        tolerance = 0.02

        if abs(high_slope) < tolerance and low_slope > 0:

            pattern = "Ascending Triangle"

            bullish = True

            score += 35

            reasons.extend([
                "Flat Resistance",
                "Rising Support"
            ])

        elif abs(low_slope) < tolerance and high_slope < 0:

            pattern = "Descending Triangle"

            bearish = True

            score += 35

            reasons.extend([
                "Flat Support",
                "Falling Resistance"
            ])

        elif high_slope < 0 and low_slope > 0:

            pattern = "Symmetrical Triangle"

            score += 30

            reasons.extend([
                "Falling Resistance",
                "Rising Support"
            ])

        else:

            return self.create_pattern_result(

                detected=False,

                pattern="Triangle",

                bullish=False,

                bearish=False,

                score=0,

                confidence=0,

                reasons=[
                    "No Triangle"
                ]
            )

        # --------------------------------------------------
        # Breakout Confirmation
        # --------------------------------------------------

        breakout = False

        if latest_close > upper_price:

            breakout = True

            bullish = True

            bearish = False

            score += 15

            reasons.append(
                "Upside Breakout"
            )

        elif latest_close < lower_price:

            breakout = True

            bearish = True

            bullish = False

            score += 15

            reasons.append(
                "Downside Breakdown"
            )

        if not breakout:

            return self.create_pattern_result(

                detected=False,

                pattern=pattern,

                bullish=False,

                bearish=False,

                score=0,

                confidence=0,

                reasons=[
                    "Triangle formed but no breakout"
                ]
            )

        # --------------------------------------------------
        # Breakout Candle Confirmation
        # --------------------------------------------------

        candle_confirmed = False

        if bullish:

            if latest_close > latest_open:

                candle_confirmed = True

        elif bearish:

            if latest_close < latest_open:

                candle_confirmed = True

        if candle_confirmed:

            score += 10

            reasons.append(
                "Breakout Candle Confirmed"
            )

        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------

        volume_confirmed = False

        if "RVOL" in df.columns:

            if latest["RVOL"] >= 1.5:

                volume_confirmed = True

        elif "VOLUME_MA20" in df.columns:

            if latest_volume > latest["VOLUME_MA20"]:

                volume_confirmed = True

        if volume_confirmed:

            score += 15

            reasons.append(
                "Volume Expansion"
            )

        # --------------------------------------------------
        # Confidence
        # --------------------------------------------------

        confidence = round(

            min(

                (score / 75) * 100,

                100

            ),

            2

        )

        # --------------------------------------------------
        # Return
        # --------------------------------------------------

        return self.create_pattern_result(

            detected=True,

            pattern=pattern,

            bullish=bullish,

            bearish=bearish,

            score=score,

            confidence=confidence,

            reasons=reasons,

            details={

                "Upper Trendline":
                    round(upper_price, 2),

                "Lower Trendline":
                    round(lower_price, 2),

                "Upper Slope":
                    round(high_slope, 4),

                "Lower Slope":
                    round(low_slope, 4),

                "Latest Close":
                    round(latest_close, 2)

            }

        )
    # ======================================================
    # Detect Channel (Version 2)
    # ======================================================
    
    def detect_channel(self, df):
    
        """
        Detect
    
        • Rising Channel
        • Falling Channel
        • Horizontal Channel
    
        Version 2
    
        Uses Linear Regression Trendlines
    
        Confirmations
        -------------
        ✓ Parallel Trendlines
        ✓ Breakout / Breakdown
        ✓ Volume Expansion
        ✓ Breakout Candle
        """
    
        self.validate(df)
    
        highs = self._find_recent_peaks(df, count=6)
        lows = self._find_recent_troughs(df, count=6)
    
        if len(highs) < 3 or len(lows) < 3:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Channel",
    
                reasons=["Not enough swing points"]
    
            )
    
        # --------------------------------------------------
        # Regression Trendlines
        # --------------------------------------------------
    
        upper_line = self._calculate_trendline(highs)
    
        lower_line = self._calculate_trendline(lows)
    
        if upper_line is None or lower_line is None:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Channel",
    
                reasons=["Unable to calculate trendlines"]
    
            )
    
        upper_slope = upper_line["slope"]
        lower_slope = lower_line["slope"]
    
        latest_index = len(df) - 1
    
        upper_price = (
    
            upper_slope * latest_index
    
            +
    
            upper_line["intercept"]
    
        )
    
        lower_price = (
    
            lower_slope * latest_index
    
            +
    
            lower_line["intercept"]
    
        )
    
        # --------------------------------------------------
        # Parallel Trendlines
        # --------------------------------------------------
    
        slope_difference = abs(
    
            upper_slope - lower_slope
    
        )
    
        if slope_difference > 0.10:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Channel",
    
                reasons=["Trendlines are not parallel"]
    
            )
    
        pattern = None
    
        bullish = False
    
        bearish = False
    
        score = 0
    
        reasons = []
    
        tolerance = 0.02
    
        # --------------------------------------------------
        # Rising Channel
        # --------------------------------------------------
    
        if (
    
            upper_slope > tolerance
    
            and
    
            lower_slope > tolerance
    
        ):
    
            pattern = "Rising Channel"
    
            bullish = True
    
            score += 30
    
            reasons.extend([
    
                "Parallel Rising Trendlines",
    
                "Higher Highs",
    
                "Higher Lows"
    
            ])
    
        # --------------------------------------------------
        # Falling Channel
        # --------------------------------------------------
    
        elif (
    
            upper_slope < -tolerance
    
            and
    
            lower_slope < -tolerance
    
        ):
    
            pattern = "Falling Channel"
    
            bearish = True
    
            score += 30
    
            reasons.extend([
    
                "Parallel Falling Trendlines",
    
                "Lower Highs",
    
                "Lower Lows"
    
            ])
    
        # --------------------------------------------------
        # Horizontal Channel
        # --------------------------------------------------
    
        elif (
    
            abs(upper_slope) <= tolerance
    
            and
    
            abs(lower_slope) <= tolerance
    
        ):
    
            pattern = "Horizontal Channel"
    
            score += 25
    
            reasons.extend([
    
                "Horizontal Resistance",
    
                "Horizontal Support"
    
            ])
    
        else:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Channel",
    
                reasons=["No valid channel"]
    
            )
    
        latest = df.iloc[-1]
    
        latest_close = latest["Close"]
    
        latest_open = latest["Open"]
    
        latest_volume = latest["Volume"]
    
        # --------------------------------------------------
        # Breakout / Breakdown
        # --------------------------------------------------
    
        if latest_close > upper_price:
    
            bullish = True
    
            score += 20
    
            reasons.append("Upside Breakout")
    
            if latest_close > latest_open:
    
                score += 10
    
                reasons.append("Bullish Breakout Candle")
    
        elif latest_close < lower_price:
    
            bearish = True
    
            score += 20
    
            reasons.append("Downside Breakdown")
    
            if latest_close < latest_open:
    
                score += 10
    
                reasons.append("Bearish Breakdown Candle")
    
        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------
    
        volume_confirmed = False
    
        if "RVOL" in df.columns:
    
            volume_confirmed = (
    
                latest["RVOL"] >= 1.5
    
            )
    
        elif "VOLUME_MA20" in df.columns:
    
            volume_confirmed = (
    
                latest_volume >
    
                latest["VOLUME_MA20"]
    
            )
    
        elif "VOLUME_MA" in df.columns:
    
            volume_confirmed = (
    
                latest_volume >
    
                latest["VOLUME_MA"]
    
            )
    
        if volume_confirmed:
    
            score += 15
    
            reasons.append("High Breakout Volume")
    
        else:
    
            reasons.append("Weak Volume")
    
        # --------------------------------------------------
        # Confidence
        # --------------------------------------------------
    
        confidence = min(
    
            round(score * 100 / 85),
    
            100
    
        )
    
        # --------------------------------------------------
        # Return
        # --------------------------------------------------
    
        return self.create_pattern_result(
    
            detected=True,
    
            pattern=pattern,
    
            bullish=bullish,
    
            bearish=bearish,
    
            score=score,
    
            confidence=confidence,
    
            reasons=reasons,
    
            details={
    
                "Upper Trendline": round(upper_price, 2),
    
                "Lower Trendline": round(lower_price, 2),
    
                "Upper Slope": round(upper_slope, 4),
    
                "Lower Slope": round(lower_slope, 4),
    
                "Slope Difference": round(slope_difference, 4),
    
                "Latest Close": round(latest_close, 2)
    
            },
    
            bars=len(df),
    
            latest_close=latest_close,
    
            latest_date=df.iloc[-1]["Date"]
    
            if "Date" in df.columns else None
    
        )
        

    # ======================================================
    # Detect Wedge
    # ======================================================
    
    def detect_wedge(self, df):
    
        """
        Detect
    
        • Rising Wedge
        • Falling Wedge
    
        Uses
        ----
        ✓ Linear regression trendlines
        ✓ Converging trendlines
        ✓ Breakout confirmation
        ✓ Volume confirmation
    
        Returns
        -------
        Pattern Result
        """
    
        self.validate(df)
    
        highs = self._find_recent_peaks(df, count=6)
        lows = self._find_recent_troughs(df, count=6)
    
        if len(highs) < 3 or len(lows) < 3:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Wedge",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=["Not enough swing points"]
    
            )
    
        # --------------------------------------------------
        # Regression Trendlines
        # --------------------------------------------------
    
        upper_line = self._calculate_trendline(highs)
        lower_line = self._calculate_trendline(lows)
    
        if upper_line is None or lower_line is None:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Wedge",
    
                reasons=["Unable to calculate trendlines"]
    
            )
    
        upper_slope = upper_line["slope"]
        lower_slope = lower_line["slope"]
    
        latest_index = len(df) - 1
    
        upper_price = (
    
            upper_slope * latest_index
    
            +
    
            upper_line["intercept"]
    
        )
    
        lower_price = (
    
            lower_slope * latest_index
    
            +
    
            lower_line["intercept"]
    
        )
    
        latest = df.iloc[-1]
    
        close = latest["Close"]
    
        volume = latest["Volume"]
    
        volume_ma = latest["VOLUME_MA"]
    
        reasons = []
    
        score = 0
    
        bullish = False
    
        bearish = False
    
        pattern = None
    
        # --------------------------------------------------
        # Convergence Check
        # --------------------------------------------------
    
        slope_difference = abs(
    
            upper_slope - lower_slope
    
        )
    
        channel_width = upper_price - lower_price
    
        if channel_width <= 0:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Wedge",
    
                reasons=["Invalid trendlines"]
    
            )
    
        # --------------------------------------------------
        # Rising Wedge
        # --------------------------------------------------
    
        if (
    
            upper_slope > 0
    
            and
    
            lower_slope > 0
    
            and
    
            lower_slope > upper_slope
    
        ):
    
            pattern = "Rising Wedge"
    
            bearish = True
    
            score += 25
    
            reasons.extend([
    
                "Both trendlines rising",
    
                "Support steeper than resistance",
    
                "Converging trendlines"
    
            ])
    
        # --------------------------------------------------
        # Falling Wedge
        # --------------------------------------------------
    
        elif (
    
            upper_slope < 0
    
            and
    
            lower_slope < 0
    
            and
    
            abs(upper_slope) > abs(lower_slope)
    
        ):
    
            pattern = "Falling Wedge"
    
            bullish = True
    
            score += 25
    
            reasons.extend([
    
                "Both trendlines falling",
    
                "Resistance steeper than support",
    
                "Converging trendlines"
    
            ])
    
        else:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Wedge",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=["No wedge structure"]
    
            )
    
        # --------------------------------------------------
        # Breakout Confirmation
        # --------------------------------------------------
    
        if close > upper_price:
    
            bullish = True
    
            bearish = False
    
            score += 15
    
            reasons.append("Upside breakout")
    
        elif close < lower_price:
    
            bearish = True
    
            bullish = False
    
            score += 15
    
            reasons.append("Downside breakdown")
    
        else:
    
            reasons.append("Price inside wedge")
    
        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------
    
        if volume >= volume_ma * self.volume_multiplier:
    
            score += 10
    
            reasons.append("High breakout volume")
    
        else:
    
            reasons.append("Weak volume")
    
        confidence = min(score * 2, 100)
    
        return self.create_pattern_result(
    
            detected=True,
    
            pattern=pattern,
    
            bullish=bullish,
    
            bearish=bearish,
    
            score=score,
    
            confidence=confidence,
    
            reasons=reasons,
    
            details={
    
                "Upper Trendline": round(upper_price, 2),
    
                "Lower Trendline": round(lower_price, 2),
    
                "Upper Slope": round(upper_slope, 4),
    
                "Lower Slope": round(lower_slope, 4),
    
                "Slope Difference": round(slope_difference, 4),
    
                "Channel Width": round(channel_width, 2),
    
                "Latest Close": round(close, 2)
    
            },
    
            bars=len(df),
    
            latest_close=close,
    
            latest_date=df.iloc[-1]["Date"] if "Date" in df.columns else None
    
        )
    # ==========================================================
    #├── 10. Flag patterns
    # ==========================================================
    # ======================================================
    # Detect Bull Flag
    # ======================================================
    
    def detect_bull_flag(self, df):
    
        """
        Detect Bull Flag Pattern
    
        Daily Timeframe
    
        Confirmations
        -------------
        ✓ Strong Bullish Pole
        ✓ Downward Flag
        ✓ Volume Contraction
        ✓ Breakout Candle
        ✓ Breakout Volume
        ✓ EMA Trend
        """
    
        self.validate(df)
    
        if len(df) < 40:
    
            return self.create_pattern_result(
                detected=False,
                pattern="Bull Flag",
                bullish=True,
                bearish=False,
                reasons=["Not enough candles"]
            )
    
        latest = df.iloc[-1]
    
        # --------------------------------------------------
        # Split Pole / Flag
        # --------------------------------------------------
    
        pole = df.iloc[-25:-10].copy()
    
        flag = df.iloc[-10:].copy()
    
        # --------------------------------------------------
        # Pole Strength
        # --------------------------------------------------
    
        pole_start = pole.iloc[0]["Close"]
        pole_end = pole.iloc[-1]["Close"]
    
        pole_return = (
    
            (pole_end - pole_start)
    
            / pole_start
    
        ) * 100
    
        if pole_return < 8:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Bull Flag",
    
                bullish=True,
    
                bearish=False,
    
                reasons=[
                    "Weak Flag Pole"
                ]
    
            )
    
        # --------------------------------------------------
        # Flag Trend
        # --------------------------------------------------
    
        x = np.arange(len(flag))
    
        slope, intercept = np.polyfit(
            x,
            flag["Close"],
            1
        )
    
        if slope >= 0:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Bull Flag",
    
                bullish=True,
    
                bearish=False,
    
                reasons=[
                    "Flag is not downward"
                ]
    
            )
    
        # --------------------------------------------------
        # Flag Depth
        # --------------------------------------------------
    
        flag_high = flag["High"].max()
    
        flag_low = flag["Low"].min()
    
        retracement = (
    
            (flag_high - flag_low)
    
            /
    
            (pole_end - pole_start)
    
        )
    
        if retracement > 0.50:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Bull Flag",
    
                bullish=True,
    
                bearish=False,
    
                reasons=[
                    "Flag retracement too deep"
                ]
    
            )
    
        # --------------------------------------------------
        # Volume Contraction
        # --------------------------------------------------
    
        pole_volume = pole["Volume"].mean()
    
        flag_volume = flag["Volume"].mean()
    
        volume_contraction = flag_volume < pole_volume
    
        # --------------------------------------------------
        # Breakout Level
        # --------------------------------------------------
    
        breakout_level = flag["High"].max()
    
        latest_close = latest["Close"]
    
        breakout = latest_close > breakout_level
    
        if not breakout:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Bull Flag",
    
                bullish=True,
    
                bearish=False,
    
                reasons=[
                    "No Breakout"
                ]
    
            )
    
        # --------------------------------------------------
        # Bullish Candle
        # --------------------------------------------------
    
        bullish_candle = (
    
            latest["Close"]
    
            >
    
            latest["Open"]
    
        )
    
        if not bullish_candle:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Bull Flag",
    
                bullish=True,
    
                bearish=False,
    
                reasons=[
                    "Breakout candle not bullish"
                ]
    
            )
    
        # --------------------------------------------------
        # Breakout Volume
        # --------------------------------------------------
    
        volume_confirmed = False
    
        if "RVOL" in df.columns:
    
            volume_confirmed = latest["RVOL"] >= 1.5
    
        elif "VOLUME_MA20" in df.columns:
    
            volume_confirmed = (
    
                latest["Volume"]
    
                >
    
                latest["VOLUME_MA20"]
    
            )
    
        if not volume_confirmed:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Bull Flag",
    
                bullish=True,
    
                bearish=False,
    
                reasons=[
                    "Breakout volume not confirmed"
                ]
    
            )
    
        # --------------------------------------------------
        # EMA Trend Confirmation
        # --------------------------------------------------
    
        ema_confirm = False
    
        if all(col in df.columns for col in ["EMA20", "EMA50", "EMA200"]):
    
            ema_confirm = (
    
                latest["EMA20"]
    
                >
    
                latest["EMA50"]
    
                >
    
                latest["EMA200"]
    
            )
    
        # --------------------------------------------------
        # Score
        # --------------------------------------------------
    
        score = 0
    
        reasons = []
    
        score += 25
        reasons.append("Strong Bullish Pole")
    
        score += 20
        reasons.append("Downward Flag")
    
        if volume_contraction:
    
            score += 15
            reasons.append("Volume Contracted During Flag")
    
        score += 20
        reasons.append("Breakout Confirmed")
    
        score += 15
        reasons.append("High Breakout Volume")
    
        if ema_confirm:
    
            score += 10
            reasons.append("EMA Trend Alignment")
    
        confidence = min(
    
            round(score / 105 * 100),
    
            100
    
        )
    
        return self.create_pattern_result(
    
            detected=True,
    
            pattern="Bull Flag",
    
            bullish=True,
    
            bearish=False,
    
            score=score,
    
            confidence=confidence,
    
            reasons=reasons,
    
            details={
    
                "Pole Return %": round(pole_return, 2),
    
                "Flag Slope": round(slope, 4),
    
                "Retracement": round(retracement * 100, 2),
    
                "Breakout Level": round(breakout_level, 2),
    
                "Latest Close": round(latest_close, 2),
    
                "Pole Volume": round(pole_volume, 0),
    
                "Flag Volume": round(flag_volume, 0),
    
                "EMA Trend": ema_confirm,
    
                "Volume Contraction": volume_contraction
    
            }
    
        )

    # ==========================================================
    # Part 3B : Detect Bear Flag
    # ==========================================================
    
    def detect_bear_flag(
        self,
        df,
        lookback=20,
        min_pole_pct=8,
        max_flag_pullback=40
    ):
        """
        Detect Bear Flag Pattern.
    
        Parameters
        ----------
        df : DataFrame
    
        lookback : int
            Number of candles to inspect.
    
        min_pole_pct : float
            Minimum fall (%) required to form the flag pole.
    
        max_flag_pullback : float
            Maximum pullback (% of pole).
    
        Returns
        -------
        {
            "Detected": bool,
            "Pattern": "Bear Flag",
            "Bullish": bool,
            "Bearish": bool,
            "Confidence": int,
            "Pole %": float,
            "Pullback %": float,
            "Breakdown Price": float,
            "Reason": str
        }
        """
    
        if df is None or len(df) < lookback:
    
            return {
    
                "Detected": False,
    
                "Pattern": "Bear Flag",
    
                "Bullish": False,
    
                "Bearish": False,
    
                "Confidence": 0,
    
                "Reason": "Not enough candles"
    
            }
    
        recent = df.tail(lookback).copy()
    
        # --------------------------------------------------
        # Highest & Lowest
        # --------------------------------------------------
    
        highest = recent["High"].max()
    
        lowest = recent["Low"].min()
    
        pole_pct = ((highest - lowest) / highest) * 100
    
        if pole_pct < min_pole_pct:
    
            return {
    
                "Detected": False,
    
                "Pattern": "Bear Flag",
    
                "Bullish": False,
    
                "Bearish": False,
    
                "Confidence": 0,
    
                "Reason": "Pole too small"
    
            }
    
        # --------------------------------------------------
        # Find Lowest Candle
        # --------------------------------------------------
    
        low_index = recent["Low"].idxmin()
    
        after_low = recent.loc[low_index:]
    
        if len(after_low) < 3:
    
            return {
    
                "Detected": False,
    
                "Pattern": "Bear Flag",
    
                "Bullish": False,
    
                "Bearish": False,
    
                "Confidence": 0,
    
                "Reason": "No pullback"
    
            }
    
        # --------------------------------------------------
        # Pullback
        # --------------------------------------------------
    
        pullback_high = after_low["High"].max()
    
        pullback_pct = (
    
            (pullback_high - lowest)
    
            /
    
            (highest - lowest)
    
        ) * 100
    
        if pullback_pct > max_flag_pullback:
    
            return {
    
                "Detected": False,
    
                "Pattern": "Bear Flag",
    
                "Bullish": False,
    
                "Bearish": False,
    
                "Confidence": 0,
    
                "Reason": "Pullback too deep"
    
            }
    
        # --------------------------------------------------
        # Lower Highs
        # --------------------------------------------------
    
        highs = after_low["High"].values
    
        lower_highs = 0
    
        for i in range(1, len(highs)):
    
            if highs[i] <= highs[i - 1]:
    
                lower_highs += 1
    
        # --------------------------------------------------
        # Confidence
        # --------------------------------------------------
    
        confidence = 60
    
        if lower_highs >= 2:
    
            confidence += 15
    
        if pole_pct >= 15:
    
            confidence += 10
    
        if pullback_pct <= 25:
    
            confidence += 10
    
        confidence = min(confidence, 95)
    
        # --------------------------------------------------
        # Return
        # --------------------------------------------------
    
        return {
    
            "Detected": True,
    
            "Pattern": "Bear Flag",
    
            "Bullish": False,
    
            "Bearish": True,
    
            "Confidence": confidence,
    
            "Pole %": round(pole_pct, 2),
    
            "Pullback %": round(pullback_pct, 2),
    
            "Breakdown Price": round(lowest, 2),
    
            "Reason": "Healthy bear flag detected"
    
        }
    
    # ==========================================================
    # ├── 11. Cup / rounding patterns
    # ==========================================================
    # Detect Cup & Handle (Version 2)
    # ==========================================================
    
    def detect_cup_handle(
        self,
        df,
        lookback=120,
        rim_tolerance=3,
        min_cup_depth=10,
        max_cup_depth=40,
        min_cup_bars=30,
        max_handle_depth_ratio=0.40,
        min_handle_bars=5,
        max_handle_bars=20
    ):
    
        """
        Detect Cup & Handle Pattern (Version 2)
    
        Features
        --------
        ✓ Better cup geometry
        ✓ Rim validation
        ✓ Cup duration validation
        ✓ Handle validation
        ✓ Breakout validation
        ✓ Volume confirmation
        """
    
        self.validate(df)
    
        # ------------------------------------------------------
        # Enough candles?
        # ------------------------------------------------------
    
        if len(df) < lookback:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=["Not enough candles"]
    
            )
    
        # ------------------------------------------------------
        # Recent Data
        # ------------------------------------------------------
    
        recent = (
            df.tail(lookback)
              .reset_index(drop=True)
              .copy()
        )
    
        total_bars = len(recent)
    
        # ------------------------------------------------------
        # Split into 3 Sections
        #
        # Left ---- Bottom ---- Right
        # ------------------------------------------------------
    
        left_end = total_bars // 3
    
        right_start = (total_bars * 2) // 3
    
        left_section = recent.iloc[:left_end]
    
        middle_section = recent.iloc[left_end:right_start]
    
        right_section = recent.iloc[right_start:]
    
        # ------------------------------------------------------
        # Left Rim
        # ------------------------------------------------------
    
        left_idx = left_section["High"].idxmax()
    
        left_rim = float(
    
            recent.loc[left_idx, "High"]
    
        )
    
        # ------------------------------------------------------
        # Cup Bottom
        # ------------------------------------------------------
    
        bottom_idx = middle_section["Low"].idxmin()
    
        bottom = float(
    
            recent.loc[bottom_idx, "Low"]
    
        )
    
        # ------------------------------------------------------
        # Right Rim
        # ------------------------------------------------------
    
        right_idx = right_section["High"].idxmax()
    
        right_rim = float(
    
            recent.loc[right_idx, "High"]
    
        )
    
        # ------------------------------------------------------
        # Basic Ordering Validation
        # ------------------------------------------------------
    
        if not (
    
            left_idx < bottom_idx < right_idx
    
        ):
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                reasons=[
    
                    "Invalid cup structure"
    
                ]
    
            )
    
        # ------------------------------------------------------
        # Cup Width
        # ------------------------------------------------------
    
        cup_width = right_idx - left_idx
    
        if cup_width < min_cup_bars:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                reasons=[
    
                    "Cup duration too short"
    
                ]
    
            )

        # ------------------------------------------------------
        # Rim Similarity Validation
        # ------------------------------------------------------
    
        rim_difference = abs(
    
            left_rim - right_rim
    
        )
    
        rim_difference_pct = (
    
            rim_difference /
    
            max(left_rim, right_rim)
    
        ) * 100
    
        if rim_difference_pct > rim_tolerance:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=[
    
                    f"Rims differ by {rim_difference_pct:.2f}%"
    
                ]
    
            )
    
        # ------------------------------------------------------
        # Cup Depth
        # ------------------------------------------------------
    
        cup_depth = (
    
            (
    
                ((left_rim + right_rim) / 2)
    
                -
    
                bottom
    
            )
    
            /
    
            ((left_rim + right_rim) / 2)
    
        ) * 100
    
        # ------------------------------------------------------
        # Cup Too Shallow
        # ------------------------------------------------------
    
        if cup_depth < min_cup_depth:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=[
    
                    f"Cup too shallow ({cup_depth:.2f}%)"
    
                ]
    
            )
    
        # ------------------------------------------------------
        # Cup Too Deep
        # ------------------------------------------------------
    
        if cup_depth > max_cup_depth:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=[
    
                    f"Cup too deep ({cup_depth:.2f}%)"
    
                ]
    
            )
    
        # ------------------------------------------------------
        # Cup Duration
        # ------------------------------------------------------
    
        if cup_width < min_cup_bars:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                score=0,
    
                confidence=0,
    
                reasons=[
    
                    "Cup duration too short"
    
                ]
    
            )
    
        # ------------------------------------------------------
        # Store Geometry Details
        # ------------------------------------------------------
    
        geometry_details = {
    
            "Left Rim": round(left_rim, 2),
    
            "Bottom": round(bottom, 2),
    
            "Right Rim": round(right_rim, 2),
    
            "Cup Width": int(cup_width),
    
            "Cup Depth %": round(cup_depth, 2),
    
            "Rim Difference %": round(rim_difference_pct, 2)
    
        }
        # =====================================================
        # Section C
        # U-Shape Validation
        # =====================================================
    
        cup_section = recent.iloc[left_idx:right_idx + 1].copy()
    
        cup_length = len(cup_section)
    
        if cup_length < 10:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                confidence=0,
    
                reasons=[
    
                    "Cup section too small"
    
                ]
    
            )
    
        # -----------------------------------------------------
        # Ideal Rounded Cup
        #
        # Divide cup into:
        # Left
        # Bottom
        # Right
        # -----------------------------------------------------
    
        third = cup_length // 3
    
        left_curve = cup_section.iloc[:third]
    
        middle_curve = cup_section.iloc[
            third:2 * third
        ]
    
        right_curve = cup_section.iloc[
            2 * third:
        ]
    
        geometry_score = 0
    
        reasons = []
    
        # -----------------------------------------------------
        # Left Side Should Decline
        # -----------------------------------------------------
    
        if (
    
            left_curve["Close"].iloc[-1]
    
            <
    
            left_curve["Close"].iloc[0]
    
        ):
    
            geometry_score += 20
    
            reasons.append(
    
                "Left side declines"
    
            )
    
        # -----------------------------------------------------
        # Bottom Should Stay Flat
        # -----------------------------------------------------
    
        bottom_std = middle_curve["Close"].std()
    
        bottom_mean = middle_curve["Close"].mean()
    
        if bottom_mean > 0:
    
            bottom_variation = (
    
                bottom_std
    
                /
    
                bottom_mean
    
            ) * 100
    
        else:
    
            bottom_variation = 100
    
        if bottom_variation <= 3:
    
            geometry_score += 25
    
            reasons.append(
    
                "Rounded bottom"
    
            )
    
        else:
    
            reasons.append(
    
                "Bottom too volatile"
    
            )
    
        # -----------------------------------------------------
        # Right Side Should Rise
        # -----------------------------------------------------
    
        if (
    
            right_curve["Close"].iloc[-1]
    
            >
    
            right_curve["Close"].iloc[0]
    
        ):
    
            geometry_score += 20
    
            reasons.append(
    
                "Right side rising"
    
            )
    
        # -----------------------------------------------------
        # Polynomial Curvature
        # -----------------------------------------------------
    
        x = np.arange(cup_length)
    
        y = cup_section["Close"].values
    
        try:
    
            a, b, c = np.polyfit(
    
                x,
    
                y,
    
                2
    
            )
    
        except Exception:
    
            a = 0
    
        # Positive curvature indicates U-shape
    
        if a > 0:
    
            geometry_score += 25
    
            reasons.append(
    
                "Positive U-shaped curvature"
    
            )
    
        else:
    
            reasons.append(
    
                "Curvature not rounded"
    
            )
    
        # -----------------------------------------------------
        # Symmetry Check
        # -----------------------------------------------------
    
        left_width = bottom_idx - left_idx
    
        right_width = right_idx - bottom_idx
    
        symmetry = min(
    
            left_width,
    
            right_width
    
        ) / max(
    
            left_width,
    
            right_width
    
        )
    
        if symmetry >= 0.60:
    
            geometry_score += 10
    
            reasons.append(
    
                "Cup reasonably symmetric"
    
            )
    
        else:
    
            reasons.append(
    
                "Cup asymmetric"
    
            )
    
        # -----------------------------------------------------
        # Geometry Quality
        # -----------------------------------------------------
    
        geometry_score = min(
    
            geometry_score,
    
            100
    
        )
    
        if geometry_score < 60:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                confidence=geometry_score,
    
                reasons=reasons,
    
                details={
    
                    "Geometry Score": geometry_score,
    
                    "Cup Width": cup_width,
    
                    "Cup Depth %": round(cup_depth, 2),
    
                    "Polynomial Curvature": round(a, 6)
    
                }
    
            )
    
        # -----------------------------------------------------
        # Intermediate Values
        # Returned to Part 2 (Handle Detection)
        # -----------------------------------------------------
    
        cup_data = {
    
            "Left Rim": left_rim,
    
            "Right Rim": right_rim,
    
            "Bottom": bottom,
    
            "Cup Width": cup_width,
    
            "Cup Depth %": round(cup_depth, 2),
    
            "Geometry Score": geometry_score,
    
            "Curvature": a,
    
            "Reasons": reasons,
    
            "Right Rim Index": right_idx,
    
            "Left Rim Index": left_idx,
    
            "Bottom Index": bottom_idx
    
        }

        # =====================================================
        # Part 2 : Handle Detection
        # =====================================================
    
        # Handle begins after right rim
        handle = recent.iloc[right_idx + 1:].copy()
    
        if len(handle) < 5:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                reasons=[
    
                    "Handle not formed"
    
                ]
    
            )
    
        # ------------------------------------------
        # Maximum Handle Length
        # ------------------------------------------
    
        if len(handle) > 20:
    
            handle = handle.iloc[:20]
    
        handle_width = len(handle)
    
        # ------------------------------------------
        # Handle High / Low
        # ------------------------------------------
    
        handle_high = handle["High"].max()
    
        handle_low = handle["Low"].min()
    
        handle_depth = (
    
            (right_rim - handle_low)
    
            / right_rim
    
        ) * 100
    
        # ------------------------------------------
        # Handle Depth Validation
        # Handle depth should be <= 40% of cup depth
        # ------------------------------------------
    
        max_allowed_depth = cup_depth * 0.40
    
        if handle_depth > max_allowed_depth:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                bullish=False,
    
                bearish=False,
    
                reasons=[
    
                    "Handle too deep"
    
                ],
    
                details={
    
                    "Cup Depth %": round(cup_depth,2),
    
                    "Handle Depth %": round(handle_depth,2)
    
                }
    
            )
    
        # ------------------------------------------
        # Handle Duration
        # ------------------------------------------
    
        if handle_width < 5:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                reasons=[
    
                    "Handle too short"
    
                ]
    
            )
    
        if handle_width > 20:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                reasons=[
    
                    "Handle too long"
    
                ]
    
            )
    
        # ------------------------------------------
        # Handle Trendline
        # ------------------------------------------
    
        x = np.arange(handle_width)
    
        y = handle["Close"].values
    
        slope, intercept = np.polyfit(
    
            x,
    
            y,
    
            1
    
        )
    
        # ------------------------------------------
        # Valid Handle Direction
        # Slight downward or sideways
        # ------------------------------------------
    
        if slope > 0.15:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Cup & Handle",
    
                reasons=[
    
                    "Handle rising instead of drifting down"
    
                ]
    
            )
    
        # ------------------------------------------
        # Handle Volume Contraction
        # ------------------------------------------
    
        handle_avg_volume = handle["Volume"].mean()
    
        cup_avg_volume = recent.iloc[
            left_idx:right_idx + 1
        ]["Volume"].mean()
    
        volume_ratio = (
    
            handle_avg_volume
    
            / cup_avg_volume
    
        )
    
        volume_contraction = volume_ratio < 0.80
    
        # ------------------------------------------
        # Handle Quality Score
        # ------------------------------------------
    
        handle_score = 100
    
        if handle_depth > cup_depth * 0.30:
    
            handle_score -= 15
    
        if handle_width > 15:
    
            handle_score -= 10
    
        if slope > 0:
    
            handle_score -= 20
    
        if not volume_contraction:
    
            handle_score -= 20
    
        handle_score = max(
    
            0,
    
            min(
    
                100,
    
                handle_score
    
            )
    
        )
    
        # ------------------------------------------
        # Intermediate Values
        # Used by Part 3 (Breakout)
        # ------------------------------------------
    
        handle_details = {
    
            "Handle High": round(handle_high,2),
    
            "Handle Low": round(handle_low,2),
    
            "Handle Width": handle_width,
    
            "Handle Depth %": round(handle_depth,2),
    
            "Handle Slope": round(slope,4),
    
            "Handle Avg Volume": round(handle_avg_volume),
    
            "Cup Avg Volume": round(cup_avg_volume),
    
            "Volume Ratio": round(volume_ratio,2),
    
            "Volume Contraction": volume_contraction,
    
            "Handle Score": round(handle_score,2)
    
        }

        # =====================================================
        # Part 3 : Breakout Validation
        # =====================================================

        latest = recent.iloc[-1]

        breakout_price = max(
            left_rim,
            right_rim
        )

        latest_close = latest["Close"]
        latest_open = latest["Open"]
        latest_volume = latest["Volume"]

        # -----------------------------------------------------
        # Volume MA
        # -----------------------------------------------------

        if "VOLUME_MA" in latest.index:

            volume_ma = latest["VOLUME_MA"]

        else:

            volume_ma = (
                recent["Volume"]
                .rolling(20)
                .mean()
                .iloc[-1]
            )

        # -----------------------------------------------------
        # Initialize
        # -----------------------------------------------------

        reasons = []

        pattern_quality = geometry_score

        breakout_confirmation = 0

        volume_confirmation = 0

        trend_alignment = 50

        momentum = 50

        detected = False

        # -----------------------------------------------------
        # Breakout Above Rim
        # -----------------------------------------------------

        if latest_close > breakout_price:

            breakout_confirmation += 60

            reasons.append(
                "Close above cup resistance"
            )

        # -----------------------------------------------------
        # Strong Bullish Candle
        # -----------------------------------------------------

        body = abs(
            latest_close - latest_open
        )

        candle_range = (
            latest["High"] -
            latest["Low"]
        )

        if (
            latest_close > latest_open
            and
            candle_range > 0
            and
            body / candle_range >= 0.60
        ):

            breakout_confirmation += 20

            reasons.append(
                "Strong bullish breakout candle"
            )

        # -----------------------------------------------------
        # Breakout Volume
        # -----------------------------------------------------

        breakout_volume_ratio = 0

        if (
            volume_ma is not None
            and
            not pd.isna(volume_ma)
            and
            volume_ma > 0
        ):

            breakout_volume_ratio = (
                latest_volume /
                volume_ma
            )

        if (
            breakout_volume_ratio >=
            self.volume_multiplier
        ):

            volume_confirmation = min(
                100,
                breakout_volume_ratio * 50
            )

            breakout_confirmation += 20

            reasons.append(
                "High breakout volume"
            )

        else:

            volume_confirmation = min(
                100,
                breakout_volume_ratio * 50
            )

            reasons.append(
                "Weak breakout volume"
            )

        # -----------------------------------------------------
        # Trend Alignment
        # -----------------------------------------------------

        if "EMA20" in recent.columns:

            if latest_close > latest["EMA20"]:

                trend_alignment += 20

                reasons.append(
                    "Above EMA20"
                )

        if "EMA50" in recent.columns:

            if latest_close > latest["EMA50"]:

                trend_alignment += 20

                reasons.append(
                    "Above EMA50"
                )

        if "EMA200" in recent.columns:

            if latest_close > latest["EMA200"]:

                trend_alignment += 10

                reasons.append(
                    "Above EMA200"
                )

        trend_alignment = min(
            trend_alignment,
            100
        )

        # -----------------------------------------------------
        # Momentum
        # -----------------------------------------------------

        if "RSI" in recent.columns:

            rsi = latest["RSI"]

            if 55 <= rsi <= 70:

                momentum = 90

                reasons.append(
                    "Healthy bullish RSI"
                )

            elif rsi > 70:

                momentum = 70

                reasons.append(
                    "Strong momentum"
                )

            elif rsi >= 45:

                momentum = 60

        # -----------------------------------------------------
        # Final Pattern Score
        # -----------------------------------------------------

        score = self.calculate_pattern_score(

            pattern_quality=pattern_quality,

            breakout_confirmation=
                breakout_confirmation,

            volume_confirmation=
                volume_confirmation,

            trend_alignment=
                trend_alignment

        )

        # -----------------------------------------------------
        # Final Confidence
        # -----------------------------------------------------

        confidence = self.calculate_confidence(

            geometry=pattern_quality,

            volume=volume_confirmation,

            breakout=breakout_confirmation,

            trend=trend_alignment,

            momentum=momentum

        )

        # -----------------------------------------------------
        # Detection Decision
        # -----------------------------------------------------

        if (
            breakout_confirmation >= 70
            and
            volume_confirmation >= 60
            and
            confidence >= 70
        ):

            detected = True

        # -----------------------------------------------------
        # Final Return
        # -----------------------------------------------------

        return self.create_pattern_result(

            detected=detected,

            pattern="Cup & Handle",

            bullish=detected,

            bearish=False,

            score=score,

            confidence=confidence,

            reasons=reasons,

            details={

                "Cup Depth %":
                    round(cup_depth, 2),

                "Handle Depth %":
                    round(handle_depth, 2),

                "Cup Width":
                    int(cup_width),

                "Handle Width":
                    int(handle_width),

                "Breakout Price":
                    round(
                        breakout_price,
                        2
                    ),

                "Breakout Volume Ratio":
                    round(
                        breakout_volume_ratio,
                        2
                    ),

                "Handle Volume Ratio":
                    round(
                        handle_volume_ratio,
                        2
                    ),

                "Pattern Quality":
                    round(
                        pattern_quality,
                        2
                    ),

                "Breakout Score":
                    round(
                        breakout_confirmation,
                        2
                    ),

                "Volume Score":
                    round(
                        volume_confirmation,
                        2
                    ),

                "Trend Score":
                    round(
                        trend_alignment,
                        2
                    ),

                "Momentum Score":
                    round(
                        momentum,
                        2
                    )

            },

            bars=len(recent),

            latest_close=latest_close,

            latest_date=(
                latest["Date"]
                if "Date" in recent.columns
                else None
            )

        )


    
    # ======================================================
    # Detect Rounding Bottom (Version 2)
    # ======================================================
    
    def detect_rounding_bottom(self, df):
    
        """
        Detect Rounding Bottom using quadratic regression.
    
        Uses
        ----
        ✓ Quadratic curve fitting
        ✓ Curvature scoring
        ✓ Bottom near center
        ✓ Right-side recovery
        ✓ Breakout confirmation
        ✓ Volume confirmation
    
        Returns
        -------
        Pattern Result
        """
    
        self.validate(df)
    
        lookback = min(60, len(df))
    
        if lookback < 30:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Bottom",
    
                reasons=["Not enough candles"]
    
            )
    
        section = df.iloc[-lookback:].copy()
    
        x = np.arange(len(section))
    
        y = section["Close"].values
    
        # --------------------------------------------------
        # Quadratic Regression
        # y = ax² + bx + c
        # --------------------------------------------------
    
        coeffs = np.polyfit(x, y, 2)
    
        a, b, c = coeffs
    
        fitted = np.polyval(coeffs, x)
    
        # --------------------------------------------------
        # Goodness of Fit (R²)
        # --------------------------------------------------
    
        ss_res = np.sum((y - fitted) ** 2)
    
        ss_tot = np.sum((y - np.mean(y)) ** 2)
    
        if ss_tot == 0:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Bottom",
    
                reasons=["Flat price movement"]
    
            )
    
        r_squared = 1 - (ss_res / ss_tot)
    
        # --------------------------------------------------
        # Must curve upward
        # --------------------------------------------------
    
        if a <= 0:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Bottom",
    
                reasons=["Curve opens downward"]
    
            )
    
        reasons = []
    
        score = 0
    
        bullish = False
    
        # --------------------------------------------------
        # Curvature Strength
        # --------------------------------------------------
    
        curvature = abs(a)
    
        if curvature > 0.005:
    
            score += 20
    
            reasons.append("Strong positive curvature")
    
        elif curvature > 0.002:
    
            score += 15
    
            reasons.append("Moderate curvature")
    
        else:
    
            score += 8
    
            reasons.append("Weak curvature")
    
        # --------------------------------------------------
        # Curve Fit
        # --------------------------------------------------
    
        if r_squared > 0.90:
    
            score += 20
    
            reasons.append("Excellent curve fit")
    
        elif r_squared > 0.80:
    
            score += 15
    
            reasons.append("Good curve fit")
    
        elif r_squared > 0.70:
    
            score += 10
    
            reasons.append("Acceptable curve fit")
    
        else:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Bottom",
    
                reasons=["Poor curve fit"]
    
            )
    
        # --------------------------------------------------
        # Bottom near middle
        # --------------------------------------------------
    
        bottom_index = np.argmin(y)
    
        center = len(section) / 2
    
        distance = abs(bottom_index - center)
    
        if distance <= len(section) * 0.20:
    
            score += 15
    
            reasons.append("Bottom near center")
    
        else:
    
            reasons.append("Bottom off-center")
    
        # --------------------------------------------------
        # Right-side recovery
        # --------------------------------------------------
    
        left_rim = np.max(y[:bottom_index])
    
        right_close = y[-1]
    
        recovery = (right_close - np.min(y)) / max(left_rim - np.min(y), 1)
    
        if recovery >= 0.80:
    
            score += 15
    
            reasons.append("Strong recovery")
    
        elif recovery >= 0.60:
    
            score += 10
    
            reasons.append("Moderate recovery")
    
        else:
    
            reasons.append("Weak recovery")
    
        # --------------------------------------------------
        # Breakout above left rim
        # --------------------------------------------------
    
        if right_close > left_rim:
    
            bullish = True
    
            score += 20
    
            reasons.append("Breakout above left rim")
    
        else:
    
            reasons.append("No breakout")
    
        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------
    
        if "VOLUME_MA" in section.columns:
    
            latest_volume = section.iloc[-1]["Volume"]
    
            volume_ma = section.iloc[-1]["VOLUME_MA"]
    
            if latest_volume >= volume_ma * self.volume_multiplier:
    
                score += 10
    
                reasons.append("High breakout volume")
    
            else:
    
                reasons.append("Weak volume")
    
        confidence = min(score * 2, 100)
    
        return self.create_pattern_result(
    
            detected=score >= 55,
    
            pattern="Rounding Bottom",
    
            bullish=bullish,
    
            bearish=False,
    
            score=score,
    
            confidence=confidence,
    
            reasons=reasons,
    
            details={
    
                "Curvature": round(curvature, 6),
    
                "R²": round(r_squared, 4),
    
                "Bottom Index": int(bottom_index),
    
                "Left Rim": round(left_rim, 2),
    
                "Latest Close": round(right_close, 2),
    
                "Recovery": round(recovery, 2)
    
            },
    
            bars=len(section),
    
            latest_close=right_close,
    
            latest_date=section.iloc[-1]["Date"] if "Date" in section.columns else None
    
        )
    
    # ======================================================
    # Detect Rounding Top
    # ======================================================
    
    def detect_rounding_top(self, df):
    
        """
        Detect Rounding Top
    
        Conditions
        ----------
        ✓ Smooth dome-shaped reversal
        ✓ Highest point near center
        ✓ Left side rising
        ✓ Right side falling
        ✓ Breakdown confirmation
        ✓ Volume confirmation
    
        Returns
        -------
        Pattern Result
        """
    
        self.validate(df)
    
        lookback = min(60, len(df))
    
        section = df.tail(lookback).reset_index(drop=True)
    
        closes = section["Close"].values
    
        if len(closes) < 25:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Top",
    
                reasons=["Insufficient candles"]
    
            )
    
        # --------------------------------------------------
        # Quadratic Curve Fit
        # --------------------------------------------------
    
        x = np.arange(len(closes))
    
        coefficients = np.polyfit(x, closes, 2)
    
        a, b, c = coefficients
    
        fitted = np.polyval(coefficients, x)
    
        # --------------------------------------------------
        # Downward Curvature Required
        # --------------------------------------------------
    
        if a >= 0:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Top",
    
                reasons=["No downward curvature"]
    
            )
    
        # --------------------------------------------------
        # Peak Near Middle
        # --------------------------------------------------
    
        peak_index = np.argmax(closes)
    
        center = len(closes) / 2
    
        if abs(peak_index - center) > len(closes) * 0.25:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Top",
    
                reasons=["Peak not centered"]
    
            )
    
        # --------------------------------------------------
        # Left Side Rising
        # --------------------------------------------------
    
        left = closes[:peak_index + 1]
    
        if len(left) < 5:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Top",
    
                reasons=["Invalid left side"]
    
            )
    
        left_slope = np.polyfit(
    
            np.arange(len(left)),
            left,
            1
    
        )[0]
    
        if left_slope <= 0:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Top",
    
                reasons=["Left side not rising"]
    
            )
    
        # --------------------------------------------------
        # Right Side Falling
        # --------------------------------------------------
    
        right = closes[peak_index:]
    
        if len(right) < 5:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Top",
    
                reasons=["Invalid right side"]
    
            )
    
        right_slope = np.polyfit(
    
            np.arange(len(right)),
            right,
            1
    
        )[0]
    
        if right_slope >= 0:
    
            return self.create_pattern_result(
    
                detected=False,
    
                pattern="Rounding Top",
    
                reasons=["Right side not falling"]
    
            )
    
        # --------------------------------------------------
        # Neckline
        # --------------------------------------------------
    
        neckline = min(
    
            closes[0],
    
            closes[-1]
    
        )
    
        latest = df.iloc[-1]
    
        close = latest["Close"]
    
        reasons = [
    
            "Rounded top",
    
            "Peak centered",
    
            "Smooth reversal"
    
        ]
    
        score = 30
    
        bearish = True
    
        bullish = False
    
        # --------------------------------------------------
        # Breakdown Confirmation
        # --------------------------------------------------
    
        if close < neckline:
    
            score += 15
    
            reasons.append("Neckline breakdown")
    
        else:
    
            reasons.append("No breakdown")
    
        # --------------------------------------------------
        # Volume Confirmation
        # --------------------------------------------------
    
        if (
    
            "Volume" in df.columns
    
            and
    
            "VOLUME_MA" in df.columns
    
        ):
    
            volume = latest["Volume"]
    
            volume_ma = latest["VOLUME_MA"]
    
            if volume >= volume_ma * self.volume_multiplier:
    
                score += 10
    
                reasons.append("High volume")
    
            else:
    
                reasons.append("Weak volume")
    
        confidence = min(score * 2, 100)
    
        return self.create_pattern_result(
    
            detected=True,
    
            pattern="Rounding Top",
    
            bullish=bullish,
    
            bearish=bearish,
    
            score=score,
    
            confidence=confidence,
    
            reasons=reasons,
    
            details={
    
                "Curvature": round(a, 6),
    
                "Peak Index": int(peak_index),
    
                "Neckline": round(neckline, 2),
    
                "Latest Close": round(close, 2)
    
            },
    
            bars=len(df),
    
            latest_close=close,
    
            latest_date=(
                df.iloc[-1]["Date"]
                if "Date" in df.columns
                else None
            )
    
        )
        
    # ==========================================================
    # ├── 12. Standard result formatter
    # ======================================================
    # Create Standard Pattern Result
    # ======================================================
    
    def create_pattern_result(
        self,
        detected=False,
        pattern=None,
        bullish=False,
        bearish=False,
        score=0,
        confidence=0,
        reasons=None,
        details=None,
        bars=None,
        latest_close=None,
        latest_date=None
    ):
        """
        Returns a standardized dictionary for every pattern.
        """
    
        if reasons is None:
            reasons = []
    
        if details is None:
            details = {}
    
        result = {
    
            "Detected": detected,
            "Pattern": pattern,
            "Bullish": bullish,
            "Bearish": bearish,
            "Score": round(float(score), 2),
            "Confidence": round(float(confidence), 2),
            "Reasons": reasons,
            "Details": details
    
        }
    
        if bars is not None:
            result["Bars"] = int(bars)
    
        if latest_close is not None:
            result["LatestClose"] = round(float(latest_close), 2)
    
        if latest_date is not None:
            result["LatestDate"] = str(latest_date)
    
        return result

    # ==========================================================
    # ├── 13. Scoring
        # ======================================================
    # Calculate Pattern Score
    # ======================================================
    
    def calculate_pattern_score(
        self,
        pattern_quality=0,
        breakout_confirmation=0,
        volume_confirmation=0,
        trend_alignment=0
    ):
        """
        Calculate weighted pattern score.
    
        Weights
        -------
        Pattern Quality        : 35%
        Breakout Confirmation  : 25%
        Volume Confirmation    : 20%
        Trend Alignment        : 20%
    
        Each parameter should be between 0 and 100.
    
        Returns
        -------
        float (0-100)
        """
    
        pattern_quality = max(0, min(pattern_quality, 100))
        breakout_confirmation = max(0, min(breakout_confirmation, 100))
        volume_confirmation = max(0, min(volume_confirmation, 100))
        trend_alignment = max(0, min(trend_alignment, 100))
    
        score = (
        
            pattern_quality *
        
            self.pattern_weights["quality"]
        
            +
        
            breakout_confirmation *
        
            self.pattern_weights["breakout"]
        
            +
        
            volume_confirmation *
        
            self.pattern_weights["volume"]
        
            +
        
            trend_alignment *
        
            self.pattern_weights["trend"]
        
        )
    
        return round(score, 2)

    # ======================================================
    # Calculate Confidence
    # ======================================================
    
    def calculate_confidence(
        self,
        geometry=0,
        volume=0,
        breakout=0,
        trend=0,
        momentum=0
    ):
        """
        Calculate confidence percentage.
    
        Components
        ----------
        Geometry : 30%
        Volume   : 20%
        Breakout : 20%
        Trend    : 15%
        Momentum : 15%
    
        Returns
        -------
        float (0-100)
        """
    
        geometry = max(0, min(geometry, 100))
        volume = max(0, min(volume, 100))
        breakout = max(0, min(breakout, 100))
        trend = max(0, min(trend, 100))
        momentum = max(0, min(momentum, 100))
    
        confidence = (
        
            geometry *
        
            self.confidence_weights["geometry"]
        
            +
        
            volume *
        
            self.confidence_weights["volume"]
        
            +
        
            breakout *
        
            self.confidence_weights["breakout"]
        
            +
        
            trend *
        
            self.confidence_weights["trend"]
        
            +
        
            momentum *
        
            self.confidence_weights["momentum"]
        
        )       
    
        return round(confidence, 2)
        
    # ==========================================================
    # ─14. MASTER DETECTOR
    # ==========================================================
    # Detect All Price Action Patterns
    # ==========================================================

    
    def detect(self, df):
    
        """
        Detect all implemented price action patterns.
    
        The master detector:
    
        1. Runs every available pattern detector.
        2. Collects every detected pattern.
        3. Calculates aggregate bullish/bearish information.
        4. Selects the strongest pattern using:
               Confidence first
               Pattern Score second
        5. Detects conflicting bullish/bearish patterns.
        6. Returns the strongest pattern as the primary pattern.
    
        Returns
        -------
        {
            "Bullish": bool,
            "Bearish": bool,
            "Market Bias": str,
            "Signal": str,
            "Pattern": str,
            "Pattern Score": float,
            "Confidence": float,
            "Reasons": [],
            "Detected Patterns": [],
            "Bullish Patterns": [],
            "Bearish Patterns": [],
            "Total Score": float,
            "Best Pattern": dict
        }
        """
    
        # ======================================================
        # Validation
        # ======================================================
    
        self.validate(df)
    
        # ======================================================
        # Storage
        # ======================================================
    
        detected_patterns = []
    
        all_reasons = []
    
        total_score = 0.0
    
        # ======================================================
        # Run All Pattern Detectors
        #
        # Using getattr() makes the master detector safer.
        # If a detector has not yet been implemented,
        # it will simply be skipped.
        # ======================================================
    
        pattern_detectors = [
    
            ("Higher High Higher Low",
             "detect_higher_high_higher_low"),
    
            ("Lower High Lower Low",
             "detect_lower_high_lower_low"),
    
            ("Breakout",
             "detect_breakout"),
    
            ("Breakdown",
             "detect_breakdown"),
    
            ("Head & Shoulders",
             "detect_head_and_shoulders"),
    
            ("Double Top",
             "detect_double_top"),
    
            ("Double Bottom",
             "detect_double_bottom"),
    
            ("Rectangle",
             "detect_rectangle"),
    
            ("Triangle",
             "detect_triangle"),
    
            ("Wedge",
             "detect_wedge"),
    
            ("Channel",
             "detect_channel"),
    
            ("Cup & Handle",
             "detect_cup_handle"),
    
            ("Pennant",
             "detect_pennant"),
    
            ("Rounding Bottom",
             "detect_rounding_bottom"),
    
            ("Rounding Top",
             "detect_rounding_top"),
    
        ]
    
        # ======================================================
        # Execute Detectors
        # ======================================================
    
        for detector_name, detector_method in pattern_detectors:
    
            detector = getattr(
                self,
                detector_method,
                None
            )
    
            # --------------------------------------------------
            # Detector not implemented
            # --------------------------------------------------
    
            if detector is None:
    
                continue
    
            # --------------------------------------------------
            # Run detector
            # --------------------------------------------------
    
            try:
    
                result = detector(df)
    
            except Exception as e:
    
                print(
                    f"[WARNING] {detector_name} "
                    f"detector failed: {e}"
                )
    
                continue
    
            # --------------------------------------------------
            # Validate result
            # --------------------------------------------------
    
            if not isinstance(result, dict):
    
                print(
                    f"[WARNING] {detector_name} "
                    f"returned invalid result."
                )
    
                continue
    
            # --------------------------------------------------
            # Only collect detected patterns
            # --------------------------------------------------
    
            if not result.get(
                "Detected",
                False
            ):
    
                continue
    
            detected_patterns.append(
                result
            )
    
            # --------------------------------------------------
            # Pattern Score
            # --------------------------------------------------
    
            pattern_score = float(
                result.get(
                    "Score",
                    result.get(
                        "Pattern Score",
                        0
                    )
                )
            )
    
            total_score += pattern_score
    
            # --------------------------------------------------
            # Reasons
            # --------------------------------------------------
    
            reasons = result.get(
                "Reasons",
                []
            )
    
            if reasons:
    
                all_reasons.extend(
                    reasons
                )
    
        # ======================================================
        # No Pattern Found
        # ======================================================
    
        if not detected_patterns:
    
            return {
    
                "Bullish": False,
    
                "Bearish": False,
    
                "Market Bias": "NEUTRAL",
    
                "Signal": "HOLD",
    
                "Pattern": "None",
    
                "Pattern Score": 0,
    
                "Confidence": 0,
    
                "Reasons": [],
    
                "Detected Patterns": [],
    
                "Bullish Patterns": [],
    
                "Bearish Patterns": [],
    
                "Total Score": 0,
    
                "Best Pattern": None
    
            }
    
        # ======================================================
        # Separate Bullish / Bearish Patterns
        # ======================================================
    
        bullish_patterns = []
    
        bearish_patterns = []
    
        neutral_patterns = []
    
        for pattern in detected_patterns:
    
            pattern_name = pattern.get(
                "Pattern",
                "Unknown"
            )
    
            is_bullish = bool(
                pattern.get(
                    "Bullish",
                    False
                )
            )
    
            is_bearish = bool(
                pattern.get(
                    "Bearish",
                    False
                )
            )
    
            if is_bullish:
    
                bullish_patterns.append(
                    pattern_name
                )
    
            if is_bearish:
    
                bearish_patterns.append(
                    pattern_name
                )
    
            if not is_bullish and not is_bearish:
    
                neutral_patterns.append(
                    pattern_name
                )
    
        # ======================================================
        # Remove Duplicate Pattern Names
        # ======================================================
    
        bullish_patterns = list(
            dict.fromkeys(
                bullish_patterns
            )
        )
    
        bearish_patterns = list(
            dict.fromkeys(
                bearish_patterns
            )
        )
    
        neutral_patterns = list(
            dict.fromkeys(
                neutral_patterns
            )
        )
    
        # ======================================================
        # Aggregate Direction
        # ======================================================
    
        any_bullish = len(
            bullish_patterns
        ) > 0
    
        any_bearish = len(
            bearish_patterns
        ) > 0
    
        # ======================================================
        # Market Bias
        # ======================================================
    
        if any_bullish and any_bearish:
    
            market_bias = "CONFLICTING"
    
        elif any_bullish:
    
            market_bias = "BULLISH"
    
        elif any_bearish:
    
            market_bias = "BEARISH"
    
        else:
    
            market_bias = "NEUTRAL"
    
        # ======================================================
        # Strongest Pattern
        #
        # Priority:
        #
        # 1. Confidence
        # 2. Pattern Score
        #
        # This is important because a pattern with a
        # high-quality geometry and confirmation should
        # beat a pattern that merely has a larger raw score.
        # ======================================================
    
        def pattern_strength(pattern):
    
            confidence = float(
                pattern.get(
                    "Confidence",
                    0
                )
            )
    
            score = float(
                pattern.get(
                    "Score",
                    pattern.get(
                        "Pattern Score",
                        0
                    )
                )
            )
    
            return (
                confidence,
                score
            )
    
        detected_patterns.sort(
            key=pattern_strength,
            reverse=True
        )
    
        best_pattern = (
            detected_patterns[0]
        )
    
        # ======================================================
        # Best Pattern Information
        # ======================================================
    
        best_pattern_name = best_pattern.get(
            "Pattern",
            "Unknown"
        )
    
        best_score = float(
            best_pattern.get(
                "Score",
                best_pattern.get(
                    "Pattern Score",
                    0
                )
            )
        )
    
        best_confidence = float(
            best_pattern.get(
                "Confidence",
                0
            )
        )
    
        best_bullish = bool(
            best_pattern.get(
                "Bullish",
                False
            )
        )
    
        best_bearish = bool(
            best_pattern.get(
                "Bearish",
                False
            )
        )
    
        # ======================================================
        # Final Trading Signal
        #
        # IMPORTANT:
        #
        # A conflicting secondary pattern should NOT
        # automatically turn the strongest pattern into
        # both BUY and SELL.
        #
        # Example:
        #
        # Rounding Bottom     100% bullish
        # Lower High Lower Low 50% bearish
        #
        # Strongest = Rounding Bottom
        #
        # But market bias = CONFLICTING
        #
        # Therefore:
        #
        # Signal = HOLD
        # ======================================================
    
        if market_bias == "CONFLICTING":
    
            signal = "HOLD"
    
        elif best_bullish and not best_bearish:
    
            signal = "BUY"
    
        elif best_bearish and not best_bullish:
    
            signal = "SELL"
    
        else:
    
            signal = "HOLD"
    
        # ======================================================
        # Final Bullish / Bearish Flags
        #
        # These flags represent the PRIMARY signal direction,
        # not merely whether some pattern somewhere was bullish
        # or bearish.
        # ======================================================
    
        if market_bias == "CONFLICTING":
    
            final_bullish = False
            final_bearish = False
    
        else:
    
            final_bullish = best_bullish
            final_bearish = best_bearish
    
        # ======================================================
        # Remove Duplicate Reasons
        # ======================================================
    
        all_reasons = list(
            dict.fromkeys(
                all_reasons
            )
        )
    
        # ======================================================
        # Add Conflict Reason
        # ======================================================
    
        if market_bias == "CONFLICTING":
    
            conflict_reason = (
                "Bullish and bearish patterns detected"
            )
    
            if conflict_reason not in all_reasons:
    
                all_reasons.append(
                    conflict_reason
                )
    
        # ======================================================
        # Add Strongest Pattern Reason
        # ======================================================
    
        strongest_reason = (
            f"Strongest pattern: "
            f"{best_pattern_name}"
        )
    
        if strongest_reason not in all_reasons:
    
            all_reasons.append(
                strongest_reason
            )
    
        # ======================================================
        # Return
        # ======================================================
    
        return {
    
            # --------------------------------------------------
            # Primary Direction
            # --------------------------------------------------
    
            "Bullish": final_bullish,
    
            "Bearish": final_bearish,
    
            # --------------------------------------------------
            # Overall Market Condition
            # --------------------------------------------------
    
            "Market Bias": market_bias,
    
            # --------------------------------------------------
            # Trading Signal
            # --------------------------------------------------
    
            "Signal": signal,
    
            # --------------------------------------------------
            # Strongest Pattern
            # --------------------------------------------------
    
            "Pattern": best_pattern_name,
    
            "Pattern Score": round(
                best_score,
                2
            ),
    
            "Confidence": round(
                best_confidence,
                2
            ),
    
            # --------------------------------------------------
            # Reasons
            # --------------------------------------------------
    
            "Reasons": all_reasons,
    
            # --------------------------------------------------
            # All Detected Patterns
            # --------------------------------------------------
    
            "Detected Patterns": [
    
                p.get(
                    "Pattern",
                    "Unknown"
                )
    
                for p in detected_patterns
    
            ],
    
            # --------------------------------------------------
            # Directional Pattern Lists
            # --------------------------------------------------
    
            "Bullish Patterns":
                bullish_patterns,
    
            "Bearish Patterns":
                bearish_patterns,
    
            "Neutral Patterns":
                neutral_patterns,
    
            # --------------------------------------------------
            # Combined Score
            # --------------------------------------------------
    
            "Total Score": round(
                total_score,
                2
            ),
    
            # --------------------------------------------------
            # Complete Best Pattern
            # --------------------------------------------------
    
            "Best Pattern":
                best_pattern
    
        }

