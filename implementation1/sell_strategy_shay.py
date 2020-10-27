# Import packages.
import datetime
import json
import os
import sys
import time

import pandas as pd
import numpy as np
import redis
from pandas.core.common import SettingWithCopyWarning

import warnings
warnings.simplefilter(action="ignore", category=SettingWithCopyWarning)

# CloudEx imports.
import cloud_ex

# Import AlgorithmicTrader helper class.
from algorithmic_trader_shay import AlgorithmicTrader
from algorithmic_trader_shay import summarize_historical_trades_df


#  Sell 
class strategy(AlgorithmicTrader):
    def __init__(self, trader, symbol_list, bin_interval_ms=500):
        """
        """
        # Initialize AlgorithmicTrader.
        AlgorithmicTrader.__init__(self, trader, symbol_list, bin_interval_ms=bin_interval_ms)

    def algorithm(self, df, **kwargs):
        """ 
        Sell
        """
        # We need to know the close price to submit our order
        row = df.iloc[-1]
        p_s = row['ClosePrice']
        
        return 'Sell', p_s
        
        
        