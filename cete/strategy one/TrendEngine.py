# ==========================================================
# Module 3 : Trend Engine
# ==========================================================
from config import *

class TrendEngine:

    def __init__(self):

        print("Trend Engine Initialized")

    # ------------------------------------------------------
    # Detect Trend
    # ------------------------------------------------------

    def detect(self, df):

        latest = df.iloc[-1]

        score = 0


        reasons = []

        # ==========================================
        # Confidence Counters
        # ==========================================
    
        bullish_checks = 0
    
        bearish_checks = 0
        # ==================================================
        # EMA
        # ==================================================

        if latest["EMA20"] > latest["EMA50"] > latest["EMA200"]:

            score += EMA_ALIGNMENT_SCORE

            reasons.append("EMA20 > EMA50 > EMA200")
            
            bullish_checks += 1


        elif latest["EMA20"] < latest["EMA50"] < latest["EMA200"]:

            score += EMA_MISALIGNMENT_SCORE

            reasons.append("EMA20 < EMA50 < EMA200")
            
            bearish_checks += 1

        # ==================================================
        # Price Above EMA20
        # ==================================================

        if latest["Close"] > latest["EMA20"]:

            score += PRICE_ABOVE_EMA20_SCORE

            reasons.append("Close Above EMA20")
            bullish_checks += 1

        else:

            score += PRICE_BELOW_EMA20_SCORE

            reasons.append("Close Below EMA20")
            bearish_checks += 1

        # ==================================================
        # RSI
        # ==================================================

        if latest["RSI"] >= RSI_STRONG_LEVEL:

            score += RSI_STRONG_SCORE

            reasons.append("RSI Strong")
            bullish_checks += 1


        elif latest["RSI"] >= RSI_BULLISH_LEVEL:

            score += RSI_BULLISH_SCORE

            reasons.append("RSI Bullish")
            bullish_checks += 1


        elif latest["RSI"] <= RSI_WEAK_LEVEL:

            score += RSI_WEAK_SCORE

            reasons.append("RSI Weak")
            bearish_checks += 1


        elif latest["RSI"] <= RSI_BEARISH_LEVEL:

            score += RSI_BEARISH_SCORE

            reasons.append("RSI Bearish")
            bearish_checks += 1

        # ==================================================
        # MACD
        # ==================================================
        
        hist = latest["MACD_HIST"]
        prev_hist = df.iloc[-2]["MACD_HIST"]
        
        # ------------------------------
        # Bullish Histogram
        # ------------------------------
        
        if hist > 0:
        
            score += MACD_BULLISH_SCORE
            reasons.append("Positive MACD Histogram")
        
            bullish_checks += 1
        
            # MACD Momentum Increasing
            if hist > prev_hist:
        
                score += MACD_MOMENTUM_BULL_SCORE
                reasons.append("MACD Momentum Increasing")
        
                bullish_checks += 1
        
            # MACD Momentum Weakening
            elif hist < prev_hist:
        
                score += MACD_MOMENTUM_WEAK_BULL_SCORE
                reasons.append("MACD Bullish Momentum and consider as neutral")
                        
        # ------------------------------
        # Bearish Histogram
        # ------------------------------
        
        elif hist < 0:
        
            score += MACD_BEARISH_SCORE
            reasons.append("Negative MACD Histogram")
        
            bearish_checks += 1
        
            # Bearish Momentum Increasing
            if hist < prev_hist:
        
                score += MACD_MOMENTUM_BEAR_SCORE
                reasons.append("MACD Bearish Momentum Increasing")
        
                bearish_checks += 1
        
            # Bearish Momentum Weakening
            elif hist > prev_hist:
        
                score += MACD_MOMENTUM_WEAK_BEAR_SCORE
                reasons.append("MACD Bearish Momentum Weakening")
        
                bullish_checks += 1
        
        # ------------------------------
        # Flat Histogram
        # ------------------------------
        
        else:
        
            reasons.append("MACD Histogram Neutral")

        # ==================================================
        # ADX
        # ==================================================

        adx = latest["ADX"]
        plus_di = latest["+DI"]
        minus_di = latest["-DI"]

        if adx >= ADX_STRONG_LEVEL:

            score += ADX_STRONG_SCORE
            reasons.append("Strong Trend")
            bullish_checks += 1          # <-- Missing


            if plus_di > minus_di:

                score += DI_STRONG_BULL_SCORE
                reasons.append("+DI Above -DI")

                bullish_checks += 1

            elif minus_di > plus_di:

                score += DI_STRONG_BEAR_SCORE
                reasons.append("-DI Above +DI")

                bearish_checks += 1

        elif adx >= ADX_TREND_LEVEL:

            score += ADX_TREND_SCORE
            reasons.append("Trending Market")

            if plus_di > minus_di:

                score += DI_TREND_BULL_SCORE
                reasons.append("+DI Above -DI")

                bullish_checks += 1

            elif minus_di > plus_di:

                score += DI_TREND_BEAR_SCORE
                reasons.append("-DI Above +DI")

                bearish_checks += 1

        else:

            score += ADX_WEAK_SCORE
            reasons.append("Weak/Sideways Market")
                    


        # ==================================================
        # Volume
        # ==================================================
        
        rvol = latest["RVOL"]
        
        if rvol >= EXCEPTIONAL_VOLUME_THRESHOLD:
        
            score += EXCEPTIONAL_VOLUME_SCORE
            reasons.append("Exceptional Volume")
            bullish_checks += 1

        
        elif rvol >= HIGH_VOLUME_THRESHOLD:
        
            score += HIGH_VOLUME_SCORE
            reasons.append("High Volume")
            bullish_checks += 1

        
        elif rvol <= LOW_VOLUME_THRESHOLD:
        
            score += LOW_VOLUME_SCORE
            reasons.append("Low Volume")
            bearish_checks += 1
        else:
        
            reasons.append("Normal Volume")
        
        # ==================================================
        # Market Structure
        # ==================================================
        
        # Market structure (HH, HL, LH, LL, BOS, CHoCH)
        # is evaluated by PatternEngine using confirmed
        # swing points. It is intentionally omitted here
        # to avoid duplicate scoring.

        # ==================================================
        # Trend
        # ==================================================

        if score >= STRONG_BUY_SCORE:

            trend = "Strong Bullish"

        elif score >= BUY_SCORE:

            trend = "Bullish"

        elif score <= STRONG_SELL_SCORE:

            trend = "Strong Bearish"

        elif score <= SELL_SCORE:

            trend = "Bearish"

        else:

            trend = "Neutral"

        # ==================================================
        # Signal
        # ==================================================

        if score >= STRONG_BUY_SCORE:

            signal = "STRONG BUY"

        elif score >= BUY_SCORE:

            signal = "BUY"

        elif score >= WATCH_SCORE:

            signal = "WATCH"

        elif score <= STRONG_SELL_SCORE:

            signal = "STRONG SELL"

        elif score <= SELL_SCORE:

            signal = "SELL"

        else:

            signal = "WAIT"

        # ==================================================
        # Allowed Trades
        # ==================================================

        if signal in ["BUY", "STRONG BUY"]:

            allowed_trades = "LONG ONLY"

        elif signal in ["SELL", "STRONG SELL"]:

            allowed_trades = "SHORT ONLY"

        else:

            allowed_trades = "NO TRADE"

        # ==================================================
        # Confidence
        # ==================================================

        total_checks = (
            bullish_checks +
            bearish_checks
        )

        if total_checks == 0:

            confidence = 0

        else:

            confidence = round(
                (
                    max(
                        bullish_checks,
                        bearish_checks
                    )
                    /
                    total_checks
                ) * 100,
                2
            )

        # ==================================================
        # Return Result
        # ==================================================

        return {

            "Trend": trend,

            "Signal": signal,

            "Score": score,

            "Confidence": confidence,

            "Allowed_Trades": allowed_trades,

            "Bullish": bullish_checks,

            "Bearish": bearish_checks,

            "Reasons": reasons
        }
