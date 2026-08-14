# ==========================================================
# DAILY ANALYSIS CONFIGURATION
# ==========================================================

# ----------------------------------------------------------
# EMA
# ----------------------------------------------------------

EMA_FAST = 20
EMA_MEDIUM = 50
EMA_SLOW = 200

# ----------------------------------------------------------
# RSI
# ----------------------------------------------------------

RSI_PERIOD = 14

RSI_STRONG_LEVEL = 60
RSI_BULLISH_LEVEL = 55
RSI_BEARISH_LEVEL = 45
RSI_WEAK_LEVEL = 40

# ----------------------------------------------------------
# MACD
# ----------------------------------------------------------

MACD_FAST = 12
MACD_SLOW = 26
MACD_SIGNAL = 9

# ----------------------------------------------------------
# ATR
# ----------------------------------------------------------

ATR_PERIOD = 14
ATR_MA_PERIOD = 14

# ----------------------------------------------------------
# ADX
# ----------------------------------------------------------

ADX_PERIOD = 14

ADX_STRONG_LEVEL = 30
ADX_TREND_LEVEL = 25
ADX_WEAK_LEVEL = 20

# ----------------------------------------------------------
# Bollinger Bands
# ----------------------------------------------------------

BB_PERIOD = 20
BB_STD = 2

# ----------------------------------------------------------
# Volume
# ----------------------------------------------------------

VOLUME_MA_PERIOD = 20

# ==========================================================
# SCORING
# ==========================================================

EMA_ALIGNMENT_SCORE = 30
EMA_MISALIGNMENT_SCORE = -30

PRICE_ABOVE_EMA20_SCORE = 10
PRICE_BELOW_EMA20_SCORE = -10

RSI_STRONG_SCORE = 20
RSI_BULLISH_SCORE = 10
RSI_BEARISH_SCORE = -10
RSI_WEAK_SCORE = -20

MACD_BULLISH_SCORE = 15
MACD_BEARISH_SCORE = -15

ADX_STRONG_SCORE = 15
ADX_TREND_SCORE = 10
ADX_WEAK_SCORE = -10

HIGH_VOLUME_SCORE = 5

HIGHER_HIGH_LOW_SCORE = 5
LOWER_HIGH_LOW_SCORE = -5

# ==========================================================
# SIGNAL THRESHOLDS
# ==========================================================

STRONG_BUY_SCORE = 80
BUY_SCORE = 60
WATCH_SCORE = 40

SELL_SCORE = -40
STRONG_SELL_SCORE = -80

# ==========================================================
# Trend Engine Settings
# ==========================================================


EMA_ALIGNMENT_SCORE = 20
EMA_MISALIGNMENT_SCORE = -20

PRICE_ABOVE_EMA20_SCORE = 10
PRICE_BELOW_EMA20_SCORE = -10

RSI_STRONG_SCORE = 15
RSI_BULLISH_SCORE = 8


# ==========================================================
# MACD Section
# ==========================================================
MACD_BULLISH_SCORE = 10
MACD_BEARISH_SCORE = -10

# Momentum
MACD_MOMENTUM_BULL_SCORE = 5
MACD_MOMENTUM_BEAR_SCORE = -5

# Weakening Momentum
MACD_MOMENTUM_WEAK_BULL_SCORE = -2
MACD_MOMENTUM_WEAK_BEAR_SCORE = 2

# ==========================================================
# ADX section
# ==========================================================


DI_STRONG_BULL_SCORE = 5
DI_STRONG_BEAR_SCORE = -5

DI_TREND_BULL_SCORE = 3
DI_TREND_BEAR_SCORE = -3

# ==========================================================
# Volume Section
# ==========================================================
EXCEPTIONAL_VOLUME_SCORE = 10
HIGH_VOLUME_SCORE = 5
LOW_VOLUME_SCORE = -5

EXCEPTIONAL_VOLUME_THRESHOLD = 2.0
HIGH_VOLUME_THRESHOLD = 1.5
LOW_VOLUME_THRESHOLD = 0.7


# ======================================================
# Pattern Scores Configuration
# ======================================================

# Breakout / Breakdown
BREAKOUT_SCORE = 20
BREAKDOWN_SCORE = 20

# Market Structure
HHHL_SCORE = 20
LHLL_SCORE = 20

# Double Top
DOUBLE_TOP_EQUAL_PEAKS = 30
DOUBLE_TOP_BREAKDOWN = 25
DOUBLE_TOP_VOLUME = 20
DOUBLE_TOP_WIDTH = 15

# Double Bottom
DOUBLE_BOTTOM_EQUAL_BOTTOMS = 30
DOUBLE_BOTTOM_BREAKOUT = 25
DOUBLE_BOTTOM_VOLUME = 20
DOUBLE_BOTTOM_WIDTH = 15

# ==========================================================
# Score Signal
# ==========================================================


# ==========================================================
# Score Engine V2 Configuration
# ==========================================================

TREND_WEIGHT = 35

SUPPORT_RESISTANCE_WEIGHT = 25

PRICE_ACTION_WEIGHT = 25

INDICATOR_WEIGHT = 15


# ==========================================================
# Final Signal Thresholds
# ==========================================================

BUY_THRESHOLD = 70

SELL_THRESHOLD = 30


