import os 
import datetime 
import pandas as pd 
import importlib 
import sys

def reload(l) : 
    importlib.reload(sys.modules[l]) 

def debug(msg) : 
    if True  : 
        print(msg) 
        
        
        
def check_for_file(fname)  : 
    import os.path
    return os.path.isfile(fname) 


def append_to_file(fname, strang) : 
    if not check_for_file(fname) : 
        mode = 'w' 
    else : 
        mode = 'a+' 

    with open(fname, mode) as outfile : 
        outfile.write(strang)
        
        
def logfile(logname, strang) : 
    fname = logname 
    #can add a log dir here if we want
    fname = "logs/" + logname 
    append_to_file(fname,strang + "\n")
    

# from course template - 

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