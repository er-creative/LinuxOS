# ==========================================================
# Module 6 : Score Engine V2
# ==========================================================
import pandas as pd
from config import (
    TREND_WEIGHT,
    SUPPORT_RESISTANCE_WEIGHT,
    PRICE_ACTION_WEIGHT,
    INDICATOR_WEIGHT,
    BUY_THRESHOLD,
    SELL_THRESHOLD
)

class ScoreEngine:

    # ======================================================
    # Constructor
    # ======================================================

    def __init__(
        self,

        trend_weight=TREND_WEIGHT,

        support_resistance_weight=SUPPORT_RESISTANCE_WEIGHT,

        price_action_weight=PRICE_ACTION_WEIGHT,

        indicator_weight=INDICATOR_WEIGHT,

        buy_threshold=BUY_THRESHOLD,

        sell_threshold=SELL_THRESHOLD
    ):

        # ==================================================
        # Module Weights
        # ==================================================

        self.trend_weight = float(
            trend_weight
        )

        self.support_resistance_weight = float(
            support_resistance_weight
        )

        self.price_action_weight = float(
            price_action_weight
        )

        self.indicator_weight = float(
            indicator_weight
        )

        # ==================================================
        # Thresholds
        # ==================================================

        self.buy_threshold = float(
            buy_threshold
        )

        self.sell_threshold = float(
            sell_threshold
        )

        # ==================================================
        # Validate Weights
        # ==================================================

        total_weight = (

            self.trend_weight

            + self.support_resistance_weight

            + self.price_action_weight

            + self.indicator_weight

        )

        if abs(total_weight - 100) > 0.001:

            raise ValueError(

                "Score Engine weights must total 100."

                f"\nCurrent Total = {total_weight}"

            )

        # ==================================================
        # Print Initialization
        # ==================================================

        print(
            "Score Engine V2 Initialized"
        )


    # ======================================================
    # Utility
    # Clamp Value
    # ======================================================

    @staticmethod
    def clamp(
        value,
        minimum,
        maximum
    ):

        try:

            value = float(value)

        except (
            TypeError,
            ValueError
        ):

            value = 0.0

        return max(
            minimum,
            min(value, maximum)
        )


    # ======================================================
    # Utility
    # Safe Float
    # ======================================================

    @staticmethod
    def safe_float(
        value,
        default=0.0
    ):

        try:

            value = float(value)

            if pd.isna(value):

                return default

            return value

        except (
            TypeError,
            ValueError
        ):

            return default


    # ======================================================
    # Validate Inputs
    # ======================================================

    def validate_inputs(
        self,
        df=None,
        sr_result=None,
        trend_result=None,
        indicator_result=None,
        pattern_result=None
    ):

        if df is None:

            raise ValueError(
                "df cannot be None."
            )

        if len(df) == 0:

            raise ValueError(
                "DataFrame is empty."
            )

        if sr_result is not None:

            if not isinstance(
                sr_result,
                dict
            ):

                raise ValueError(
                    "Invalid Support/Resistance result."
                )

        if trend_result is not None:

            if not isinstance(
                trend_result,
                dict
            ):

                raise ValueError(
                    "Invalid Trend result."
                )

        if indicator_result is not None:

            if not isinstance(
                indicator_result,
                dict
            ):

                raise ValueError(
                    "Invalid Indicator result."
                )

        if pattern_result is not None:

            if not isinstance(
                pattern_result,
                dict
            ):

                raise ValueError(
                    "Invalid Pattern result."
                )

        return True


    # ======================================================
    # Create Standard Score Result
    # ======================================================

    def create_score_result(

        self,

        score=0,

        confidence=0,

        signal="HOLD",

        bullish=False,

        bearish=False,

        reasons=None,

        details=None

    ):

        if reasons is None:

            reasons = []

        if details is None:

            details = {}

        return {

            "Score": round(
                float(score),
                2
            ),

            "Confidence": round(
                float(confidence),
                2
            ),

            "Signal": signal,

            "Bullish": bool(
                bullish
            ),

            "Bearish": bool(
                bearish
            ),

            "Reasons": list(
                dict.fromkeys(
                    reasons
                )
            ),

            "Details": details

        }


    # ==========================================================
    # Part 6B
    # Calculate Indicator Score V2
    # ==========================================================

    def calculate_indicator_score(
        self,
        df
    ):

        self.validate_inputs(
            df=df
        )

        latest = df.iloc[-1]

        score = 0.0

        reasons = []

        bullish_points = 0

        bearish_points = 0

        # ==================================================
        # EMA Alignment
        # ==================================================

        ema20 = self.safe_float(
            latest.get("EMA20")
        )

        ema50 = self.safe_float(
            latest.get("EMA50")
        )

        ema200 = self.safe_float(
            latest.get("EMA200")
        )

        if (
            ema20 > 0
            and ema50 > 0
            and ema200 > 0
        ):

            if (
                ema20 >
                ema50 >
                ema200
            ):

                score += 25

                bullish_points += 1

                reasons.append(
                    "EMA20 > EMA50 > EMA200"
                )

            elif (
                ema20 <
                ema50 <
                ema200
            ):

                score -= 25

                bearish_points += 1

                reasons.append(
                    "EMA20 < EMA50 < EMA200"
                )

            elif ema20 > ema50:

                score += 10

                bullish_points += 1

                reasons.append(
                    "EMA20 above EMA50"
                )

            elif ema20 < ema50:

                score -= 10

                bearish_points += 1

                reasons.append(
                    "EMA20 below EMA50"
                )


        # ==================================================
        # RSI
        # ==================================================

        rsi = self.safe_float(
            latest.get("RSI"),
            50
        )

        if 50 <= rsi <= 70:

            score += 12

            bullish_points += 1

            reasons.append(
                "RSI bullish zone"
            )

        elif 30 <= rsi < 50:

            score -= 8

            bearish_points += 1

            reasons.append(
                "RSI below 50"
            )

        elif rsi > 70:

            score -= 4

            reasons.append(
                "RSI overbought"
            )

        elif rsi < 30:

            score += 5

            bullish_points += 1

            reasons.append(
                "RSI oversold"
            )


        # ==================================================
        # MACD
        # ==================================================

        macd = self.safe_float(
            latest.get("MACD")
        )

        macd_signal = self.safe_float(
            latest.get("MACD_SIGNAL")
        )

        macd_hist = self.safe_float(
            latest.get("MACD_HIST")
        )

        if macd > macd_signal:

            score += 12

            bullish_points += 1

            reasons.append(
                "MACD above signal"
            )

        elif macd < macd_signal:

            score -= 12

            bearish_points += 1

            reasons.append(
                "MACD below signal"
            )


        if macd_hist > 0:

            score += 6

            bullish_points += 1

            reasons.append(
                "Positive MACD histogram"
            )

        elif macd_hist < 0:

            score -= 6

            bearish_points += 1

            reasons.append(
                "Negative MACD histogram"
            )


        # ==================================================
        # ADX / DI
        # ==================================================

        adx = self.safe_float(
            latest.get("ADX")
        )

        plus_di = self.safe_float(
            latest.get("+DI")
        )

        minus_di = self.safe_float(
            latest.get("-DI")
        )

        if adx >= 25:

            if plus_di > minus_di:

                score += 10

                bullish_points += 1

                reasons.append(
                    "Strong bullish directional trend"
                )

            elif minus_di > plus_di:

                score -= 10

                bearish_points += 1

                reasons.append(
                    "Strong bearish directional trend"
                )

        else:

            reasons.append(
                "ADX indicates weak trend"
            )


        # ==================================================
        # Price vs EMA20
        # ==================================================

        close = self.safe_float(
            latest.get("Close")
        )

        if ema20 > 0:

            if close > ema20:

                score += 8

                bullish_points += 1

                reasons.append(
                    "Price above EMA20"
                )

            elif close < ema20:

                score -= 8

                bearish_points += 1

                reasons.append(
                    "Price below EMA20"
                )


        # ==================================================
        # Bollinger Bands
        # ==================================================

        bb_upper = self.safe_float(
            latest.get("BB_UPPER")
        )

        bb_lower = self.safe_float(
            latest.get("BB_LOWER")
        )

        if (
            bb_upper > 0
            and bb_lower > 0
        ):

            if close > bb_upper:

                score -= 3

                reasons.append(
                    "Price above upper Bollinger Band"
                )

            elif close < bb_lower:

                score += 3

                reasons.append(
                    "Price below lower Bollinger Band"
                )

            else:

                reasons.append(
                    "Price inside Bollinger Bands"
                )


        # ==================================================
        # Volume
        # ==================================================

        high_volume = latest.get(
            "HIGH_VOLUME",
            False
        )

        if bool(high_volume):

            if (
                bullish_points >
                bearish_points
            ):

                score += 5

                reasons.append(
                    "High volume supports bullish momentum"
                )

            elif (
                bearish_points >
                bullish_points
            ):

                score -= 5

                reasons.append(
                    "High volume supports bearish momentum"
                )


        # ==================================================
        # ATR
        # ==================================================

        atr = self.safe_float(
            latest.get("ATR")
        )

        atr_ma = self.safe_float(
            latest.get("ATR_MA")
        )

        if (
            atr > 0
            and atr_ma > 0
        ):

            if atr > atr_ma:

                reasons.append(
                    "ATR above average"
                )

            else:

                reasons.append(
                    "ATR below average"
                )


        # ==================================================
        # Normalize Score
        # ==================================================

        score = self.clamp(
            score,
            -100,
            100
        )

        # ==================================================
        # Direction
        # ==================================================

        bullish = score > 5

        bearish = score < -5

        if bullish and not bearish:

            signal = "BUY"

        elif bearish and not bullish:

            signal = "SELL"

        else:

            signal = "HOLD"


        # ==================================================
        # Indicator Confidence
        # ==================================================

        total_directional_votes = (
            bullish_points
            + bearish_points
        )

        if total_directional_votes > 0:

            directional_agreement = (

                max(
                    bullish_points,
                    bearish_points
                )

                / total_directional_votes

            ) * 100

        else:

            directional_agreement = 0


        confidence = (

            abs(score) * 0.70

            + directional_agreement * 0.30

        )

        confidence = self.clamp(
            confidence,
            0,
            100
        )


        # ==================================================
        # Details
        # ==================================================

        details = {

            "Bullish Votes":
                bullish_points,

            "Bearish Votes":
                bearish_points,

            "Directional Agreement":
                round(
                    directional_agreement,
                    2
                )

        }


        return self.create_score_result(

            score=score,

            confidence=confidence,

            signal=signal,

            bullish=bullish,

            bearish=bearish,

            reasons=reasons,

            details=details

        )


    # ==========================================================
    # Part 6C
    # Calculate Trend Score V2
    # ==========================================================

    def calculate_trend_score(
        self,
        trend_result
    ):

        if trend_result is None:

            raise ValueError(
                "Trend result cannot be None."
            )

        raw_score = self.safe_float(
            trend_result.get(
                "Score",
                0
            )
        )

        trend = str(
            trend_result.get(
                "Trend",
                ""
            )
        ).strip().lower()

        signal = str(
            trend_result.get(
                "Signal",
                ""
            )
        ).strip().upper()

        confidence = self.clamp(
            self.safe_float(
                trend_result.get(
                    "Confidence",
                    0
                )
            ),
            0,
            100
        )

        reasons = list(
            trend_result.get(
                "Reasons",
                []
            )
        )

        # ==================================================
        # Normalize Existing Trend Score
        # ==================================================

        score = self.clamp(
            raw_score,
            -100,
            100
        )

        # ==================================================
        # Trend Direction
        # ==================================================

        if trend == "bullish":

            score = max(
                score,
                50
            )

            bullish = True

            bearish = False

            reasons.append(
                "Bullish Trend"
            )

        elif trend == "bearish":

            score = min(
                score,
                -50
            )

            bullish = False

            bearish = True

            reasons.append(
                "Bearish Trend"
            )

        else:

            bullish = False

            bearish = False

            reasons.append(
                "Sideways / Neutral Trend"
            )


        # ==================================================
        # Signal Agreement
        # ==================================================

        if (
            signal == "BUY"
            and bullish
        ):

            score = min(
                score + 10,
                100
            )

            reasons.append(
                "Trend BUY signal confirmed"
            )

        elif (
            signal == "SELL"
            and bearish
        ):

            score = max(
                score - 10,
                -100
            )

            reasons.append(
                "Trend SELL signal confirmed"
            )


        # ==================================================
        # Final Signal
        # ==================================================

        if bullish and not bearish:

            final_signal = "BUY"

        elif bearish and not bullish:

            final_signal = "SELL"

        else:

            final_signal = "HOLD"


        return self.create_score_result(

            score=score,

            confidence=confidence,

            signal=final_signal,

            bullish=bullish,

            bearish=bearish,

            reasons=reasons

        )


    # ==========================================================
    # Part 6D
    # Calculate Support / Resistance Score V2
    # ==========================================================

    def calculate_support_resistance_score(
        self,
        df,
        sr_result
    ):

        self.validate_inputs(

            df=df,

            sr_result=sr_result

        )

        latest = df.iloc[-1]

        close = self.safe_float(
            latest.get("Close")
        )

        score = 0.0

        bullish = False

        bearish = False

        reasons = []

        details = {}


        # ==================================================
        # Major Support
        # ==================================================

        support = sr_result.get(
            "major_support"
        )

        if support is not None:

            support_price = self.safe_float(
                support.get("price")
            )

            zone_low = self.safe_float(
                support.get("zone_low")
            )

            zone_high = self.safe_float(
                support.get("zone_high")
            )

            details[
                "Major Support"
            ] = support_price


            if (
                zone_low <= close <= zone_high
            ):

                score += 30

                bullish = True

                reasons.append(
                    "Price inside Major Support Zone"
                )

            elif close > zone_high:

                if support_price > 0:

                    distance = (

                        (close - support_price)

                        / support_price

                    ) * 100

                else:

                    distance = 100


                details[
                    "Support Distance %"
                ] = round(
                    distance,
                    2
                )


                if distance <= 2:

                    score += 25

                    bullish = True

                    reasons.append(
                        "Price near Major Support"
                    )

                elif distance <= 5:

                    score += 15

                    bullish = True

                    reasons.append(
                        "Price above Support"
                    )

                elif distance <= 10:

                    score += 5

                    bullish = True

                    reasons.append(
                        "Support below price"
                    )

            else:

                score -= 30

                bearish = True

                reasons.append(
                    "Price below Major Support"
                )


        # ==================================================
        # Major Resistance
        # ==================================================

        resistance = sr_result.get(
            "major_resistance"
        )

        if resistance is not None:

            resistance_price = self.safe_float(
                resistance.get("price")
            )

            zone_low = self.safe_float(
                resistance.get("zone_low")
            )

            zone_high = self.safe_float(
                resistance.get("zone_high")
            )

            details[
                "Major Resistance"
            ] = resistance_price


            if (
                zone_low <= close <= zone_high
            ):

                score -= 30

                bearish = True

                reasons.append(
                    "Price inside Major Resistance Zone"
                )

            elif close < zone_low:

                if resistance_price > 0:

                    distance = (

                        (resistance_price - close)

                        / resistance_price

                    ) * 100

                else:

                    distance = 100


                details[
                    "Resistance Distance %"
                ] = round(
                    distance,
                    2
                )


                if distance <= 2:

                    score -= 25

                    bearish = True

                    reasons.append(
                        "Price near Major Resistance"
                    )

                elif distance <= 5:

                    score -= 15

                    bearish = True

                    reasons.append(
                        "Price below Resistance"
                    )

                elif distance <= 10:

                    score -= 5

                    bearish = True

                    reasons.append(
                        "Resistance above price"
                    )

            else:

                score += 30

                bullish = True

                reasons.append(
                    "Price broke above Major Resistance"
                )


        # ==================================================
        # Pivot
        # ==================================================

        pivot = sr_result.get(
            "pivot"
        )

        if pivot:

            pp = self.safe_float(
                pivot.get("PP")
            )

            details[
                "Pivot"
            ] = pp

            buffer = pp * 0.005


            if close > pp + buffer:

                score += 10

                bullish = True

                reasons.append(
                    "Price above Pivot"
                )

            elif close < pp - buffer:

                score -= 10

                bearish = True

                reasons.append(
                    "Price below Pivot"
                )

            else:

                reasons.append(
                    "Price near Pivot"
                )


        # ==================================================
        # Clamp
        # ==================================================

        score = self.clamp(
            score,
            -100,
            100
        )


        # ==================================================
        # Signal
        # ==================================================

        if bullish and not bearish:

            signal = "BUY"

        elif bearish and not bullish:

            signal = "SELL"

        else:

            signal = "HOLD"


        # ==================================================
        # Confidence
        # ==================================================

        confidence = min(
            abs(score),
            100
        )


        return self.create_score_result(

            score=score,

            confidence=confidence,

            signal=signal,

            bullish=bullish,

            bearish=bearish,

            reasons=reasons,

            details=details

        )


    # ==========================================================
    # Part 6E
    # Calculate Price Action Pattern Score V2
    # ==========================================================

    def calculate_pattern_score(
        self,
        pattern_result
    ):

        if pattern_result is None:

            raise ValueError(
                "Pattern result cannot be None."
            )


        detected = pattern_result.get(
            "Detected Patterns",
            []
        )

        if not detected:

            return self.create_score_result(

                score=0,

                confidence=0,

                signal="HOLD",

                reasons=[
                    "No Price Action Pattern"
                ],

                details={}

            )


        # ==================================================
        # Strongest Pattern
        # ==================================================

        pattern = pattern_result.get(
            "Pattern",
            "None"
        )

        pattern_score = self.clamp(

            self.safe_float(
                pattern_result.get(
                    "Pattern Score",
                    0
                )
            ),

            0,

            100

        )

        pattern_confidence = self.clamp(

            self.safe_float(
                pattern_result.get(
                    "Confidence",
                    0
                )
            ),

            0,

            100

        )


        bullish_patterns = pattern_result.get(
            "Bullish Patterns",
            []
        )

        bearish_patterns = pattern_result.get(
            "Bearish Patterns",
            []
        )


        bullish = len(
            bullish_patterns
        ) > 0

        bearish = len(
            bearish_patterns
        ) > 0


        reasons = list(
            pattern_result.get(
                "Reasons",
                []
            )
        )


        details = {

            "Best Pattern":
                pattern,

            "Detected Patterns":
                detected,

            "Bullish Patterns":
                bullish_patterns,

            "Bearish Patterns":
                bearish_patterns,

            "Raw Pattern Score":
                pattern_score,

            "Pattern Confidence":
                pattern_confidence

        }


        # ==================================================
        # Direction of Strongest Pattern
        # ==================================================

        best_bullish = (
            pattern_result.get(
                "Best Pattern",
                {}
            ).get(
                "Bullish",
                False
            )
        )

        best_bearish = (
            pattern_result.get(
                "Best Pattern",
                {}
            ).get(
                "Bearish",
                False
            )
        )


        # ==================================================
        # Convert 0-100 Pattern Score
        # to -100/+100 Directional Score
        # ==================================================

        if (
            best_bullish
            and not best_bearish
        ):

            score = pattern_score

            signal = "BUY"

        elif (
            best_bearish
            and not best_bullish
        ):

            score = -pattern_score

            signal = "SELL"

        else:

            # ---------------------------------------------
            # If master pattern result is conflicting,
            # calculate directional balance.
            # ---------------------------------------------

            bullish_strength = 0

            bearish_strength = 0


            if bullish_patterns:

                bullish_strength = pattern_score


            if bearish_patterns:

                bearish_strength = pattern_score


            if (
                bullish_strength >
                bearish_strength
            ):

                score = bullish_strength

                signal = "BUY"

            elif (
                bearish_strength >
                bullish_strength
            ):

                score = -bearish_strength

                signal = "SELL"

            else:

                score = 0

                signal = "HOLD"


        # ==================================================
        # Conflict Handling
        # ==================================================

        conflict = (

            bullish
            and bearish

        )


        if conflict:

            score *= 0.70

            confidence = (
                pattern_confidence
                * 0.70
            )

            reasons.append(
                "Bullish and bearish patterns conflict"
            )

            details[
                "Conflict"
            ] = True

        else:

            confidence = (
                pattern_confidence
            )

            details[
                "Conflict"
            ] = False


        # ==================================================
        # Final Direction
        # ==================================================

        bullish = score > 5

        bearish = score < -5


        if bullish and not bearish:

            signal = "BUY"

        elif bearish and not bullish:

            signal = "SELL"

        else:

            signal = "HOLD"


        # ==================================================
        # Store Signed Score
        # ==================================================

        details[
            "Signed Pattern Score"
        ] = round(
            score,
            2
        )


        return self.create_score_result(

            score=score,

            confidence=confidence,

            signal=signal,

            bullish=bullish,

            bearish=bearish,

            reasons=reasons,

            details=details

        )


    # ==========================================================
    # Part 6F
    # Calculate Final Weighted Score V2
    # ==========================================================

    def calculate_total_score(

        self,

        indicator_score,

        trend_score,

        sr_score,

        pattern_score

    ):

        if indicator_score is None:

            raise ValueError(
                "Indicator score is missing."
            )

        if trend_score is None:

            raise ValueError(
                "Trend score is missing."
            )

        if sr_score is None:

            raise ValueError(
                "Support/Resistance score is missing."
            )

        if pattern_score is None:

            raise ValueError(
                "Pattern score is missing."
            )


        # ==================================================
        # Weighted Module Scores
        # ==================================================

        indicator_weighted = (

            indicator_score["Score"]

            * self.indicator_weight

            / 100

        )


        trend_weighted = (

            trend_score["Score"]

            * self.trend_weight

            / 100

        )


        sr_weighted = (

            sr_score["Score"]

            * self.support_resistance_weight

            / 100

        )


        pattern_weighted = (

            pattern_score["Score"]

            * self.price_action_weight

            / 100

        )


        # ==================================================
        # Final Score
        # Range: -100 to +100
        # ==================================================

        total_score = (

            indicator_weighted

            + trend_weighted

            + sr_weighted

            + pattern_weighted

        )


        total_score = self.clamp(

            total_score,

            -100,

            100

        )


        # ==================================================
        # Weighted Confidence
        # ==================================================

        confidence = (

            indicator_score["Confidence"]
            * self.indicator_weight

            +

            trend_score["Confidence"]
            * self.trend_weight

            +

            sr_score["Confidence"]
            * self.support_resistance_weight

            +

            pattern_score["Confidence"]
            * self.price_action_weight

        ) / 100


        confidence = self.clamp(

            confidence,

            0,

            100

        )


        # ==================================================
        # Directional Votes
        # ==================================================

        bullish_votes = sum([

            bool(
                indicator_score["Bullish"]
            ),

            bool(
                trend_score["Bullish"]
            ),

            bool(
                sr_score["Bullish"]
            ),

            bool(
                pattern_score["Bullish"]
            )

        ])


        bearish_votes = sum([

            bool(
                indicator_score["Bearish"]
            ),

            bool(
                trend_score["Bearish"]
            ),

            bool(
                sr_score["Bearish"]
            ),

            bool(
                pattern_score["Bearish"]
            )

        ])


        # ==================================================
        # Conflict Detection
        # ==================================================

        directional_conflict = (

            bullish_votes > 0

            and

            bearish_votes > 0

        )


        # ==================================================
        # Final Signal
        # ==================================================

        if (

            total_score >= self.buy_threshold

            and bullish_votes >= 3

            and not directional_conflict

        ):

            signal = "BUY"


        elif (

            total_score <= self.sell_threshold

            and bearish_votes >= 3

            and not directional_conflict

        ):

            signal = "SELL"


        else:

            signal = "HOLD"


        # ==================================================
        # Final Direction
        # ==================================================

        if signal == "BUY":

            bullish = True

            bearish = False

        elif signal == "SELL":

            bullish = False

            bearish = True

        else:

            bullish = False

            bearish = False


        # ==================================================
        # Reasons
        # ==================================================

        reasons = []

        reasons.extend(
            indicator_score["Reasons"]
        )

        reasons.extend(
            trend_score["Reasons"]
        )

        reasons.extend(
            sr_score["Reasons"]
        )

        reasons.extend(
            pattern_score["Reasons"]
        )


        reasons = list(
            dict.fromkeys(
                reasons
            )
        )


        if directional_conflict:

            reasons.append(
                "Directional conflict between scoring modules"
            )


        if signal == "BUY":

            reasons.append(
                "Final BUY conditions satisfied"
            )

        elif signal == "SELL":

            reasons.append(
                "Final SELL conditions satisfied"
            )

        else:

            reasons.append(
                "Final BUY/SELL conditions not satisfied"
            )


        # ==================================================
        # Details
        # ==================================================

        details = {

            "Indicator Score":
                round(
                    indicator_score["Score"],
                    2
                ),

            "Trend Score":
                round(
                    trend_score["Score"],
                    2
                ),

            "Support/Resistance Score":
                round(
                    sr_score["Score"],
                    2
                ),

            "Pattern Score":
                round(
                    pattern_score["Score"],
                    2
                ),

            "Weighted Indicator":
                round(
                    indicator_weighted,
                    2
                ),

            "Weighted Trend":
                round(
                    trend_weighted,
                    2
                ),

            "Weighted Support":
                round(
                    sr_weighted,
                    2
                ),

            "Weighted Pattern":
                round(
                    pattern_weighted,
                    2
                ),

            "Bullish Votes":
                bullish_votes,

            "Bearish Votes":
                bearish_votes,

            "Directional Conflict":
                directional_conflict,

            "Buy Threshold":
                self.buy_threshold,

            "Sell Threshold":
                self.sell_threshold

        }


        return self.create_score_result(

            score=total_score,

            confidence=confidence,

            signal=signal,

            bullish=bullish,

            bearish=bearish,

            reasons=reasons,

            details=details

        )


    # ==========================================================
    # Part 6G
    # Main Score Engine
    # ==========================================================

    def calculate(

        self,

        df,

        trend_result,

        sr_result,

        pattern_result=None

    ):

        """
        Main Score Engine V2.

        Pipeline:

        Data
          ↓
        Indicator Score
          ↓
        Trend Score
          ↓
        Support / Resistance Score
          ↓
        Price Action Pattern Score
          ↓
        Weighted Final Score
          ↓
        BUY / SELL / HOLD
        """

        # ==================================================
        # Validation
        # ==================================================

        self.validate_inputs(

            df=df,

            trend_result=trend_result,

            sr_result=sr_result,

            pattern_result=pattern_result

        )


        # ==================================================
        # Indicator
        # ==================================================

        indicator_score = (

            self.calculate_indicator_score(
                df
            )

        )


        # ==================================================
        # Trend
        # ==================================================

        trend_score = (

            self.calculate_trend_score(
                trend_result
            )

        )


        # ==================================================
        # Support / Resistance
        # ==================================================

        sr_score = (

            self.calculate_support_resistance_score(

                df,

                sr_result

            )

        )


        # ==================================================
        # Price Action Pattern
        # ==================================================

        if pattern_result is None:

            pattern_score = (

                self.create_score_result(

                    score=0,

                    confidence=0,

                    signal="HOLD",

                    bullish=False,

                    bearish=False,

                    reasons=[
                        "Pattern module not available"
                    ]

                )

            )

        else:

            pattern_score = (

                self.calculate_pattern_score(

                    pattern_result

                )

            )


        # ==================================================
        # Final Weighted Score
        # ==================================================

        total_score = (

            self.calculate_total_score(

                indicator_score,

                trend_score,

                sr_score,

                pattern_score

            )

        )


        # ==================================================
        # Attach Module Results
        # ==================================================

        total_score[
            "Indicator Score"
        ] = indicator_score


        total_score[
            "Trend Score"
        ] = trend_score


        total_score[
            "Support Resistance Score"
        ] = sr_score


        total_score[
            "Pattern Score"
        ] = pattern_score


        # ==================================================
        # Module Weight Information
        # ==================================================

        total_score[
            "Weights"
        ] = {

            "Trend":
                self.trend_weight,

            "Support Resistance":
                self.support_resistance_weight,

            "Price Action":
                self.price_action_weight,

            "Indicator":
                self.indicator_weight

        }


        return total_score
