import sys
import time
import pandas as pd
import datetime
import json 

sys.path.insert(1, '/root/CloudExchange/bazel-bin/python/')
import cloud_ex


import utilities as u 

#changes --> 

#submits market order by default


class AlgorithmicTrader:
    def __init__(self, trader, symbol_list, bin_interval_ms=500):
        """
        Base class for Algorithmic Traders.

        :param trader(cloud_ex.Trader): CloudEx's base Trader object
        :param symbol_list: List of ticker symbols (str) to fetch data for.
        :param bin_interval_ms: Frequency with which to bin trading data in milliseconds (int).
        """
        self.bin_interval_ms = bin_interval_ms
        # Create utilities to place orders.
        self.trader = trader
        self.symbol_list = self.trader.GetSymbols() if self.trader else []
        self.set_active_symbols(symbol_list)
        self.last_seen_lob_timestamp_us_dict = {}
        self.last_seen_trade_timestamp_us_dict = {}
        self.time_and_sales_dict = {}
        self.summarized_time_and_sales_dict = {}
        for symbol in self.symbol_list:
            self.last_seen_lob_timestamp_us_dict[symbol] = 1
            self.last_seen_trade_timestamp_us_dict[symbol] = 1
            self.time_and_sales_dict[symbol] = pd.DataFrame()
            self.summarized_time_and_sales_dict[symbol] = pd.DataFrame()
            
    def set_id(self,id) : 
        self.trader_id = id

    def set_active_symbols(self, symbol_list):
        """
        Set the list of active symbols we will track.

        :param symbol_list: List of ticker symbols (str) to fetch data for.
        """
        if not self.trader: 
            print("Running in offline mode. Could not set active symbols.")
            return

        self.trader.ConfigActiveSymbols(symbol_list)
        self.active_symbol_list = symbol_list

    def change_bin_interval_ms(self, new_bin_interval_ms):
        """
        Updates the time and sales information for each symbol.

        :param new_bin_interval_ms: Interval at which to bin data in milliseconds (int).
        """
        self.bin_interval_ms = new_bin_interval_ms
        for symbol in self.symbol_list:
            self._update_time_and_sales(symbol)

    def algorithm(self, df, **kwargs):
        """
        Placeholder algorithm for base class.

        :param df: The time and sales dataframe for a specific symbol (pd.DataFrame).
        """
        return None, None

    def place_order(self, symbol, price, num_shares, buy=True):
        """
        Place an order.

        :param symbol: Symbol to buy or sell (str).
        :param price: Price at which to execute the order (int).
        :param num_shares: Number of shares to buy or sell (int).
        :param buy: Whether to buy or sell (bool).
        """
        if not self.trader: 
            print("Running in offline mode. Could not set place order.")
            return

        returned_order_ = cloud_ex.Order()

        type_ = cloud_ex.OrderType.market
        action_ = cloud_ex.OrderAction.buy if buy else cloud_ex.OrderAction.sell

        # Submit order and wait
        result = self.trader.SubmitOrder(symbol, returned_order_, type_,
                                         action_, num_shares, int(price))
        if result != cloud_ex.OrderResult.in_sequencer:
            return None
        return returned_order_.order_id_

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
        df = summarize_historical_trades_df(symbol_historical_trades_df,
                                            self.bin_interval_ms)
        actions_list = []
        # Submit trades.
        capital = init_capital
        shares_holding = init_shares
        init_shares_value = init_shares * df.iloc[0]['ClosePrice']
        init_net_worth = init_capital + init_shares_value

        for i in range(len(df)):
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

    def trade(self, symbol, num_shares, max_num_orders, wait_interval,
              **kwargs):
        """
        Run algorithm and place orders based on algorithm outputs.

        :param symbol: Symbol to buy or sell (str).
        :param num_shares: Number of shares to buy or sell (int).
        :param max_num_orders: Number of algorithm iterations, equates to the maximum orders we can place (int).
        :param wait_interval: Time to wait between each algorithm iteration (float).
        """
        # Submit trades.
        submitted_order_ids = []
        for _ in range(max_num_orders):
            # Get latest trades for that symbol
            self._update_time_and_sales(symbol)
            # Run algorithm and determine buy/sell signal and price to execute trade.
            action, price = self.algorithm(
                self.summarized_time_and_sales_dict[symbol], **kwargs)
            buy = True if action == 'Buy' else False

            # Place order.
            if price is not None and action is not None:
                
                logmsg = "[{}] Placing trade, symbol={}, price={}, num_shares={}, buy={}".format(self.trader_id,
                                                                                                 symbol,
                                                                                                 price, 
                                                                                                 num_shares,
                                                                                                 buy) 
                #u.logfile("main", logmsg) #-- actually is IO intensive! 
                order_id = self.place_order(symbol, price, num_shares, buy=buy)
                submitted_order_ids.append(order_id)
            else:
                submitted_order_ids.append(None)
                #u.logfile("main","Did not submit an order in this iteration.")

            # Wait for an interval before placing next order (optional).
            time.sleep(wait_interval)

      
        return submitted_order_ids

    def _update_time_and_sales(self, target_symbol):
        """
        Pull new trades and update the time and sales for a specific symbol.

        :param target_symbol: Symbol to buy or sell (str).
        """
        if not self.trader: 
            print("Running in offline mode. Could not update time and sales data.")
            return

        latest_trades = cloud_ex.VectorTrade()
        confirmed_trade_temp_ = cloud_ex.Trade()

        self.trader.GetRecentTrades(
            target_symbol, latest_trades,
            self.last_seen_trade_timestamp_us_dict[target_symbol])
        if len(latest_trades) > 0:
            print("{} new trades".format(len(latest_trades)))
            self.last_seen_trade_timestamp_us_dict[
                target_symbol] = latest_trades[0].creation_timestamp_
            new_trades = []
            for i in range(len(latest_trades)):
                confirmed_trade_temp_ = latest_trades[-i - 1]
                # Add time and sales data to DataFrame of all trades.
                trade_ts = {
                    'symbol':
                    confirmed_trade_temp_.symbol_,
                    'price':
                    confirmed_trade_temp_.exec_price_,
                    'time':
                    datetime.datetime.fromtimestamp(
                        confirmed_trade_temp_.creation_timestamp_ * 1e-6),
                    'shares':
                    confirmed_trade_temp_.shares_traded_
                }
                new_trades.append(trade_ts)
            self.time_and_sales_dict[target_symbol] = self.time_and_sales_dict[
                target_symbol].append(new_trades, ignore_index=True)
            self.summarized_time_and_sales_dict[
                target_symbol] = summarize_time_and_sales_ms(
                    self.time_and_sales_dict[target_symbol],
                    self.bin_interval_ms)
        else:
            print("no new trades")


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