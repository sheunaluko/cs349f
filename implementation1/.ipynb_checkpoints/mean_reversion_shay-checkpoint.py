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
from algorithmic_trader import AlgorithmicTrader
from algorithmic_trader import summarize_historical_trades_df


# MEAN REVERSION 
class strategy(AlgorithmicTrader):
    def __init__(self, trader, symbol_list, bin_interval_ms=500):
        """
        Mean Reversion Trader Class.

        :param trader(cloud_ex.Trader): CloudEx's base Trader object.
        :param symbol_list: List of ticker symbols (str) to fetch data for.
        :param bin_interval_ms: Frequency with which to bin trading data in milliseconds (int).
        """
        # Initialize AlgorithmicTrader.
        AlgorithmicTrader.__init__(self, trader, symbol_list, bin_interval_ms=bin_interval_ms)

    def algorithm(self, df, **kwargs):
        """ 
        Calculate buy sell points for an equity.
        A buy is triggered when the moving average is threshold% below the price.
        A sell is triggered when the moving average is threshold% above the price.
        For this implementation we evaluate the stock price at the close of each bin_interval_ms time interval.

        :param df: Dataframes with prices for an equity (pd.DataFrame).
        Mean reversion specific arguments:
          :param ma: Number of bins to consider in the moving average (int).
          :param threshold: Amount ma is below/above price (as a percentage) for which to buy/sell (float).

        Returns: 
            An (action, price) pair.
            
            'action' is None when no action is determined to be taken. This happens either when we do not have enough
            data to build a moving average estimate or if the price is within a small threshold above/below
            the moving average. In this case, 'price' is also None.

            'action' is 'Buy' when the price rises above the threshold defined for the moving average. 
                The buy price is set to the most recent closing price.
                
            'action' is 'Sell' when the price drops below the threshold defined for the moving average. 
                The sell price is set to the most recent closing price.
        """
        # Unpack keyword arguments.
        ma = kwargs['ma']
        threshold = kwargs['threshold']
        
        # Handle case when data not available to build moving average.
        if len(df) < ma:
            return None, None
        
        # Select the last ma rows from our dataframe for processing.
        df = df.iloc[-1*ma:]

        df['MA'] = df['ClosePrice'].rolling(ma).mean()
        row = df.iloc[-1]

        # Check if moving average price is defined.
        # This value will be undefined for the first `ma` steps.
        if np.isnan(row['MA']):
            return None, None

        buy_cutoff = (1.0 - threshold / 100.0) * row['MA']
        sell_cutoff = (1.0 + threshold / 100.0) * row['MA']
        p_s = row['ClosePrice']
        if p_s <= buy_cutoff:
            return 'Buy', p_s
        elif p_s >= sell_cutoff:
            return 'Sell', p_s
        else:
            return None, None
        
        
        