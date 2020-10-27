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

# Random Buy 
class strategy(AlgorithmicTrader):
    def __init__(self, trader, symbol_list, bin_interval_ms=500):
        """
        """
        # Initialize AlgorithmicTrader.
        AlgorithmicTrader.__init__(self, trader, symbol_list, bin_interval_ms=bin_interval_ms)

    def algorithm(self, df, **kwargs):
        """ 
        Random buy
        """
        
        if not len(df) : 
            return None, None 
        
        # Unpack keyword arguments.
        p = kwargs['p']  # probability that we should trigger trade 

        # in this case we do not care about the price, we just randomly trigger based on our probability param
        randomness = np.random.rand() 
        should_trade = (randomness <= p )
        
        # but we do need to know the close price to submit our order
        try : 
            row = df.iloc[-1]
        except : 
            print("ERROR random buy")
            print(df) 
            
        p_s = row['ClosePrice']

        if should_trade : 
            return 'Buy', p_s
        else:
            return None, None
        
        
        