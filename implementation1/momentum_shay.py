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


# MOMENTUM
class strategy(AlgorithmicTrader):
    def __init__(self, trader, symbol_list, bin_interval_ms=500):
        """
        Momentum Trader Class.

        :param trader(cloud_ex.Trader): CloudEx's base Trader object
        :param symbol_list: List of ticker symbols (str) to fetch data for.
        :param bin_interval_ms: Frequency with which to bin trading data in milliseconds (int).
        """
        # Initialize Trader.
        AlgorithmicTrader.__init__(self, trader, symbol_list, bin_interval_ms=bin_interval_ms)

    def algorithm(self, df, **kwargs):
        """
        Calculate buy sell points for an equity.
        A buy is triggered when the weighted rate of change for the past two time intervals is above a given threshold.
        A sell is triggered when the weighted rate of change for the past two time intervals is below a given threshold.
        For this implementation we evaluate the rate of change for stock price from open to close for each bin_interval_ms time interval.

        :param df: Dataframes with prices for an equity (pd.DataFrame).
        Momentum specific arguments:
        :param threshold: Weighted rate of change above/below which we will buy/sell (float).
        :param p1: weight for t-1 rate of change, Note: p1+p2=1 (float).
        :param p2: Weight for t-2 rate of change, Note: p1+p2=1 (float).

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
        # Handle case when data not available to get momentum.
        if len(df) < 2:
            return None, None

        # Select the last few rows from our dataframe for processing.
        df = df.iloc[-2:]
        
        # Unpack keyword arguments
        threshold = kwargs['threshold']
        p1 = kwargs['p1']
        p2 = kwargs['p2']

        # Calculate the rate of change for our equity at each timestep.
        df['Perc Change'] = (df['ClosePrice'] / df['OpenPrice'] - 1)
        row = df.iloc[-1]
        prev_row = df.iloc[-2]

        # Check if percentage change of previous row is defined.
        # This value will be undefined for the first step.
        if np.isnan(prev_row['Perc Change']):
            return None, None

        # Find the weighted average of recent moves (as a percent of stock price).
        weighted_ave_roc = p1 * row['Perc Change'] + p2 * prev_row[
            'Perc Change']
        print("Momentum value= ", weighted_ave_roc*100)
        p_s = row['ClosePrice']
        
        if weighted_ave_roc >= threshold / 100.0:
            return 'Buy', p_s
        elif weighted_ave_roc <= -threshold / 100.0:
            return 'Sell', p_s
        else:
            return None, None
