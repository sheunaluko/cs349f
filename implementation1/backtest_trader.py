import sys
import time
import pandas as pd
import datetime
import json 
import numpy as np
import os 

# MOD OF ALGORITHMIC TRADER CLASS FOR OFFLINE BACKTESTING 

class BacktestTrader:
    def __init__(self, bin_interval_ms=500):
        """
        Modified Class for BACKTEST ONLY , modified from AlgorithmicTrader Class 
        Base class for Algorithmic Traders.
        :param bin_interval_ms: Frequency with which to bin trading data in milliseconds (int).
        """
        self.bin_interval_ms = bin_interval_ms
        
        
    def change_bin_interval_ms(self, new_bin_interval_ms):
        """
        :param new_bin_interval_ms: Interval at which to bin data in milliseconds (int).
        """
        self.bin_interval_ms = new_bin_interval_ms

    def algorithm(self, df, **kwargs):
        """
        Placeholder algorithm for base class.
        :param df: The time and sales dataframe for a specific symbol (pd.DataFrame).
        """
        return None, None

    def backtest(self, symbol_historical_trades_df, num_shares, init_capital,
                 init_shares, **kwargs):
        """
        Run algorithm and place orders based on algorithm outputs.

        :param df: Stock time series dataframe to backtest on.
        :param symbol: Symbol to buy or sell (str).
        :param num_shares: Number of shares to buy or sell (int).
        :param init_capital: Amount of capital to start with (int).
        :param init_shares: Number of shares to start with (int).
        """
        #print("BACKTESTING!") 
        df = summarize_historical_trades_df(symbol_historical_trades_df,
                                            self.bin_interval_ms)
        actions_list = []
        # Submit trades.
        capital = init_capital
        shares_holding = init_shares
        init_shares_value = init_shares * df.iloc[0]['ClosePrice']
        init_net_worth = init_capital + init_shares_value

        for i in range(1,len(df)+1):
            #print(i)
            df_now = df.iloc[:i]
            # Run algorithm and determine buy/sell signal and price to execute trade.
            action, price = self.algorithm(df_now, **kwargs)

            # Skip iteration if action is None.
            if not action:
                continue

            # Place a buy order.
            if action == "Buy" and capital >= num_shares * price:
                capital -= num_shares * price
                shares_holding += num_shares
                actions_list.append((i, action, price))

            # Place a sell order.
            if action == "Sell" and shares_holding >= num_shares:
                capital += num_shares * price
                shares_holding -= num_shares
                actions_list.append((i, action, price))

        # Liquidate all assets on end of backtest to calculate ROI.
        final_net_worth = capital + shares_holding * df.iloc[-1]['ClosePrice']
        roi = (float(final_net_worth) /
               init_net_worth) * 100 - 100  # Calculate ROI as a percent.
        return roi, actions_list

# ----- Utils ------
def summarize_time_and_sales_ms(symbol_time_and_sales_df, bin_interval_ms):
    """
    Summarizes the time and sales information into a format 
    that trading algorithms can use (OHLC with a specified bin interval).

    :param symbol_time_and_sales_df: The time and sales dataframe for a specific symbol (pd.DataFrame).
    :param bin_interval_ms: Frequency with which to bin trading data in milliseconds (int).
    """
    def group_apply_func(group):
        if len(group):
            return pd.Series({
                'MinPrice': group['price'].min(),
                'MaxPrice': group['price'].max(),
                'OpenPrice': group.head(1)['price'].values[0],
                'ClosePrice': group.tail(1)['price'].values[0],
                'Volume': group['shares'].sum()
            })

    grouped = symbol_time_and_sales_df.groupby(
        pd.Grouper(key='time', freq='{}ms'.format(bin_interval_ms)))
    grouped_apply = grouped.apply(group_apply_func)
    grouped_apply.loc[:, 'Volume'] = grouped_apply['Volume'].fillna(0)
    grouped_apply.loc[:, ['MinPrice', 'MaxPrice', 'OpenPrice', 'ClosePrice'
                          ]] = grouped_apply[[
                              'MinPrice', 'MaxPrice', 'OpenPrice', 'ClosePrice'
                          ]].fillna(method='ffill')
    return grouped_apply


def summarize_historical_trades_df(symbol_historical_trades_df,
                                   bin_interval_ms):
    """
    Summarizes the symbol_historical_trades_df (OHLC with a specified bin interval).

    :param symbol_historical_trades_df: output of TradeDF (pd.DataFrame).
    :param bin_interval_ms: Frequency with which to bin trading data in milliseconds (int).
    """
    def group_apply_func(group):
        if len(group):
            return pd.Series({
                'MinPrice': group['ExecPrice'].min(),
                'MaxPrice': group['ExecPrice'].max(),
                'OpenPrice': group.head(1)['ExecPrice'].values[0],
                'ClosePrice': group.tail(1)['ExecPrice'].values[0],
                'Volume': group['SharesTraded'].sum()
            })

    symbol_historical_trades_df.loc[:, "time"] = symbol_historical_trades_df[
        "CreationTimestamp"].apply(
            lambda us_ts: datetime.datetime.fromtimestamp(us_ts * 1e-6))
    grouped = symbol_historical_trades_df.groupby(
        pd.Grouper(key='time', freq='{}ms'.format(bin_interval_ms)))
    grouped_apply = grouped.apply(group_apply_func)
    grouped_apply.loc[:, 'Volume'] = grouped_apply['Volume'].fillna(0)
    grouped_apply.loc[:, ['MinPrice', 'MaxPrice', 'OpenPrice', 'ClosePrice'
                          ]] = grouped_apply[[
                              'MinPrice', 'MaxPrice', 'OpenPrice', 'ClosePrice'
                          ]].fillna(method='ffill')
    return grouped_apply