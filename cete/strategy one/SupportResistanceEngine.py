# ==========================================================
# Module 4 : Support & Resistance Engine 
# ==========================================================

import numpy as np
import pandas as pd


class SupportResistanceEngine:

    def __init__(self,
                 swing_window=5,
                 price_tolerance=0.005):

        self.swing_window = swing_window
        self.price_tolerance = price_tolerance

        print("Support & Resistance Engine Initialized")

    # ======================================================
    # Validate Data
    # ======================================================

    def validate(self, df):

        required = [
            "High",
            "Low",
            "Close",
            "Volume"
        ]

        for col in required:

            if col not in df.columns:

                raise ValueError(
                    f"Missing column : {col}"
                )

        if len(df) < (self.swing_window * 2 + 1):

            raise ValueError(
                "Not enough candles."
            )

    # ======================================================
    # Swing High Detection
    # ======================================================

    def detect_swing_highs(self, df):

        highs = []

        w = self.swing_window

        for i in range(w, len(df) - w):

            current_high = df.iloc[i]["High"]

            left = df.iloc[i-w:i]["High"]

            right = df.iloc[i+1:i+w+1]["High"]

            if current_high >= left.max() and \
               current_high >= right.max():

                highs.append({

                    "index": i,

                    "price": current_high,

                    "datetime": df.iloc[i]["DateTime"],

                    "volume": df.iloc[i]["Volume"]

                })

        return highs

    # ======================================================
    # Swing Low Detection
    # ======================================================

    def detect_swing_lows(self, df):

        lows = []

        w = self.swing_window

        for i in range(w, len(df) - w):

            current_low = df.iloc[i]["Low"]

            left = df.iloc[i-w:i]["Low"]

            right = df.iloc[i+1:i+w+1]["Low"]

            if current_low < left.min() and \
               current_low <= right.min():

                lows.append({

                    "index": i,

                    "price": current_low,

                    "datetime": df.iloc[i]["DateTime"],

                    "volume": df.iloc[i]["Volume"]

                })

        return lows

    # ======================================================
    # Count Touches
    # ======================================================

    def count_touches(self, df, price):

        tolerance = price * self.price_tolerance

        touches = (

            (df["High"] >= price - tolerance) &
            (df["Low"] <= price + tolerance)

        ).sum()

        return int(touches)

    # ======================================================
    # Strength Score
    # ======================================================

    def calculate_strength(self,
                           df,
                           level):

        touches = self.count_touches(
            df,
            level["price"]
        )

        latest_close = df.iloc[-1]["Close"]

        distance = abs(
            latest_close - level["price"]
        )

        distance_pct = (
            distance / latest_close
        ) * 100

        # ---------------------------------------
        # Distance Score
        # ---------------------------------------

        if distance_pct < 1:

            distance_score = 30

        elif distance_pct < 3:

            distance_score = 20

        elif distance_pct < 5:

            distance_score = 10

        else:

            distance_score = 5

        # ---------------------------------------
        # Touch Score
        # ---------------------------------------

        touch_score = min(
            touches * 10,
            50
        )

        # ---------------------------------------
        # Volume Score
        # ---------------------------------------

        avg_volume = df["Volume"].mean()

        volume_ratio = (
            level["volume"] / avg_volume
        )

        volume_score = min(
            volume_ratio * 20,
            20
        )

        strength = (

            touch_score +

            distance_score +

            volume_score

        )

        level["touches"] = touches

        level["strength"] = round(
            strength,
            2
        )

        return level

    # ======================================================
    # Merge Nearby Levels
    # ======================================================
    
    def merge_levels(self, levels):
    
        if len(levels) == 0:
            return []
    
        levels = sorted(
            levels,
            key=lambda x: x["price"]
        )
    
        merged = []
    
        current = levels[0].copy()
    
        for level in levels[1:]:
    
            tolerance = (
                current["price"] *
                self.price_tolerance
            )
    
            if abs(level["price"] - current["price"]) <= tolerance:
    
                current["price"] = round(
                    (
                        current["price"] +
                        level["price"]
                    ) / 2,
                    2
                )
    
                current["touches"] += level["touches"]
    
                current["strength"] = max(
                    current["strength"],
                    level["strength"]
                )
    
                current["volume"] = max(
                    current["volume"],
                    level["volume"]
                )
    
            else:
    
                merged.append(current)
    
                current = level.copy()
    
        merged.append(current)
    
        return merged

    # ======================================================
    # Create Price Zones
    # ======================================================
    
    def create_zones(self, levels):
    
        zones = []
    
        for level in levels:
    
            width = (
                level["price"] *
                self.price_tolerance
            )
    
            zones.append({
    
                "price": level["price"],
    
                "zone_low": round(
                    level["price"] - width,
                    2
                ),
    
                "zone_high": round(
                    level["price"] + width,
                    2
                ),
    
                "strength": level["strength"],
    
                "touches": level["touches"],
    
                "volume": level["volume"],
    
                "datetime": level["datetime"]
    
            })
    
        return zones

    # ======================================================
    # Major Support
    # ======================================================
    
    def get_major_support(
        self,
        support_zones,
        current_price
    ):
    
        candidates = [
    
            zone
    
            for zone in support_zones
    
            if zone["price"] <= current_price
    
        ]
    
        if len(candidates) == 0:
    
            return None
    
        candidates.sort(
    
            key=lambda x: (
    
                x["strength"],
    
                x["price"]
    
            ),
    
            reverse=True
    
        )
    
        return candidates[0]

    # ======================================================
    # Major Resistance
    # ======================================================
    
    def get_major_resistance(
        self,
        resistance_zones,
        current_price
    ):
    
        candidates = [
    
            zone
    
            for zone in resistance_zones
    
            if zone["price"] >= current_price
    
        ]
    
        if len(candidates) == 0:
    
            return None
    
        candidates.sort(
    
            key=lambda x: (
    
                x["strength"],
    
                -x["price"]
    
            ),
    
            reverse=True
    
        )
    
        return candidates[0]

    # ======================================================
    # Process Support & Resistance
    # ======================================================
    
    def process_levels(self, df):
    
        levels = self.calculate_levels(df)
    
        supports = self.merge_levels(
            levels["supports"]
        )
    
        resistances = self.merge_levels(
            levels["resistances"]
        )
    
        support_zones = self.create_zones(
            supports
        )
    
        resistance_zones = self.create_zones(
            resistances
        )
    
        current_price = df.iloc[-1]["Close"]
    
        major_support = self.get_major_support(
    
            support_zones,
    
            current_price
    
        )
    
        major_resistance = self.get_major_resistance(
    
            resistance_zones,
    
            current_price
    
        )
    
        return {
    
            "supports": supports,
    
            "resistances": resistances,
    
            "support_zones": support_zones,
    
            "resistance_zones": resistance_zones,
    
            "major_support": major_support,
    
            "major_resistance": major_resistance
    
        }
    # ======================================================
    # Analyze All Levels
    # ======================================================

    def calculate_levels(self, df):

        self.validate(df)

        swing_highs = self.detect_swing_highs(df)

        swing_lows = self.detect_swing_lows(df)

        resistance_levels = []

        support_levels = []

        for level in swing_highs:

            resistance_levels.append(

                self.calculate_strength(
                    df,
                    level
                )

            )

        for level in swing_lows:

            support_levels.append(

                self.calculate_strength(
                    df,
                    level
                )

            )

        resistance_levels.sort(

            key=lambda x: x["strength"],

            reverse=True

        )

        support_levels.sort(

            key=lambda x: x["strength"],

            reverse=True

        )

        return {

            "supports": support_levels,

            "resistances": resistance_levels

        }
    # ======================================================
    # Calculate Classic Pivot Points
    # ======================================================
    
    def calculate_pivot_points(self, df):
    
        """
        Calculate Classic Pivot Points using the previous day's OHLC.
    
        Returns:
            {
                "PP": ...,
                "R1": ...,
                "R2": ...,
                "R3": ...,
                "S1": ...,
                "S2": ...,
                "S3": ...
            }
        """
    
        # --------------------------------------------------
        # Validation
        # --------------------------------------------------
    
        if len(df) < 2:
    
            raise ValueError(
                "At least two candles are required "
                "to calculate Pivot Points."
            )
    
        # --------------------------------------------------
        # Previous Completed Candle
        # --------------------------------------------------
    
        previous = df.iloc[-2]
    
        high = previous["High"]
        low = previous["Low"]
        close = previous["Close"]
    
        # --------------------------------------------------
        # Pivot Point
        # --------------------------------------------------
    
        pp = (high + low + close) / 3
    
        # --------------------------------------------------
        # Resistance Levels
        # --------------------------------------------------
    
        r1 = (2 * pp) - low
    
        r2 = pp + (high - low)
    
        r3 = high + 2 * (pp - low)
    
        # --------------------------------------------------
        # Support Levels
        # --------------------------------------------------
    
        s1 = (2 * pp) - high
    
        s2 = pp - (high - low)
    
        s3 = low - 2 * (high - pp)
    
        # --------------------------------------------------
        # Return
        # --------------------------------------------------
    
        return {
    
            "PP": round(pp, 2),
    
            "R1": round(r1, 2),
            "R2": round(r2, 2),
            "R3": round(r3, 2),
    
            "S1": round(s1, 2),
            "S2": round(s2, 2),
            "S3": round(s3, 2)
    
        }

    # =====================================================
    # Calculate Level Strength
    # =====================================================

    def calculate_level_strength(
        self,
        level,
        current_price,
        atr,
        ema20,
        ema50,
        ema200,
        pivot=None
    ):

        score = 0

        # -----------------------------------------
        # 1. Touch Count (30%)
        # -----------------------------------------

        touches = level.get("touches", 0)

        score += min(touches, 10) * 3

        # -----------------------------------------
        # 2. Volume (25%)
        # -----------------------------------------

        volume = level.get("volume", 0)

        if volume > 0:

            score += 25

        # -----------------------------------------
        # 3. Freshness (15%)
        # -----------------------------------------

        age = level.get("age", 999)

        if age <= 20:

            score += 15

        elif age <= 50:

            score += 10

        elif age <= 100:

            score += 5

        # -----------------------------------------
        # 4. Pivot Confluence (15%)
        # -----------------------------------------

        if pivot is not None:

            if abs(level["price"] - pivot) <= atr:

                score += 15

        # -----------------------------------------
        # 5. EMA Confluence (15%)
        # -----------------------------------------

        ema_match = False

        for ema in [ema20, ema50, ema200]:

            if abs(level["price"] - ema) <= atr:

                ema_match = True
                break

        if ema_match:

            score += 15

        return round(score, 2) 

    # ======================================================
    # Final Support & Resistance Calculation
    # ======================================================

    def calculate(self, df):

        """
        Complete Daily Support & Resistance Analysis

        Returns:
        {
            "supports": [],
            "resistances": [],
            "support_zones": [],
            "resistance_zones": [],
            "major_support": {},
            "major_resistance": {},
            "strongest_support": {},
            "strongest_resistance": {},
            "nearest_support": {},
            "nearest_resistance": {},
            "pivot": {}
        }
        """

        # --------------------------------------------------
        # Validation
        # --------------------------------------------------

        self.validate(df)

        # --------------------------------------------------
        # Latest Values
        # --------------------------------------------------

        latest = df.iloc[-1]

        current_price = latest["Close"]
        atr = latest["ATR"]
        ema20 = latest["EMA20"]
        ema50 = latest["EMA50"]
        ema200 = latest["EMA200"]

        # --------------------------------------------------
        # Pivot Points
        # --------------------------------------------------

        pivot = self.calculate_pivot_points(df)
        pivot_price = pivot["PP"]

        # --------------------------------------------------
        # Detect Raw Levels
        # --------------------------------------------------

        levels = self.calculate_levels(df)

        # --------------------------------------------------
        # Merge Duplicate Levels
        # --------------------------------------------------

        supports = self.merge_levels(levels["supports"])

        resistances = self.merge_levels(levels["resistances"])

        # --------------------------------------------------
        # Strength Scoring
        # --------------------------------------------------

        for level in supports:

            level["strength"] = self.calculate_level_strength(
                level=level,
                current_price=current_price,
                atr=atr,
                ema20=ema20,
                ema50=ema50,
                ema200=ema200,
                pivot=pivot_price
            )

        for level in resistances:

            level["strength"] = self.calculate_level_strength(
                level=level,
                current_price=current_price,
                atr=atr,
                ema20=ema20,
                ema50=ema50,
                ema200=ema200,
                pivot=pivot_price
            )

        # --------------------------------------------------
        # Sort by Strength (Highest First)
        # --------------------------------------------------

        supports.sort(
            key=lambda x: x["strength"],
            reverse=True
        )

        resistances.sort(
            key=lambda x: x["strength"],
            reverse=True
        )

        # --------------------------------------------------
        # Strongest Levels
        # --------------------------------------------------

        strongest_support = (
            supports[0]
            if supports
            else None
        )

        strongest_resistance = (
            resistances[0]
            if resistances
            else None
        )

        # --------------------------------------------------
        # Nearest Levels
        # --------------------------------------------------

        support_candidates = [
            s
            for s in supports
            if s["price"] <= current_price
        ]

        nearest_support = (
            max(
                support_candidates,
                key=lambda x: x["price"]
            )
            if support_candidates
            else None
        )

        resistance_candidates = [
            r
            for r in resistances
            if r["price"] >= current_price
        ]

        nearest_resistance = (
            min(
                resistance_candidates,
                key=lambda x: x["price"]
            )
            if resistance_candidates
            else None
        )

        # --------------------------------------------------
        # Create Zones
        # --------------------------------------------------

        support_zones = self.create_zones(supports)

        resistance_zones = self.create_zones(resistances)

        # --------------------------------------------------
        # Major Zones
        # --------------------------------------------------

        major_support = self.get_major_support(
            support_zones,
            current_price
        )

        major_resistance = self.get_major_resistance(
            resistance_zones,
            current_price
        )

        # --------------------------------------------------
        # Final Result
        # --------------------------------------------------

        return {

            "supports": supports,

            "resistances": resistances,

            "support_zones": support_zones,

            "resistance_zones": resistance_zones,

            "major_support": major_support,

            "major_resistance": major_resistance,

            "strongest_support": strongest_support,

            "strongest_resistance": strongest_resistance,

            "nearest_support": nearest_support,

            "nearest_resistance": nearest_resistance,

            "pivot": pivot
        }
